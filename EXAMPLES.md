# Posting to Fermilab ECL: Complete Examples

This guide shows how to post to a Fermilab Electronic Logbook using the
`ecl-post` command-line tool, the C API, the C++ API, and the Python interface.
It includes plain text entries, file attachments, image attachments, tags,
custom form fields, related entries, and private entries.

The examples use the `mu2e` instance and its `Sandbox` category. Replace those
values with the instance and exact, case-sensitive category path appropriate to
your logbook. Use a designated sandbox category for initial testing.

## Authentication

Two authentication modes are available:

- `password`: for an ECL account whose password authentication works over
  HTTPS. This is the mode to use if your current ECL credentials work with it.
- `xml`: for a dedicated ECL XML/API account created by an ECL administrator.
  This is the default mode in the client.

Never put a password directly in a command-line argument. For CLI examples,
provide credentials through the environment:

```sh
export ECL_USERNAME='your-ecl-username'
export ECL_AUTH='password'
printf 'ECL password: ' >&2
read -rs ECL_PASSWORD
printf '\n' >&2
export ECL_PASSWORD
```

Alternatively, use a protected configuration file at
`~/.config/ecl-client/config`:

```ini
instance=mu2e
username=your-ecl-username
password=your-ecl-password
auth=password
category=Sandbox
form=default
```

Protect a configuration containing a password:

```sh
chmod 600 ~/.config/ecl-client/config
```

If the password is absent from both the environment and configuration,
`ecl-post` prompts for it with terminal echo disabled.

## Command-line examples

The examples assume the project was built with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Simple text entry

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Test entry' \
  --text 'This entry was posted from the command line.'
```

`--instance mu2e` expands to
`https://dbweb0.fnal.gov/ECL/mu2e`. A complete instance URL can be supplied
instead:

```sh
build/ecl-post \
  --auth password \
  --url https://dbweb0.fnal.gov/ECL/mu2e \
  --category Sandbox \
  --subject 'Post using a complete URL' \
  --text 'The --url option accepts the ECL instance base URL.'
```

### Read entry text from a file

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Run summary' \
  --text-file summary.txt
```

The text file becomes the body of the ECL entry; it is not an attachment.

### Read entry text from standard input

```sh
generate-run-summary | build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Generated run summary' \
  --text-file -
```

Or use a shell pipeline directly:

```sh
printf 'Beam status: stable\nDAQ status: running\n' | build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Current status' \
  --text-file -
```

### Attach one file

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Entry with a report' \
  --text 'The generated report is attached.' \
  --attachment report.txt
```

An optional name can be placed before the path:

```sh
--attachment report=output/run-1234-report.txt
```

### Attach multiple files

Repeat `--attachment` for every file:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Run 1234 output' \
  --text 'The run report, structured results, and application log are attached.' \
  --attachment report=run-1234-report.pdf \
  --attachment results=run-1234-results.json \
  --attachment log=run-1234.log
```

### Attach an image

Use `--image` when the ECL should treat the attachment as an image:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Beam profile' \
  --text 'The latest beam profile is shown in the attached image.' \
  --image profile=beam-profile.png
```

Common image formats such as PNG and JPEG are appropriate. ECL performs its
own image processing and thumbnail generation.

### Attach files and images together

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Run 1234 analysis' \
  --text-file run-1234-summary.txt \
  --attachment data=run-1234-results.csv \
  --attachment log=run-1234.log \
  --image rate-plot=run-1234-rate.png \
  --image spectrum=run-1234-spectrum.png
```

### Tags

Repeat `--tag` to apply existing ECL tags:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Tagged entry' \
  --text 'This entry has two tags.' \
  --tag automated \
  --tag operations
```

Tags must already exist in the target ECL instance.

### Custom forms and fields

Specify the ECL form and repeat `--field NAME=VALUE` for its fields:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --form shift-summary \
  --subject 'Shift summary' \
  --text 'End-of-shift status report.' \
  --field beam_state=stable \
  --field daq_state=running \
  --field run_number=1234
```

The form name and field names must exactly match definitions in the target ECL
instance. Ordinary text entries use the `default` form.

### Related entry

Relate a new post to entry 12345:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Follow-up to entry 12345' \
  --text 'The issue described in the original entry is resolved.' \
  --related 12345
```

This creates a related entry; it does not add an ECL comment.

### Private entry

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Private test entry' \
  --text 'Visibility is restricted by ECL.' \
  --private
```

### Text formatting

Plain text is the safe default:

```sh
--format plain
```

Use ECL's Textile processing:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Textile example' \
  --format textile \
  --text $'h2. Status\n\n* Beam is stable\n* DAQ is running'
```

Preserve the body as preformatted text:

```sh
build/ecl-post \
  --auth password \
  --instance mu2e \
  --category Sandbox \
  --subject 'Preformatted output' \
  --format preformatted \
  --text-file command-output.txt
```

### Inspect XML without posting

`--dry-run` prints the exact XML and does not require credentials or network
access:

