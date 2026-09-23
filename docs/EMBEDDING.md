# Embedding ecl-client in other projects

There are four ways for a host project to write logbook entries. All of them
use the same native implementation.

| Route | Use when | Dependency |
|---|---|---|
| C++ / C library | Compiled daemons, DAQ services, Qt applications | CMake `ECL::ecl` |
| Python module | Python tools, Flask/Django/Litestar services, notebooks | `fnal-ecl-client` pip package |
| `ecl-post` CLI | Shell scripts, cron, CI, any language with `exec` | Binary on `PATH` |
| `ecl-track` CLI | Multi-step procedures that need a linked record | `ecl-post` plus Python 3.9 |

Every route below was verified on 2026-09-23 against this tree: a
`find_package` consumer, a `FetchContent` consumer, and
`pip install git+...@v0.0.2` in a clean virtual environment.

## CMake: installed package

```sh
cmake -S ecl-client -B ecl-client/build -DCMAKE_BUILD_TYPE=Release \
      -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"      # macOS only
cmake --build ecl-client/build -j
cmake --install ecl-client/build --prefix "$HOME/.local"
```

```cmake
find_package(ecl-client CONFIG REQUIRED)       # add -DCMAKE_PREFIX_PATH=$HOME/.local
target_link_libraries(my_daemon PRIVATE ECL::ecl)
```

The exported config pulls in the CURL and OpenSSL dependencies. With a static
`libecl.a` the consumer must be able to find those too, so pass the same
`OPENSSL_ROOT_DIR` on macOS.

## CMake: FetchContent (no install step)

```cmake
include(FetchContent)
set(ECL_BUILD_TESTS OFF CACHE BOOL "" FORCE)   # don't add ecl tests to the host's ctest
set(ECL_BUILD_CLI   OFF CACHE BOOL "" FORCE)   # library only
FetchContent_Declare(ecl_client
    GIT_REPOSITORY https://github.com/normanajn/ecl-client.git
    GIT_TAG        v0.0.2)                      # always pin a tag
FetchContent_MakeAvailable(ecl_client)
target_link_libraries(my_daemon PRIVATE ECL::ecl)
```

## Python

`pyproject.toml` of the host project:

```toml
dependencies = [
    "fnal-ecl-client @ git+https://github.com/normanajn/ecl-client.git@v0.0.2",
]
```

Or in `requirements.txt`:

```text
fnal-ecl-client @ git+https://github.com/normanajn/ecl-client.git@v0.0.2
```

This builds the extension from source, so libcurl and OpenSSL headers must be
present. On macOS, run `export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"`
before `pip install`. For development against a local checkout, use
`pip install -e ~/Git-Repositories/Norman/ecl-client`.

## Configuration in a host project

Keep ECL settings in the host project's YAML config, and use the standard
`ECL_*` variables as overrides so that `ecl-post`, `ecl-track` and the library
all agree. Precedence is command line, then environment, then `.env`, then
`config/*.yaml`:

```yaml
# config/app.yaml
ecl:
  enabled: true              # master switch: false turns every post into a no-op
  instance: mu2e             # -> ECL_INSTANCE
  category: Operations/DAQ   # -> ECL_CATEGORY
  username: daq-robot        # -> ECL_USERNAME
  auth: xml                  # -> ECL_AUTH
  timeout: 15                # seconds
  # The password is never stored here. Use ECL_PASSWORD, or the mode-0600
  # ~/.config/ecl-client/config file read by ecl-post.
```

## Failure policy: fail soft

A logbook is a record of the operation, not part of it. **An ECL outage, an
expired password or a mistyped category must never abort a run or a
procedure.** Wrap every post, log failures locally, and carry on. Use strict
mode only where the log entry is itself the required deliverable, such as a
signed-off shift check.

### Python helper

```python
import logging
import os
from typing import Optional

import ecl_client as ecl

log = logging.getLogger(__name__)


def ecl_post(subject: str, text: str, *, category: Optional[str] = None,
             attachments: tuple = (), related: Optional[int] = None,
             cfg: Optional[dict] = None) -> Optional[int]:
    """Post one entry. Returns the entry ID, or None if disabled or failed."""
    cfg = cfg or {}
    if not cfg.get("enabled", True):
        return None
    try:
        entry = ecl.Entry(category or os.environ.get("ECL_CATEGORY") or cfg["category"])
        entry.subject(subject).text(text)
        if related:
            entry.related(related)
        for path in attachments:
            entry.attachment(os.path.basename(path), path)
        auth = os.environ.get("ECL_AUTH") or cfg.get("auth", "xml")
        client = ecl.Client(
            ecl.instance_url(os.environ.get("ECL_INSTANCE") or cfg["instance"]),
            os.environ.get("ECL_USERNAME") or cfg["username"],
            os.environ["ECL_PASSWORD"],
            ecl.AuthMode.PASSWORD if auth == "password" else ecl.AuthMode.XML_SIGNATURE,
        ).timeout(int(cfg.get("timeout", 15)))
        return client.post(entry).entry_id
    except (ecl.ECLError, KeyError, OSError) as error:
        log.warning("ECL post failed (%s): %s", subject, error)
        return None
```

`Client.post` releases the GIL, so calling it from a daemon thread doesn't
stall the rest of the process. In Qt applications, post from a worker thread
and report the entry ID back through a signal. Don't touch widgets from the
worker.

### C++ helper

```cpp
#include <ecl/ecl.hpp>
#include <cstdlib>
#include <iostream>
#include <optional>

std::optional<std::int64_t> ecl_post(const std::string &category,
                                     const std::string &subject,
                                     const std::string &text) noexcept {
    try {
        const char *user = std::getenv("ECL_USERNAME");
        const char *pass = std::getenv("ECL_PASSWORD");
        const char *inst = std::getenv("ECL_INSTANCE");
        if (!user || !pass || !inst) return std::nullopt;
        ecl::Entry entry(category);
        entry.subject(subject).text(text);
        ecl::Client client(ecl::instance_url(inst), user, pass);
        client.timeout(15);
        return client.post(entry).entry_id;
    } catch (const std::exception &e) {
        std::cerr << "ECL post failed: " << e.what() << '\n';
        return std::nullopt;
    }
}
```

`ecl::Client` is not thread safe. Use one per thread, or serialise access to
it; separate clients can post concurrently. Keep a post off any real-time
path: a post can take up to the configured timeout (default 30 s).

### Shell

```sh
ecl-post -c Operations/DAQ -s "Run $RUN started" -t "$SUMMARY" \
  || echo "WARN: ECL post failed; continuing" >&2
```

## Recording procedures

Use `ecl-track` for anything with more than one step (upgrades, calibration
campaigns, recoveries). It gives a START entry, one entry per step linked to
the start through `related`, and an END summary listing each step's entry ID.
Failed posts are queued on disk and can be replayed with `ecl-track flush`.
See `ecl-track(1)`.

```sh
OP=tracker-hv-ramp-$(date +%F)
ecl-track start --op "$OP" --title 'Tracker HV ramp to 1450 V' -i mu2e -c Operations/Tracker
ecl-track step  --op "$OP" --name 'Plane 0-17 ramped' --attachment ramp.csv
ecl-track end   --op "$OP" --status ok
```

From Python, call it with `subprocess.run(["ecl-track", "step", "--op", op, "--name", name, ...])`.
The state file holds the shared context, so separate processes can contribute
steps to one operation.
