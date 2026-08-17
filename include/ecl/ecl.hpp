#ifndef ECL_ECL_HPP
#define ECL_ECL_HPP

#include <ecl/ecl.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ecl {

enum class AuthMode {
    XmlSignature = ECL_AUTH_XML_SIGNATURE,
    Password = ECL_AUTH_PASSWORD
};

enum class TextFormat {
    Plain = ECL_FORMAT_PLAIN,
    Textile = ECL_FORMAT_TEXTILE,
    Preformatted = ECL_FORMAT_PREFORMATTED
};

class Error : public std::runtime_error {
public:
    Error(ecl_status status, const std::string &message)
        : std::runtime_error(message), status_(status) {}
    ecl_status status() const noexcept { return status_; }

private:
    ecl_status status_;
};

inline void check(ecl_status status, const std::string &context) {
    if (status != ECL_OK) {
        throw Error(status, context + ": " + ecl_status_string(status));
    }
}

class Entry {
public:
    explicit Entry(const std::string &category) : handle_(ecl_entry_create(category.c_str())) {
        if (!handle_) throw Error(ECL_INVALID_ARGUMENT, "could not create entry");
    }
    ~Entry() { ecl_entry_destroy(handle_); }
    Entry(const Entry &) = delete;
    Entry &operator=(const Entry &) = delete;
    Entry(Entry &&other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Entry &operator=(Entry &&other) noexcept {
        if (this != &other) {
            ecl_entry_destroy(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Entry &subject(const std::string &value) {
        check(ecl_entry_set_subject(handle_, value.c_str()), "set subject"); return *this;
    }
    Entry &text(const std::string &value) {
        check(ecl_entry_set_text(handle_, value.c_str()), "set text"); return *this;
    }
    Entry &form(const std::string &value) {
        check(ecl_entry_set_form(handle_, value.c_str()), "set form"); return *this;
    }
    Entry &format(TextFormat value) {
        check(ecl_entry_set_format(handle_, static_cast<ecl_text_format>(value)), "set format"); return *this;
    }
    Entry &private_entry(bool value = true) {
        check(ecl_entry_set_private(handle_, value), "set privacy"); return *this;
    }
    Entry &related(std::int64_t value) {
        check(ecl_entry_set_related(handle_, value), "set related entry"); return *this;
    }
    Entry &tag(const std::string &value) {
        check(ecl_entry_add_tag(handle_, value.c_str()), "add tag"); return *this;
    }
    Entry &field(const std::string &name, const std::string &value) {
        check(ecl_entry_add_field(handle_, name.c_str(), value.c_str()), "add field"); return *this;
    }
    Entry &attachment(const std::string &name, const std::string &path) {
        check(ecl_entry_add_attachment_file(handle_, name.c_str(), path.c_str()), "add attachment"); return *this;
    }
    Entry &attachment_data(const std::string &name, const std::string &filename,
                           const std::vector<unsigned char> &data) {
        check(ecl_entry_add_attachment(handle_, name.c_str(), filename.c_str(),
              data.data(), data.size()), "add attachment data"); return *this;
    }
    Entry &image(const std::string &name, const std::string &path) {
        check(ecl_entry_add_image_file(handle_, name.c_str(), path.c_str()), "add image"); return *this;
    }
    Entry &image_data(const std::string &name, const std::string &filename,
                      const std::vector<unsigned char> &data) {
        check(ecl_entry_add_image(handle_, name.c_str(), filename.c_str(),
              data.data(), data.size()), "add image data"); return *this;
    }
    std::string xml() const {
        char *value = nullptr;
        size_t size = 0;
        check(ecl_entry_to_xml(handle_, &value, &size), "serialize entry");
        std::string result(value, size);
        ecl_free(value);
        return result;
    }
    const ecl_entry *native_handle() const noexcept { return handle_; }

private:
    ecl_entry *handle_;
};

struct PostResult {
    long http_status = 0;
    std::int64_t entry_id = -1;
    std::string response_body;
    std::string effective_url;

    bool created() const noexcept { return http_status >= 200 && http_status < 300 && entry_id >= 0; }
};

class Client {
public:
    Client(const std::string &instance_url, const std::string &username,
           const std::string &password, AuthMode auth_mode = AuthMode::XmlSignature)
        : handle_(ecl_client_create(instance_url.c_str(), username.c_str(), password.c_str(),
                                   static_cast<ecl_auth_mode>(auth_mode))) {
        if (!handle_) throw Error(ECL_INVALID_ARGUMENT, "could not create client");
    }
    ~Client() { ecl_client_destroy(handle_); }
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
    Client(Client &&other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Client &operator=(Client &&other) noexcept {
        if (this != &other) {
            ecl_client_destroy(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Client &timeout(long seconds) {
        check(ecl_client_set_timeout(handle_, seconds), "set timeout"); return *this;
    }
    Client &connect_timeout(long seconds) {
        check(ecl_client_set_connect_timeout(handle_, seconds), "set connect timeout"); return *this;
    }
    Client &tls_verify(bool enabled) {
        check(ecl_client_set_tls_verify(handle_, enabled), "set TLS verification"); return *this;
    }
    Client &user_agent(const std::string &value) {
        check(ecl_client_set_user_agent(handle_, value.c_str()), "set user agent"); return *this;
    }
    PostResult post(const Entry &entry) {
        ecl_post_result raw{};
        const ecl_status status = ecl_client_post(handle_, entry.native_handle(), &raw);
        if (status != ECL_OK) {
            const std::string message = ecl_client_last_error(handle_);
            ecl_post_result_free(&raw);
            throw Error(status, message.empty() ? ecl_status_string(status) : message);
        }
        PostResult result;
        result.http_status = raw.http_status;
        result.entry_id = raw.entry_id;
        if (raw.response_body) result.response_body = raw.response_body;
        if (raw.effective_url) result.effective_url = raw.effective_url;
        ecl_post_result_free(&raw);
        return result;
    }

private:
    ecl_client *handle_;
};

inline std::string instance_url(const std::string &name) {
    if (name.find("://") != std::string::npos) return name;
    return "https://dbweb0.fnal.gov/ECL/" + name;
}

} // namespace ecl

#endif