```sh
build/ecl-post \
  --dry-run \
  --category Sandbox \
  --subject 'Dry-run example' \
  --text 'No ECL entry will be created.' \
  --attachment report.txt \
  --image plot.png
```

## C library examples

Include `ecl/ecl.h` and link the `ECL::ecl` CMake target. The C interface uses
opaque client and entry objects. Every created object and returned result must
be released with its matching cleanup function.

### C text entry

```c
#include <ecl/ecl.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *username = getenv("ECL_USERNAME");
    const char *password = getenv("ECL_PASSWORD");
    if (!username || !password) {
        fprintf(stderr, "ECL_USERNAME and ECL_PASSWORD are required\n");
        return 2;
    }

    ecl_entry *entry = ecl_entry_create("Sandbox");
    ecl_entry_set_subject(entry, "C API text entry");
    ecl_entry_set_text(entry, "This entry was posted through the C API.");
    ecl_entry_set_format(entry, ECL_FORMAT_PLAIN);

    ecl_client *client = ecl_client_create(
        "https://dbweb0.fnal.gov/ECL/mu2e",
        username,
        password,
        ECL_AUTH_PASSWORD);

    if (!entry || !client) {
        fprintf(stderr, "Could not create the ECL entry or client\n");
        ecl_entry_destroy(entry);
        ecl_client_destroy(client);
        return 2;
    }

    ecl_post_result result = {0};
    ecl_status status = ecl_client_post(client, entry, &result);
    if (status == ECL_OK) {
        printf("Created ECL entry %lld\n", (long long)result.entry_id);
        printf("Endpoint: %s\n", result.effective_url);
    } else {
        fprintf(stderr, "Post failed: %s\n", ecl_client_last_error(client));
    }

    ecl_post_result_free(&result);
    ecl_client_destroy(client);
    ecl_entry_destroy(entry);
    return status == ECL_OK ? 0 : 1;
}
```

### C entry with files and images

```c
ecl_entry *entry = ecl_entry_create("Sandbox");
ecl_entry_set_subject(entry, "C API attachments");
ecl_entry_set_text(entry, "This entry contains a report and an image.");

ecl_status status;
status = ecl_entry_add_attachment_file(entry, "report", "run-report.pdf");
if (status != ECL_OK) {
    fprintf(stderr, "Could not add report: %s\n", ecl_status_string(status));
}

status = ecl_entry_add_image_file(entry, "plot", "beam-profile.png");
if (status != ECL_OK) {
    fprintf(stderr, "Could not add image: %s\n", ecl_status_string(status));
}
```

### C attachment from memory

File contents can be supplied without creating a file on disk:

```c
const unsigned char report[] = "state=ready\nrun=1234\n";

ecl_entry_add_attachment(
    entry,
    "status",
    "status.txt",
    report,
    sizeof(report) - 1);
```

The same pattern works for an in-memory image:

```c
ecl_entry_add_image(
    entry,
    "plot",
    "plot.png",
    png_bytes,
    png_size);
```

Binary data is Base64 encoded by the library.

### C tags, form fields, related and private entries

```c
ecl_entry_set_form(entry, "shift-summary");
ecl_entry_add_tag(entry, "automated");
ecl_entry_add_tag(entry, "operations");
ecl_entry_add_field(entry, "beam_state", "stable");
ecl_entry_add_field(entry, "run_number", "1234");
ecl_entry_set_related(entry, 12345);
ecl_entry_set_private(entry, 1);
```

### C XML/API account authentication

Change the final argument when using a dedicated XML/API account:

```c
ecl_client *client = ecl_client_create(
    "https://dbweb0.fnal.gov/ECL/mu2e",
    username,
    password,
    ECL_AUTH_XML_SIGNATURE);
```

### Build a C program with CMake

```cmake
cmake_minimum_required(VERSION 3.20)
project(ecl-c-example LANGUAGES C)

find_package(ecl-client CONFIG REQUIRED)
add_executable(post-ecl post.c)
target_link_libraries(post-ecl PRIVATE ECL::ecl)
```

Configure with the installation prefix if needed:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/ecl-client-install
cmake --build build
```

## C++ library examples

The C++ interface in `ecl/ecl.hpp` provides RAII object ownership, fluent entry
construction, and exceptions for errors.

### C++ text entry

```cpp
#include <ecl/ecl.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    const char *username = std::getenv("ECL_USERNAME");
    const char *password = std::getenv("ECL_PASSWORD");
    if (!username || !password) {
        throw std::runtime_error("ECL_USERNAME and ECL_PASSWORD are required");
    }

    ecl::Entry entry("Sandbox");
    entry.subject("C++ API text entry")
         .text("This entry was posted through the C++ API.")
         .format(ecl::TextFormat::Plain);

    ecl::Client client(
        ecl::instance_url("mu2e"),
        username,
        password,
        ecl::AuthMode::Password);

    try {
        const ecl::PostResult result = client.post(entry);
        std::cout << "Created ECL entry " << result.entry_id << '\n';
        std::cout << "Endpoint: " << result.effective_url << '\n';
    } catch (const ecl::Error &error) {
        std::cerr << "ECL post failed: " << error.what() << '\n';
        return 1;
    }
}
```

### C++ entry with files and images

```cpp
ecl::Entry entry("Sandbox");
entry.subject("C++ attachments")
     .text("This entry contains data files and plots.")
     .attachment("report", "run-report.pdf")
     .attachment("data", "run-results.json")
     .image("profile", "beam-profile.png")
     .image("spectrum", "energy-spectrum.jpg");
