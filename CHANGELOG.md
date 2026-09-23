# Changelog

Tags follow `vMAJOR.MINOR.PATCH`. The version compiled into the binaries
(`ecl_version()`, `ecl-post --version`, `pyproject.toml`) is still `0.1.0`, and
has not yet been aligned with the tag series.

## Unreleased

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
