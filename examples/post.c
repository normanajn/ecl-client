#include <ecl/ecl.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *password = getenv("ECL_PASSWORD");
    if (!password) {
        fprintf(stderr, "ECL_PASSWORD is required\n");
        return 2;
    }
    ecl_entry *entry = ecl_entry_create("Sandbox");
    ecl_entry_set_subject(entry, "C API example");
    ecl_entry_set_text(entry, "Posted by the ecl-client C example");
    ecl_entry_add_tag(entry, "automated");

    ecl_client *client = ecl_client_create(
        "https://dbweb0.fnal.gov/ECL/demo", "api-user", password,
        ECL_AUTH_XML_SIGNATURE);
    ecl_post_result result = {0};
    const ecl_status status = ecl_client_post(client, entry, &result);
    if (status == ECL_OK) printf("Created entry %lld\n", (long long)result.entry_id);
    else fprintf(stderr, "%s\n", ecl_client_last_error(client));

    ecl_post_result_free(&result);
    ecl_client_destroy(client);
    ecl_entry_destroy(entry);
    return status == ECL_OK ? 0 : 1;
}
