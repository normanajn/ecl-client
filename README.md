# ecl-client

`ecl-client` posts entries to Fermilab Electronic Logbook (ECL) instances from
the command line, C, C++, or Python. All interfaces use the same native protocol
implementation.

For complete posting examples—including text, files, and images for every
interface—see [EXAMPLES.md](EXAMPLES.md).

The client implements the XML posting API provided by ECL 8.x:

```text
POST <instance>/E/xml_post?salt=<random>
```

It supports entry subjects, category paths, tags, custom forms and fields,
related and private entries, ordinary file attachments, and image attachments.

## Requirements

- CMake 3.20 or newer
- A C++17 compiler
- libcurl
- OpenSSL 1.1.1 or newer
- Python 3.9 or newer, pybind11, and scikit-build-core for Python builds

Linux and macOS are supported. Windows support is not currently provided.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /desired/prefix
```

To build a shared library, add `-DBUILD_SHARED_LIBS=ON`.

Install the Python package in an isolated environment with:

```sh
python3 -m venv .venv
.venv/bin/pip install .
.venv/bin/python -c 'import ecl_client; print(ecl_client.__version__)'
```

The Python build downloads its declared build dependencies if they are not
already available. It still requires system libcurl and OpenSSL development
files.

## ECL account setup

Ask the administrator of the target ECL instance for an active XML/API account.
An ECL XML account stores a raw API password and cannot be used for interactive
web login. This is the recommended authentication mode.

Two modes are supported:

- `xml` (default): sends the username and a legacy ECL-compatible MD5 digest of
  the query string, raw API password, and exact XML body. The password is not
  transmitted.
- `password`: sends the username and password in ECL headers. This mode is
  allowed only over HTTPS and is intended for ECL accounts with a regular local
  password. Fermilab SSO credentials are not automatically interchangeable with
  an ECL local password.

MD5 is used only because it is mandated by the existing ECL XML-account
protocol. TLS verification remains enabled by default and should be used in
production.

## Command line

The shortest post using an instance alias is:

```sh
export ECL_USERNAME=automation-account
export ECL_PASSWORD='API password'

ecl-post \
  --instance mu2e \
  --category 'Operations/DAQ' \
  --subject 'Automated status' \
  --text-file status.txt \
  --tag automated \
  --attachment report=report.json
```

`--instance mu2e` expands to
`https://dbweb0.fnal.gov/ECL/mu2e`. A full URL can be supplied with `--url`.
The current aliases can be found in the
[Fermilab ECL instance directory](https://dbweb0.fnal.gov/redirector/ecl_instances.html).

Read an entry body from standard input with `--text-file -`:

```sh
generate-report | ecl-post --instance demo --category Sandbox \
  --subject 'Generated report' --text-file -
```

Inspect the exact XML without credentials or network access:

```sh
ecl-post --dry-run --category Sandbox --text 'Test entry'
```

Run `ecl-post --help` for all options. Passwords are deliberately not accepted
as command-line options, where they could be exposed through shell history or
process listings.

### Configuration

The default configuration file is `$XDG_CONFIG_HOME/ecl-client/config`, or
`$HOME/.config/ecl-client/config` when `XDG_CONFIG_HOME` is unset:

```ini
instance=mu2e
username=automation-account
auth=xml
category=Operations/DAQ
form=default
# password=optional-api-password
```

If a configuration file contains `password`, it must have mode `0600` or more
restrictive:

```sh
chmod 600 ~/.config/ecl-client/config
```

The supported environment variables are `ECL_URL`, `ECL_INSTANCE`,
`ECL_USERNAME`, `ECL_PASSWORD`, `ECL_AUTH`, `ECL_CATEGORY`, and `ECL_FORM`.
Precedence is command-line options, environment variables, configuration, then
built-in defaults. If no password is otherwise available, `ecl-post` prompts on
the controlling terminal with echo disabled.

## C++ interface

```cpp
#include <ecl/ecl.hpp>

ecl::Entry entry("Sandbox");
entry.subject("C++ post")
     .text("Beam state is stable")
     .format(ecl::TextFormat::Plain)
     .tag("automated")
     .field("state", "stable")
     .attachment("report", "report.txt");

ecl::Client client(ecl::instance_url("demo"), "api-user", password,
                   ecl::AuthMode::XmlSignature);
ecl::PostResult result = client.post(entry);
```

An installed CMake consumer can use:

```cmake
find_package(ecl-client CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE ECL::ecl)
```

## C interface

```c
#include <ecl/ecl.h>

ecl_entry *entry = ecl_entry_create("Sandbox");
ecl_entry_set_subject(entry, "C post");
ecl_entry_set_text(entry, "Entry body");
ecl_entry_add_tag(entry, "automated");

ecl_client *client = ecl_client_create(
    "https://dbweb0.fnal.gov/ECL/demo", "api-user", password,
    ECL_AUTH_XML_SIGNATURE);
ecl_post_result result = {0};
ecl_status status = ecl_client_post(client, entry, &result);
if (status == ECL_OK) {
    printf("Created entry %lld\n", (long long)result.entry_id);
}

ecl_post_result_free(&result);
ecl_client_destroy(client);
ecl_entry_destroy(entry);
```

The caller owns returned `ecl_entry`, `ecl_client`, and response objects and must
release them with the corresponding functions. `ecl_entry_to_xml` buffers are
released with `ecl_free`.

## Python interface

```python
import os
import ecl_client as ecl

entry = (
    ecl.Entry("Sandbox")
    .subject("Python post")
    .text("Automated entry")
    .format(ecl.TextFormat.PLAIN)
    .tag("automated")
    .attachment("report", "report.txt")
)

client = ecl.Client(
    ecl.instance_url("demo"),
    "api-user",
    os.environ["ECL_PASSWORD"],
    ecl.AuthMode.XML_SIGNATURE,
)
result = client.post(entry)
print(result.entry_id, result.effective_url)
```

`Client.post` releases the Python GIL during the network operation.

## Redirect and TLS behavior

The public Fermilab instance URLs currently return a `303` redirect to an active
backend. Standard HTTP behavior changes a redirected POST into a GET, which
would discard the entry. This client explicitly retains the POST method and the
exact signed body across ECL redirects. It follows at most five redirects and
recomputes the signature if a redirect changes the query string.

To prevent credentials from being forwarded to an unrelated service,
cross-host redirects are accepted only when both the original and target hosts
are under `fnal.gov`; otherwise the redirect must remain on the original host.

Password authentication permits only HTTPS requests and HTTPS redirects. XML
signature mode also permits HTTP to support local test servers, but production
ECL URLs should always use HTTPS. TLS certificates and hostnames are verified by
default; `--insecure` is intended only for controlled diagnostics.

## Testing safely

The automated suite never posts to Fermilab. It runs against a local mock server
that verifies XML, binary attachments, signature calculation, HTTP errors, and
POST preservation through a 303 redirect. Use `--dry-run` before an intentional
live test and choose a designated sandbox category.

## License

BSD 3-Clause. See [LICENSE](LICENSE).
