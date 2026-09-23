# Changelog

Tags follow `vMAJOR.MINOR.PATCH`. From v1.0.0 the tag, `CMakeLists.txt`
`project(VERSION)` (compiled into `ecl_version()`, `ecl-post --version` and the
User-Agent), `pyproject.toml`, `ecl-track --version` and the man pages all agree.

## v1.0.0 (2026-09-23)

First production release.

- Verified live: Mu2e entry 5559 and NOvA entry 256674 (password auth,
  category `Sandbox`), posted through the alias redirect to both the
  `dbweb-a` and `dbweb-b` backends.
- Version aligned to 1.0.0 everywhere. The library version now comes from
  CMake (`ECL_VERSION_STRING`) instead of literals in `src/ecl.cpp`. The shared
  library SOVERSION is now 1.
- Python classifier set to Production/Stable.
- Tests: the mock-server and ecl-track suites now run with an isolated
  `HOME`/`XDG_CONFIG_HOME` and no `ECL_*` variables, so a developer's real
  `~/.config/ecl-client/config` can neither break them nor leak credentials to
  the mock server. The Python binding test now loads the built `_native` module
  before any installed `ecl_client`, so CTest no longer tests a stale install.

- Add `ecl-track`: records multi-step operations as linked ECL entries (START,
  steps related to START, END summary), with on-disk queueing and `flush` for
  posts that fail. Tested against a mock server (`ecl-track-tests` in CTest).
- Add man pages `ecl-post(1)`, `ecl-track(1)`, `ecl(3)`, `ecl_client(3)` and
  install them with `cmake --install`.
- Add `docs/LIVE_TESTING.md` (Mu2e and NOvA live test procedure) and
  `docs/EMBEDDING.md` (CMake, pip, shell, fail-soft patterns).
- Add Claude Code skills under `.claude/skills/` (`ecl-logbook`,
  `ecl-track-steps`, `ecl-embed`).
- README: Homebrew OpenSSL (`brew --prefix openssl@3`, `--fresh`), CMake
  Python-module build, options table, NOvA alias.
- Document ECL's generic HTML `400 Bad Request` for disabled or missing
  categories.
- `ecl-logbook` skill: the credential check shows config values only when they
  match the expected shape, and redacts anything else.

## v0.0.2 (2026-09-23)

First tagged state (commit `fcd8fe1`, identical to the monorepo's `b048bee`).

- C++17 library with C ABI, header-only C++ wrapper, `ecl-post` CLI and
  pybind11 Python module.
- ECL 8.x `xml_post`: subjects, categories, tags, forms and fields, related and
  private entries, file and image attachments.
- XML-signature (MD5) and HTTPS password authentication.
- POST preserved across up to five 303 redirects; cross-host redirects only
  within `fnal.gov`.
- Tests: C++ unit, C API, CLI against a mock server, Python binding. All pass on
  macOS arm64 (OpenSSL 3.6.4, Python 3.12.1).
