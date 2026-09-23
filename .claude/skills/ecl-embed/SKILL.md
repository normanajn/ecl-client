---
name: ecl-embed
description: Add Fermilab ECL logbook posting to another project's code — C++/C via CMake (find_package or FetchContent of ECL::ecl), Python via the fnal-ecl-client pip package, or shell via ecl-post/ecl-track — with YAML/env configuration and a fail-soft wrapper so logbook outages never break the host system. Use when the user wants a program, daemon, DAQ tool, web app, script or CI job to write ECL/elog entries automatically, or asks how to depend on or link against ecl-client.
---

The canonical reference, with verified snippets, is
`$ECL_HOME/docs/EMBEDDING.md` (`ECL_HOME` defaults to
`~/Git-Repositories/Norman/ecl-client`). Read it before writing integration
code, and copy from it rather than improvising. It covers CMake
`find_package`, `FetchContent`, pip/requirements pins, the YAML config block,
the Python and C++ fail-soft helpers, and threading notes.

## Choose the route

| Host project | Route | Dependency line |
|---|---|---|
| CMake C++/C | Library | `FetchContent` `GIT_TAG v0.0.2` (pinned), or `find_package(ecl-client CONFIG REQUIRED)` |
| Python package or app | Module | `fnal-ecl-client @ git+https://github.com/normanajn/ecl-client.git@v0.0.2` |
| Shell, cron, CI, other languages | CLI | `ecl-post` on PATH; `ecl-track` for procedures |

Match the host project's language (C++ or Python per the user's standards).
Pin a tag, never `main`. Check the current tags with
`git ls-remote --tags https://github.com/normanajn/ecl-client.git`.

## Rules for the integration code

1. **Fail soft.** Wrap every post, catch `ecl_client.ECLError` or
   `ecl::Error`, log a warning and return `None` or `std::nullopt`. A logbook
   failure must never change the host's control flow, exit code or real-time
   behaviour. Make it strict only where the user says the entry is itself
   the deliverable.
2. **Configurable, with a master switch.** Add an `ecl:` block to the host's
   `config/*.yaml` with `enabled`, `instance`, `category`, `username`, `auth`
   and `timeout`. Standard `ECL_*` environment variables override it, and CLI
   flags override those. Precedence: CLI > env > `.env` > YAML > defaults.
   Tests and development default to `enabled: false` or a sandbox category.
3. **No password in config or code.** Read it only from `ECL_PASSWORD` (set
   by the deployment or secret store) or rely on `ecl-post`'s mode-0600
   `~/.config/ecl-client/config`. Production services should use an ECL
   XML/API account (`auth: xml`), which cannot log in to the web UI.
4. **Off the hot path.** A post can block for up to the timeout (default 30 s;
   use 10-15 s). In daemons, post from a daemon thread or queue. In Qt apps,
   post from a worker and report back with a signal. Never post from a
   real-time readout loop. `ecl::Client` is not thread safe, so use one per
   thread. The Python `post` releases the GIL.
5. **Useful entries.** Post at state transitions (run start/stop,
   configuration change, alarm raised/cleared, deployment), not per event.
   Include the identifiers an operator will search for (run number, host,
   version, config hash). Rate-limit anything that could loop.
6. **Testable without Fermilab.** Unit-test with the client disabled or against
   a local mock. `$ECL_HOME/tests/test_ecl_track.py` has a 30-line mock ECL
   server that returns `Created <id>`, which you can copy. Never let a test
   suite post live.
7. **Document it** in the host project's README and man page: config keys, env
   vars, which events post entries, and how to turn it off.

## Multi-step procedures from code

Shell out to `ecl-track` (see the ecl-track-steps skill). It supplies the
linked START/step/END thread and the on-disk retry queue for free:

```python
subprocess.run(["ecl-track", "step", "--op", op, "--name", name, "--status", status,
                "-T", "-"], input=body, text=True, check=False)
```

## Verify before handing over

- It builds with the dependency pinned (`cmake --build`, or `pip install` in a
  clean venv; on macOS set `OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"`).
- A test shows that `enabled: false` produces no post and that an unreachable
  ECL is logged without raising.
- One dry run (`ecl-post -n` with the same category and subject) shows the
  XML the host would send.
- Any live test post follows the ecl-logbook skill: dry run, user
  confirmation, sandbox category.
