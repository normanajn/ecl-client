# Installing ecl-client and the Claude Code skills on a new machine

This sets up `ecl-post`, `ecl-track` and the three Claude Code skills
(`ecl-logbook`, `ecl-track-steps`, `ecl-embed`) so that any Claude Code session
on the machine can make logbook entries. Supported on Linux and macOS; Windows
is not supported yet.

## 1. Build dependencies

| Platform | Command |
|---|---|
| macOS | `brew install cmake openssl@3` (plus Xcode Command Line Tools) |
| Alma/RHEL/Fedora | `sudo dnf install -y git cmake gcc-c++ libcurl-devel openssl-devel python3` |
| Debian/Ubuntu | `sudo apt install -y git cmake g++ libcurl4-openssl-dev libssl-dev python3-venv` |

## 2. Clone, build, install

The skills look for the checkout at `~/Git-Repositories/Norman/ecl-client`
by default:

```sh
git clone --branch v1.0.0 https://github.com/normanajn/ecl-client.git ~/Git-Repositories/Norman/ecl-client
cd ~/Git-Repositories/Norman/ecl-client
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# macOS only: add  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"  to the line above
cmake --build build -j
ctest --test-dir build --output-on-failure        # expect 4/4 (the Python module test is off by default)
cmake --install build --prefix ~/.local           # ecl-post, ecl-track, man pages
```

Make sure `~/.local/bin` is on `PATH`
(`export PATH="$HOME/.local/bin:$PATH"` in `~/.zshrc` or `~/.bashrc`). If you
clone somewhere else, add `export ECL_HOME=/path/to/ecl-client` to your shell
profile instead. Claude Code's shell reads your profile, so the skills will
pick it up.

## 3. Link the skills

```sh
mkdir -p ~/.claude/skills
for s in ecl-logbook ecl-track-steps ecl-embed; do
  ln -sfn ~/Git-Repositories/Norman/ecl-client/.claude/skills/$s ~/.claude/skills/$s
done
```

With symlinks, a `git pull` in the checkout updates the skills too. Use
`cp -R` instead for a fixed copy.

## 4. Credentials

Create the files mode 0600 before they hold anything:

```sh
mkdir -p ~/.config/ecl-client && chmod 700 ~/.config/ecl-client
for f in config nova.config; do (umask 077; touch ~/.config/ecl-client/$f); done
printf 'instance=mu2e\nusername=<ecl user>\nauth=password\n' > ~/.config/ecl-client/config
printf 'instance=nova\nusername=<ecl user>\nauth=password\n' > ~/.config/ecl-client/nova.config
```

Then add a `password=<your password>` line to each file in an editor. The key
must be exactly `password`; a password typed onto another line (for example
`auth=`) is not used as the password and may be displayed by diagnostics.
Accounts are per instance. NOvA posts use
`--config ~/.config/ecl-client/nova.config`.

## 5. Verify

```sh
ecl-post --version                                  # ecl-post 1.0.0
ecl-post -n -c Sandbox -t test                      # dry run: reads config, posts nothing
ls -l ~/.claude/skills | grep ecl                   # three links
```

Start a **new** Claude Code session. Sessions that were already running don't
pick up newly installed skills.

## Optional: Python module

Only needed for Python code, not for the skills:

```sh
python3 -m venv .venv && .venv/bin/pip install .    # macOS: export OPENSSL_ROOT_DIR first
```

## Notes

- The repository is public, so cloning needs no authentication. If it is made
  private, use `gh repo clone normanajn/ecl-client ...` or an SSH URL.
- On machines outside Fermilab, the ECL hosts may be reachable only through the
  VPN.
- Don't keep a password file on shared hosts where others have root or can
  read your home directory. Set `ECL_PASSWORD` for the session instead.
