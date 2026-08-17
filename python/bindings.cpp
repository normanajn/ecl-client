#include <ecl/ecl.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <vector>

namespace py = pybind11;

PYBIND11_MODULE(_native, module) {
    module.doc() = "Native client for the Fermilab Electronic Logbook";
    module.attr("__version__") = ecl_version();

    py::register_exception<ecl::Error>(module, "ECLError");

    py::enum_<ecl::AuthMode>(module, "AuthMode")
        .value("XML_SIGNATURE", ecl::AuthMode::XmlSignature)
        .value("PASSWORD", ecl::AuthMode::Password)
        .export_values();

    py::enum_<ecl::TextFormat>(module, "TextFormat")
        .value("PLAIN", ecl::TextFormat::Plain)
        .value("TEXTILE", ecl::TextFormat::Textile)
        .value("PREFORMATTED", ecl::TextFormat::Preformatted)
        .export_values();

    py::class_<ecl::Entry>(module, "Entry")
        .def(py::init<const std::string &>(), py::arg("category"))
        .def("subject", &ecl::Entry::subject, py::arg("value"), py::return_value_policy::reference_internal)
        .def("text", &ecl::Entry::text, py::arg("value"), py::return_value_policy::reference_internal)
        .def("form", &ecl::Entry::form, py::arg("value"), py::return_value_policy::reference_internal)
        .def("format", &ecl::Entry::format, py::arg("value"), py::return_value_policy::reference_internal)
        .def("private_entry", &ecl::Entry::private_entry, py::arg("value") = true,
             py::return_value_policy::reference_internal)
        .def("related", &ecl::Entry::related, py::arg("entry_id"), py::return_value_policy::reference_internal)
        .def("tag", &ecl::Entry::tag, py::arg("value"), py::return_value_policy::reference_internal)
        .def("field", &ecl::Entry::field, py::arg("name"), py::arg("value"),
             py::return_value_policy::reference_internal)
        .def("attachment", &ecl::Entry::attachment, py::arg("name"), py::arg("path"),
             py::return_value_policy::reference_internal)
        .def("attachment_data",
             [](ecl::Entry &entry, const std::string &name, const std::string &filename, py::bytes value) -> ecl::Entry & {
                 const std::string data = value;
                 return entry.attachment_data(name, filename,
                     std::vector<unsigned char>(data.begin(), data.end()));
             }, py::arg("name"), py::arg("filename"), py::arg("data"),
             py::return_value_policy::reference_internal)
        .def("image", &ecl::Entry::image, py::arg("name"), py::arg("path"),
             py::return_value_policy::reference_internal)
        .def("image_data",
             [](ecl::Entry &entry, const std::string &name, const std::string &filename, py::bytes value) -> ecl::Entry & {
                 const std::string data = value;
                 return entry.image_data(name, filename,
                     std::vector<unsigned char>(data.begin(), data.end()));
             }, py::arg("name"), py::arg("filename"), py::arg("data"),
             py::return_value_policy::reference_internal)
        .def("xml", &ecl::Entry::xml)
        .def("__str__", &ecl::Entry::xml);

    py::class_<ecl::PostResult>(module, "PostResult")
        .def_readonly("http_status", &ecl::PostResult::http_status)
        .def_readonly("entry_id", &ecl::PostResult::entry_id)
        .def_readonly("response_body", &ecl::PostResult::response_body)
        .def_readonly("effective_url", &ecl::PostResult::effective_url)
        .def_property_readonly("created", &ecl::PostResult::created);

    py::class_<ecl::Client>(module, "Client")
        .def(py::init<const std::string &, const std::string &, const std::string &, ecl::AuthMode>(),
             py::arg("instance_url"), py::arg("username"), py::arg("password"),
             py::arg("auth_mode") = ecl::AuthMode::XmlSignature)
        .def("timeout", &ecl::Client::timeout, py::arg("seconds"), py::return_value_policy::reference_internal)
        .def("connect_timeout", &ecl::Client::connect_timeout, py::arg("seconds"),
             py::return_value_policy::reference_internal)
        .def("tls_verify", &ecl::Client::tls_verify, py::arg("enabled"),
             py::return_value_policy::reference_internal)
        .def("user_agent", &ecl::Client::user_agent, py::arg("value"),
             py::return_value_policy::reference_internal)
        .def("post", &ecl::Client::post, py::arg("entry"), py::call_guard<py::gil_scoped_release>());

    module.def("instance_url", &ecl::instance_url, py::arg("name"));
}
