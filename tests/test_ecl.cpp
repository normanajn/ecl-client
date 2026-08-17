#include <ecl/ecl.h>
#include <ecl/ecl.hpp>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int main() {
    ecl::Entry entry("Parent & Child/<Ops>");
    entry.subject("A \"subject\" & more")
         .text("one < two\nthree & four")
         .form("shift-form")
         .format(ecl::TextFormat::Textile)
         .private_entry()
         .related(42)
         .tag("tag&one")
         .field("beam<state", "on & stable")
         .attachment_data("raw", "data.bin", {0x00, 0x01, 0x02, 0xff});

    const std::string expected =
        "<entry category=\"Parent &amp; Child/&lt;Ops&gt;\" formatted=\"yes\" html_safe=\"no\""
        " private=\"yes\" related=\"42\" subject=\"A &quot;subject&quot; &amp; more\">"
        "<tag name=\"tag&amp;one\"/>"
        "<attachment type=\"file\" name=\"raw\" filename=\"data.bin\">AAEC/w==</attachment>"
        "<form name=\"shift-form\"><field name=\"text\">one &lt; two\nthree &amp; four</field>"
        "<field name=\"beam&lt;state\">on &amp; stable</field></form></entry>";
    assert(entry.xml() == expected);

    ecl_entry *c_entry = ecl_entry_create("Sandbox");
    assert(c_entry != nullptr);
    assert(ecl_entry_set_text(c_entry, "C API") == ECL_OK);
    assert(ecl_entry_set_format(c_entry, ECL_FORMAT_PLAIN) == ECL_OK);
    assert(ecl_entry_set_related(c_entry, 0) == ECL_INVALID_ARGUMENT);
    assert(ecl_entry_add_field(c_entry, "state", "ready") == ECL_OK);
    char *xml = nullptr;
    size_t size = 0;
    assert(ecl_entry_to_xml(c_entry, &xml, &size) == ECL_OK);
    const std::string c_xml(xml, size);
    assert(c_xml == "<entry category=\"Sandbox\" formatted=\"no\" html_safe=\"no\">"
                    "<form name=\"default\"><field name=\"text\">C API</field>"
                    "<field name=\"state\">ready</field></form></entry>");
    ecl_free(xml);
    ecl_entry_destroy(c_entry);

    assert(std::string(ecl_version()) == "0.1.0");
    assert(std::string(ecl_status_string(ECL_HTTP_ERROR)) == "HTTP error");
    assert(ecl::instance_url("mu2e") == "https://dbweb0.fnal.gov/ECL/mu2e");
    assert(ecl::instance_url("https://example.test/ECL/test") == "https://example.test/ECL/test");

    std::cout << "native API tests passed\n";
    return 0;
}
