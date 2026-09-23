---
name: ecl-track-steps
description: Record the steps of a major operation (upgrade, firmware flash, deployment, calibration campaign, recovery, commissioning procedure) in a Fermilab ECL logbook as a linked thread of entries using ecl-track — START, one entry per step related to START, END summary. Use when the user says to "log/track the steps in the elog", "keep a logbook record of this procedure", "record this upgrade in the Mu2e/NOvA logbook", or when a session is executing a planned multi-step change to a production system and the user has asked for logbook tracking.
---

This skill builds on **ecl-logbook**. Its rules on credentials, scrubbing and
confirmation all apply here; load it too if it is not already loaded.

`ecl-track` (`$ECL_HOME/scripts/ecl-track`, man page `ecl-track(1)`) keeps
per-operation state in `.ecl-track/<op>.json` and calls `ecl-post` for each
entry. The result in the logbook:

```text
[daq-fw-2026-09-23] START: DTC firmware upgrade to v4.2        <- parent
[daq-fw-2026-09-23] step 1: Run stopped -- OK                  related=parent
[daq-fw-2026-09-23] step 2: Flash DTC 0-5 -- OK                related=parent
[daq-fw-2026-09-23] step 3: Link check -- FAIL                 related=parent
[daq-fw-2026-09-23] END -- PARTIAL   (table of steps + entry IDs)
```

## Setup

```sh
ECL_HOME=${ECL_HOME:-$HOME/Git-Repositories/Norman/ecl-client}
ECL_TRACK="$ECL_HOME/scripts/ecl-track"
export ECL_POST=${ECL_POST:-$(command -v ecl-post || echo "$ECL_HOME/build/ecl-post")}
export ECL_TRACK_DIR=${ECL_TRACK_DIR:-$PWD/.ecl-track}   # in the project being worked on
```

Add `.ecl-track/` to that project's `.gitignore`. The state holds entry bodies,
which don't belong in commits.

## Agree the plan before the first entry

Confirm with the user in one message, once:
- **op id**: short, unique, dated, e.g. `mu2e-tracker-hv-2026-09-23`
- **instance / category** (and tags, if they name any)
- **step granularity**: which actions get an entry. Record milestones and
  anything that changes system state, not every shell command.
- that they want the thread posted live. Offer a dry run of the START entry
  first.

Their yes authorises this operation's entries only. Don't ask again per step,
but do stop and ask if the operation's scope changes.

## During the operation

```sh
"$ECL_TRACK" start --op "$OP" --title 'DTC firmware upgrade to v4.2' \
    -i mu2e -c 'Operations/DAQ' -T plan.txt           # instance/category saved for later calls

# after each milestone, with the actual evidence
"$ECL_TRACK" step --op "$OP" --name 'Flash DTC 0-5' --status ok \
    -T step.txt --attachment flash-log=flash.log

"$ECL_TRACK" step --op "$OP" --name 'Link check' --status fail -T failure.txt

"$ECL_TRACK" end --op "$OP" --status partial -t 'Rolled back DTC 3; ticket INC000123.'
```

- `--status` for a step: `ok | fail | warn | info | skip`. For the end:
  `ok | fail | aborted | partial`. Report failures honestly. A step that
  failed and was then retried is two steps.
- Write each step body **after** the step, from real output: what was done
  (command or action), what was observed (key numbers or lines), and the
  result. Attach longer logs after scrubbing them.
- A context footer (UTC time, host, user, cwd, `git describe`) is added
  automatically. Use `--no-context` only if the user wants it off.
- Extra `ecl-post` options on `step`/`end` (e.g. `--image`, `--tag`) apply to
  that entry only. `--subject`, `--text*`, `--related` and `--password` are
  managed by ecl-track and are rejected.
- Separate processes, scripts and later sessions can add to the same `--op`
  as long as they share `ECL_TRACK_DIR`.

## Failures do not stop the operation

If ECL is unreachable or rejects a post, `ecl-track` prints
`WARNING: ... queued` and exits 0. **Carry on with the operation.** Tell the
user once, then:

```sh
"$ECL_TRACK" status --op "$OP"      # shows PENDING entries
"$ECL_TRACK" flush  --op "$OP"      # retry; exit 1 while some remain
```

Use `--strict` only when the user says the log entry is a required gate.
If the START entry itself failed, later steps are queued unlinked, and `flush`
links them once START succeeds.

## Rehearsal

`start ... --dry-run` makes the whole operation a dry run: every later call
prints XML and nothing is posted. Use a separate `--op` (e.g. suffix `-dry`)
so the rehearsal state doesn't collide with the real run.

## At the end

Report to the user: the op id, the START entry link
`https://dbweb0.fnal.gov/ECL/<instance>/E/show?e=<parent_id>`, the number of
steps by status, and any entries still pending.
