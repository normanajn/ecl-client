---
name: ecl-logbook
description: Post entries to a Fermilab Electronic Logbook (ECL), e.g. the Mu2e or NOvA elog, using this repo's ecl-post CLI or ecl_client Python module. Use when the user asks to "log this", "make a logbook entry", "post to the elog/ECL", record a result, shift note, run summary, or attach a plot or file to the Mu2e/NOvA/other ECL logbook, or to dry-run or check an ECL post. For multi-step procedures that need a linked record use ecl-track-steps; for adding logbook calls to another codebase use ecl-embed.
---

You are writing to a **production experiment logbook**. The collaboration reads
it, it is part of the operational record, and the XML API has **no edit or
delete**. A mistaken entry stays there until an ECL administrator removes it.
Treat every live post as an outward-facing, irreversible action.

## Locate the tools

The tools live in the `ecl-client` checkout, normally
`~/Git-Repositories/Norman/ecl-client`. Resolve them in this order:

```sh
ECL_HOME=${ECL_HOME:-$HOME/Git-Repositories/Norman/ecl-client}
ECL_POST=${ECL_POST:-$(command -v ecl-post || echo "$ECL_HOME/build/ecl-post")}
"$ECL_POST" --version          # expect: ecl-post 1.0.0 (or later)
```

If the binary is missing, build it (see `$ECL_HOME/README.md`). On macOS:
`cmake -S "$ECL_HOME" -B "$ECL_HOME/build" -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" && cmake --build "$ECL_HOME/build" -j`.
If the cache points at a stale Homebrew Cellar path, add `--fresh`.

Full option reference: `man "$ECL_HOME/man/ecl-post.1"`.

## Credentials: never see the password

- **Never** read, print, `cat`, `grep` the value of, or echo `ECL_PASSWORD` or
  the `password=` line of a config file. Never pass a password as an argument
  (`ecl-post` has no such option on purpose). Never write one into a file,
  commit, entry body, or chat message.
- `ecl-post` prompts on `/dev/tty`. Your Bash tool has no terminal, so the
  prompt fails with `no password provided and no terminal is available`.
  Credentials must already be in place. Check they exist without revealing
  them:

```sh
[ -n "${ECL_PASSWORD:+set}" ] && echo "ECL_PASSWORD: set" || echo "ECL_PASSWORD: not set"
echo "ECL_USERNAME=${ECL_USERNAME:-<unset>} ECL_AUTH=${ECL_AUTH:-<unset>} ECL_INSTANCE=${ECL_INSTANCE:-<unset>}"
CFG=${XDG_CONFIG_HOME:-$HOME/.config}/ecl-client/config
[ -f "$CFG" ] && { stat -c '%a %n' "$CFG" 2>/dev/null || stat -f '%Lp %N' "$CFG"
  awk -F= '/^[[:space:]]*(#|$)/ {next}
    { k=$1; gsub(/[[:space:]]/,"",k); v=substr($0,index($0,"=")+1); gsub(/^[[:space:]]+|[[:space:]]+$/,"",v)
      ok = (k=="auth" && (v=="xml"||v=="password")) ||
           (k~/^(instance|username|category|form|format|url)$/ && v~/^[A-Za-z0-9._\/:@-]{1,128}$/) ||
           (k~/^(timeout|connect_timeout)$/ && v~/^[0-9]+$/)
      if (k=="password") print "password=<set, " length(v) " chars>"
      else if (ok) print k "=" v
      else print k "=<REDACTED: unexpected value, check this line>" }' "$CFG"; }
```

This prints configuration values only when they match an expected shape. A
user can easily paste a password onto the wrong line, so anything unexpected
is redacted rather than shown. Never check the file with `cat` or `grep`.

- If nothing is set, ask the user to set up credentials. They can create the
  config file themselves (mode 0600), or start Claude Code from a shell that
  exports the variables. Offer this template, and never fill in the password
  yourself:

```ini
# ~/.config/ecl-client/config   (chmod 600)
instance=mu2e
username=<their ECL account>
auth=password          # or xml for an ECL XML/API account
password=<they type this>
```

- Accounts are **per instance**. A Mu2e ECL account does not authenticate to
  NOvA. If the user posts to both, keep a separate config per instance and
  select it with `--config ~/.config/ecl-client/nova.config`.

## Instances

| Experiment | `--instance` | Entry link |
|---|---|---|
| Mu2e | `mu2e` | `https://dbweb0.fnal.gov/ECL/mu2e/E/show?e=<ID>` |
| NOvA | `nova` | `https://dbweb0.fnal.gov/ECL/nova/E/show?e=<ID>` |
| other | lower-case alias from https://dbweb0.fnal.gov/redirector/ecl_instances.html | same pattern |