```

### C++ attachment from memory

```cpp
std::vector<unsigned char> report_bytes = create_report();
std::vector<unsigned char> png_bytes = create_plot();

entry.attachment_data("report", "report.dat", report_bytes)
     .image_data("plot", "plot.png", png_bytes);
```

### C++ tags, form fields, related and private entries

```cpp
entry.form("shift-summary")
     .tag("automated")
     .tag("operations")
     .field("beam_state", "stable")
     .field("run_number", "1234")
     .related(12345)
     .private_entry();
```

### C++ XML/API account authentication

```cpp
ecl::Client client(
    ecl::instance_url("mu2e"),
    username,
    password,
    ecl::AuthMode::XmlSignature);
```

### C++ timeouts and TLS

```cpp
client.connect_timeout(10)
      .timeout(60)
      .tls_verify(true)
      .user_agent("my-ecl-poster/1.0");
```

TLS verification should remain enabled for production posts.

## Python interface examples

Install the Python package in a virtual environment:

```sh
python3 -m venv .venv
.venv/bin/pip install .
```

### Python text entry

```python
import os

import ecl_client as ecl


entry = (
    ecl.Entry("Sandbox")
    .subject("Python text entry")
    .text("This entry was posted through the Python interface.")
    .format(ecl.TextFormat.PLAIN)
)

client = ecl.Client(
    ecl.instance_url("mu2e"),
    os.environ["ECL_USERNAME"],
    os.environ["ECL_PASSWORD"],
    ecl.AuthMode.PASSWORD,
)

try:
    result = client.post(entry)
    print(f"Created ECL entry {result.entry_id}")
    print(f"Endpoint: {result.effective_url}")
except ecl.ECLError as error:
    print(f"ECL post failed: {error}")
    raise
```

### Python entry with files and images

```python
entry = (
    ecl.Entry("Sandbox")
    .subject("Python attachments")
    .text("This entry contains data files and plots.")
    .attachment("report", "run-report.pdf")
    .attachment("data", "run-results.json")
    .image("profile", "beam-profile.png")
    .image("spectrum", "energy-spectrum.jpg")
)

result = client.post(entry)
print(result.entry_id)
```

### Python attachment from memory

```python
report_bytes = b"state=ready\nrun=1234\n"

with open("beam-profile.png", "rb") as stream:
    png_bytes = stream.read()

entry = (
    ecl.Entry("Sandbox")
    .subject("In-memory attachments")
    .text("Neither attachment needs to be read by the native library from disk.")
    .attachment_data("status", "status.txt", report_bytes)
    .image_data("profile", "beam-profile.png", png_bytes)
)
```

The data passed to `attachment_data` and `image_data` must be Python `bytes`.

### Python tags, form fields, related and private entries

```python
entry = (
    ecl.Entry("Sandbox")
    .subject("Structured Python entry")
    .text("A custom-form example.")
    .form("shift-summary")
    .tag("automated")
    .tag("operations")
    .field("beam_state", "stable")
    .field("run_number", "1234")
    .related(12345)
    .private_entry()
)
```

### Python Textile and preformatted text

```python
textile_entry = (
    ecl.Entry("Sandbox")
    .subject("Textile from Python")
    .text("h2. Status\n\n* Beam stable\n* DAQ running")
    .format(ecl.TextFormat.TEXTILE)
)

preformatted_entry = (
    ecl.Entry("Sandbox")
    .subject("Command output")
    .text("channel  rate\nA        123\nB        456")
    .format(ecl.TextFormat.PREFORMATTED)
)
```

### Python XML/API account authentication

```python
client = ecl.Client(
    ecl.instance_url("mu2e"),
    os.environ["ECL_USERNAME"],
    os.environ["ECL_PASSWORD"],
    ecl.AuthMode.XML_SIGNATURE,
)
```

### Inspect generated XML in Python

```python
print(entry.xml())
```

Calling `xml()` does not contact ECL or create an entry.

## Attachment behavior and practical notes

- File and image data is read into memory and Base64 encoded before posting.
  Plan memory usage accordingly for large attachments.
- `attachment` and `attachment_data` create ordinary downloadable files.
- `image` and `image_data` tell ECL to process the data as an image.
- Category paths, form names, field names, and tags are case-sensitive and must
  already exist in the selected ECL instance.
- A successful server response contains an entry ID. All interfaces expose that
  ID so automation can record or link to the created entry.
- The automated project tests use a local mock server and never create live ECL
  entries.
