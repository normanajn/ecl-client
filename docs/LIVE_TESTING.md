# Live testing against Mu2e and NOvA ECL

The CTest suite only ever talks to a local mock server. This guide covers
deliberately posting test entries to the production logbooks. **Every post is
permanent and visible to the collaboration**, so run the rungs in order and
stop at the first failure.

## Instances

| Experiment | Alias | Alias URL | Backend seen on 2026-09-23 |
|---|---|---|---|
| Mu2e | `mu2e` | `https://dbweb0.fnal.gov/ECL/mu2e` | `https://dbweb-b.fnal.gov:8443/ECL/mu2e` (303) |
| NOvA | `nova` | `https://dbweb0.fnal.gov/ECL/nova` | `https://dbweb-a.fnal.gov:8443/ECL/nova` (303) |

Aliases are lower case; `NOvA` returns HTTP 500. The alias host sends a 303 to
a backend, which is the redirect path that `ecl-post` preserves the POST
across. You need the Fermilab network or VPN if the ECL hosts are firewalled
from your location.

## Before you start

1. **Account.** Use either an ECL XML/API account (`ECL_AUTH=xml`) or an ECL
   account whose password works for HTTPS login (`ECL_AUTH=password`). The
   accounts are per instance: a Mu2e account does not authenticate to NOvA.
2. **Category.** Pick a category that exists and is meant for tests. Paths are
   exact and case sensitive. If you are unsure, check the category tree in
   the web UI for each logbook. The commands below use `$MU2E_CAT` and
   `$NOVA_CAT`.
3. **Tags.** ECL tags are defined per logbook. Leave `--tag` out until you
   know which tags each instance accepts; an undefined tag may make ECL reject
   the entry.
4. **Binary.** From the repository root, `build/ecl-post --version` should
   print `ecl-post 0.1.0`.

```sh
cd ~/Git-Repositories/Norman/ecl-client
export PATH="$PWD/build:$PWD/scripts:$PATH"
export MU2E_CAT='Sandbox'        # replace with the real Mu2e test category
export NOVA_CAT='Sandbox'        # replace with the real NOvA test category
export ECL_AUTH=password         # or xml for an API account
```

Credentials go in the environment only, never on the command line. Set them
per instance before that instance's rungs:

```sh
export ECL_USERNAME='your-mu2e-ecl-user'
read -rs 'ECL_PASSWORD?Mu2e ECL password: '; echo; export ECL_PASSWORD   # zsh
# bash: read -rsp 'Mu2e ECL password: ' ECL_PASSWORD; echo; export ECL_PASSWORD
```

## Rung 0: reachability (no credentials, posts nothing)

```sh
for i in mu2e nova; do
  curl -s -o /dev/null -w "$i %{http_code} -> %{redirect_url}\n" \
       https://dbweb0.fnal.gov/ECL/$i/E/index
done
```

Expected: `303 -> https://dbweb-?.fnal.gov:8443/ECL/<i>/E/index` for both.

## Rung 1: dry run (posts nothing)

```sh
ecl-post -n -i mu2e -c "$MU2E_CAT" -s 'ecl-client live test' -t 'dry run'
ecl-post -n -i nova -c "$NOVA_CAT" -s 'ecl-client live test' -t 'dry run'
```

Check the `category` attribute matches what you intend.

## Rung 2: minimal text entry

Mu2e:

```sh
ecl-post -i mu2e -c "$MU2E_CAT" \
  -s "ecl-client v0.0.2 live test: text ($(date -u +%FT%TZ))" \
  -t "Test entry from ecl-client on $(hostname). Safe to ignore."
```

NOvA (after switching `ECL_USERNAME` / `ECL_PASSWORD` to the NOvA account):

```sh
ecl-post -i nova -c "$NOVA_CAT" \
  -s "ecl-client v0.0.2 live test: text ($(date -u +%FT%TZ))" \
  -t "Test entry from ecl-client on $(hostname). Safe to ignore."
```

Expected output:

```text
Created ECL entry 123456
Endpoint: https://dbweb-b.fnal.gov:8443/ECL/mu2e/E/xml_post?salt=...
```

Open the entry in the web UI and check the subject, category and author.
Write down the ID; rung 4 uses it (`MU2E_ID=...`, `NOVA_ID=...`).

## Rung 3: attachment and image

```sh
printf 'ecl-client attachment test\n' > /tmp/ecl-test.txt
cp mu2e-logo.png /tmp/ecl-test.png          # any small PNG

ecl-post -i mu2e -c "$MU2E_CAT" \
  -s 'ecl-client live test: attachment + image' -t 'Attachment and image test.' \
  --attachment test-file=/tmp/ecl-test.txt --image /tmp/ecl-test.png
```

Repeat with `-i nova -c "$NOVA_CAT"`. In the UI, download the attachment and
compare it: `shasum /tmp/ecl-test.txt` should match.

## Rung 4: related entry, textile formatting

```sh
ecl-post -i mu2e -c "$MU2E_CAT" --related "$MU2E_ID" --format textile \
  -s 'ecl-client live test: related + textile' \
  -t 'h3. Follow-up

* bullet one
* bullet two'
```

Check the entry is linked from `$MU2E_ID` and that the Textile rendered.

## Rung 5: Python API

```sh
.venv/bin/python - <<'EOF'
import os, ecl_client as ecl
auth = ecl.AuthMode.PASSWORD if os.environ.get("ECL_AUTH") == "password" else ecl.AuthMode.XML_SIGNATURE
for inst, cat in (("mu2e", os.environ["MU2E_CAT"]),):   # add ("nova", os.environ["NOVA_CAT"]) with NOvA creds
    r = ecl.Client(ecl.instance_url(inst), os.environ["ECL_USERNAME"],
                   os.environ["ECL_PASSWORD"], auth).post(
        ecl.Entry(cat).subject("ecl-client live test: Python").text("Posted via ecl_client."))
    print(inst, r.entry_id, r.effective_url)
EOF
```

## Rung 6: step tracking (`ecl-track`)

This posts four linked entries.

```sh
OP=ecl-client-livetest-$(date +%Y%m%d-%H%M)
ecl-track start --op $OP --title 'ecl-track live test' -i mu2e -c "$MU2E_CAT" -t 'Three-step test.'
ecl-track step  --op $OP --name 'first step'  -t 'ok'
ecl-track step  --op $OP --name 'second step' --status warn -t 'simulated warning'
ecl-track end   --op $OP --status ok
ecl-track status --op $OP
```

## Negative checks (should fail, nothing is created)

```sh
# wrong password -> HTTP error from ECL, exit status 1
ECL_PASSWORD=wrong ecl-post -i mu2e -c "$MU2E_CAT" -t x; echo "exit=$?"
# password mode over HTTP is refused locally, exit status 2
ecl-post --auth password -U http://dbweb0.fnal.gov/ECL/mu2e -c x -t x; echo "exit=$?"
```

## Recording results

Add a row per rung and instance to the test matrix in the project status
artifact: date, instance, auth mode, rung, entry ID, pass/fail, notes.