Aliases are lower case (`NOvA` gives HTTP 500). Category paths are exact and
case sensitive per instance (`Operations/DAQ`). Tags are defined per
logbook: pass `--tag` only with tags the user has named or that have already
worked in that logbook.

## Procedure for every live post

1. **Compose.** Write the subject and body from facts established in the
   session: commands run, outputs, versions, run numbers, who asked. Keep it
   factual and in the tone of an operations log. No speculation, no filler,
   no "I" as Claude. The entry is authored under the user's account; mention
   that it was prepared with Claude only if the user wants that.
2. **Scrub.** Keep secrets, tokens, private keys, passwords, session cookies
   and personal data out of the body and attachments. Check any attachment
   before attaching it (for example `head`, or `grep -iE 'passw|token|secret|BEGIN .*PRIVATE'`).
3. **Dry run.** Always run first with `-n`. The body goes in a file, so shell
   quoting cannot mangle it:

```sh
BODY=$(mktemp); cat > "$BODY" <<'EOF'
<entry text>
EOF
"$ECL_POST" -n -i mu2e -c 'Operations/DAQ' -s '<subject>' -T "$BODY" \
    --attachment summary=/path/to/summary.txt
```

4. **Confirm.** Before the first live post in a session, show the user the
   instance, category, subject, body and attachment list, and get an explicit
   yes. You don't need to ask again for each post only when the user has
   clearly authorised a series, for example "log each step of this upgrade to
   the Mu2e elog". That authorisation covers that series only.
5. **Post.** Run the same command without `-n`. Success looks like:

```text
Created ECL entry 123456
Endpoint: https://dbweb-b.fnal.gov:8443/ECL/mu2e/E/xml_post?salt=...
```

6. **Report.** Give the entry ID and the link
   `https://dbweb0.fnal.gov/ECL/<instance>/E/show?e=<ID>`. Keep the ID if
   follow-ups should be linked (`--related <ID>`).

## Useful forms

```sh
# body from a command's output (stdin)
some-report | "$ECL_POST" -i nova -c 'Sandbox' -s 'Nightly report' -T -
# follow-up linked to an earlier entry
"$ECL_POST" -i mu2e -c 'Operations/DAQ' --related 123456 -s 'Follow-up: fixed' -T "$BODY"
# Textile markup, a plot and a data file
"$ECL_POST" -i mu2e -c 'Operations/DAQ' --format textile -s 'Timing scan' -T "$BODY" \
    --image scan.png --attachment scan-data=scan.csv
# a form with fields (the form must exist in that logbook)
"$ECL_POST" -i mu2e -c 'Shift' -f 'ShiftCheck' --field beam=off --field hv=nominal -s 'Shift check'
```

## Errors

| stderr / exit | Meaning | Action |
|---|---|---|
| exit 2, `--category is required` etc. | Local usage error | Fix the arguments |
| exit 2, `no terminal is available` | No password in env or config | See Credentials above |
| exit 2, `must not be accessible by group or others` | Config file permissions are not 0600 | Ask the user to run `chmod 600` on it |
| exit 1, `HTTP 400` + generic HTML `400 Bad Request` page | ECL rejected the entry. Seen for a **disabled or missing category** (NOvA `Sandbox`, 2026-09-23) and for unauthenticated requests. The page does not say which | No entry was created. Ask the user to check the category is enabled/exists and that they can log in to that instance. Do not retry in a loop |
| exit 1, `HTTP 4xx ... Authentication failed` | Wrong account or password, or account on another instance | Ask the user to check; do not retry in a loop |
| exit 1, `HTTP 4xx/5xx` mentioning category/form/tag | Unknown category, form or tag | Ask for the exact path, retry once |
| exit 1, `refusing ECL redirect` | Redirect left `fnal.gov` | Stop and report it; don't use `--insecure` |
| exit 1, `Couldn't connect`/timeout | Network or VPN | Report it; the operation should continue |

Never use `--insecure` except to diagnose a TLS problem, and only on the
user's instruction.

## Python instead of the CLI

When already inside Python, use the module rather than a subprocess:

```python
import os, ecl_client as ecl
entry = ecl.Entry("Operations/DAQ").subject("...").text(body).attachment("log", "run.log")
r = ecl.Client(ecl.instance_url("mu2e"), os.environ["ECL_USERNAME"], os.environ["ECL_PASSWORD"],
               ecl.AuthMode.PASSWORD).post(entry)
```

Use the repo venv: `$ECL_HOME/.venv/bin/python`. `man "$ECL_HOME/man/ecl_client.3"` has the API.
