#include <ecl/ecl.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ecl_entry *entry = ecl_entry_create("Sandbox");
    char *xml = NULL;
    size_t size = 0;
    assert(entry != NULL);
    assert(ecl_entry_set_subject(entry, "C compiler test") == ECL_OK);
    assert(ecl_entry_set_text(entry, "body") == ECL_OK);
    assert(ecl_entry_to_xml(entry, &xml, &size) == ECL_OK);
    assert(xml != NULL);
    assert(size == strlen(xml));
    assert(strstr(xml, "subject=\"C compiler test\"") != NULL);
    ecl_free(xml);
    ecl_entry_destroy(entry);
    puts("C API test passed");
    return 0;
}
