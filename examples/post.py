import os

import ecl_client as ecl


entry = (
    ecl.Entry("Sandbox")
    .subject("Python API example")
    .text("Posted by the ecl-client Python example")
    .tag("automated")
)
client = ecl.Client(ecl.instance_url("demo"), "api-user", os.environ["ECL_PASSWORD"])
result = client.post(entry)
print(f"Created entry {result.entry_id}")
