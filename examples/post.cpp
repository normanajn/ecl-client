#include <ecl/ecl.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    const char *password = std::getenv("ECL_PASSWORD");
    if (!password) throw std::runtime_error("ECL_PASSWORD is required");

    ecl::Entry entry("Sandbox");
    entry.subject("C++ API example")
         .text("Posted by the ecl-client C++ example")
         .tag("automated");
    ecl::Client client(ecl::instance_url("demo"), "api-user", password);
    const auto result = client.post(entry);
    std::cout << "Created entry " << result.entry_id << '\n';
}
