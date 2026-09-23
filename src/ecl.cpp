#include <ecl/ecl.h>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Attachment {
    std::string name;
    std::string filename;
    std::vector<unsigned char> data;
    bool image = false;
};

struct EntryData {
    std::string category;
    std::string subject;
    std::string text;
    std::string form = "default";
    ecl_text_format format = ECL_FORMAT_PLAIN;
    bool private_entry = false;
    std::int64_t related = 0;
    std::vector<std::string> tags;
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<Attachment> attachments;
};

struct ClientData {
    std::string url;
    std::string username;
    std::string password;
    ecl_auth_mode auth_mode = ECL_AUTH_XML_SIGNATURE;
    long timeout = 30;
    long connect_timeout = 10;
    bool tls_verify = true;
    std::string user_agent = "ecl-client/" ECL_VERSION_STRING;
    std::string last_error;

    ~ClientData() {
        if (!password.empty()) OPENSSL_cleanse(password.data(), password.size());
    }
};

void append_xml_escaped(std::string &out, const std::string &value, bool attribute) {
    for (const char raw : value) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += attribute ? "&quot;" : "\""; break;
        case '\'': out += attribute ? "&apos;" : "'"; break;
        default:
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                out += "\xEF\xBF\xBD";
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
}

std::string xml_attr(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    append_xml_escaped(out, value, true);
    return out;
}

std::string xml_text(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    append_xml_escaped(out, value, false);
    return out;
}

std::string base64_encode(const std::vector<unsigned char> &data) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const std::uint32_t a = data[i];
        const std::uint32_t b = i + 1 < data.size() ? data[i + 1] : 0;
        const std::uint32_t c = i + 2 < data.size() ? data[i + 2] : 0;
        const std::uint32_t triple = (a << 16U) | (b << 8U) | c;
        out.push_back(alphabet[(triple >> 18U) & 0x3fU]);
        out.push_back(alphabet[(triple >> 12U) & 0x3fU]);
        out.push_back(i + 1 < data.size() ? alphabet[(triple >> 6U) & 0x3fU] : '=');
        out.push_back(i + 2 < data.size() ? alphabet[triple & 0x3fU] : '=');
    }
    return out;
}

std::string serialize(const EntryData &entry) {
    bool formatted = false;
    bool html_safe = false;
    if (entry.format == ECL_FORMAT_TEXTILE) formatted = true;
    if (entry.format == ECL_FORMAT_PREFORMATTED) html_safe = true;

    std::string xml = "<entry category=\"" + xml_attr(entry.category) + "\"";
    xml += formatted ? " formatted=\"yes\"" : " formatted=\"no\"";
    xml += html_safe ? " html_safe=\"yes\"" : " html_safe=\"no\"";
    if (entry.private_entry) xml += " private=\"yes\"";
    if (entry.related > 0) xml += " related=\"" + std::to_string(entry.related) + "\"";
    if (!entry.subject.empty()) xml += " subject=\"" + xml_attr(entry.subject) + "\"";
    xml += ">";

    for (const auto &tag : entry.tags) {
        xml += "<tag name=\"" + xml_attr(tag) + "\"/>";
    }
    for (const auto &attachment : entry.attachments) {
        xml += "<attachment type=\"";
        xml += attachment.image ? "image" : "file";
        xml += "\" name=\"" + xml_attr(attachment.name) + "\" filename=\"";
        xml += xml_attr(attachment.filename) + "\">";
        xml += base64_encode(attachment.data);
        xml += "</attachment>";
    }
    xml += "<form name=\"" + xml_attr(entry.form) + "\">";
    if (!entry.text.empty()) {
        xml += "<field name=\"text\">" + xml_text(entry.text) + "</field>";
    }
    for (const auto &field : entry.fields) {
        xml += "<field name=\"" + xml_attr(field.first) + "\">";
        xml += xml_text(field.second) + "</field>";
    }
    xml += "</form></entry>";
    return xml;
}

bool valid_auth_mode(ecl_auth_mode mode) {
    return mode == ECL_AUTH_XML_SIGNATURE || mode == ECL_AUTH_PASSWORD;
}

bool valid_format(ecl_text_format format) {
    return format == ECL_FORMAT_PLAIN || format == ECL_FORMAT_TEXTILE ||
           format == ECL_FORMAT_PREFORMATTED;
}

std::string trim_trailing_slashes(std::string value) {
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

std::string basename(const std::string &path) {
    const std::size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool read_file(const char *path, std::vector<unsigned char> &data, std::string &error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = std::string("cannot open '") + path + "': " + std::strerror(errno);
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0) {
        error = std::string("cannot determine size of '") + path + "'";
        return false;
    }
    if (static_cast<unsigned long long>(length) >
        static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        error = std::string("file is too large: '") + path + "'";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    data.resize(static_cast<std::size_t>(length));
    if (length > 0 && !stream.read(reinterpret_cast<char *>(data.data()), length)) {
        error = std::string("cannot read '") + path + "'";
        return false;
    }
    return true;
}

std::string md5_hex(const std::string &input) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context) throw std::runtime_error("cannot allocate digest context");
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_DigestInit_ex(context.get(), EVP_md5(), nullptr) != 1 ||
        EVP_DigestUpdate(context.get(), input.data(), input.size()) != 1 ||
        EVP_DigestFinal_ex(context.get(), digest, &length) != 1) {
        throw std::runtime_error("cannot compute MD5 signature");
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i) result << std::setw(2) << static_cast<int>(digest[i]);
    return result.str();
}

std::string make_salt() {
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("cannot generate request salt");
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const unsigned char byte : bytes) result << std::setw(2) << static_cast<int>(byte);
    return result.str();
}

char *copy_c_string(const std::string &value) {
    char *copy = static_cast<char *>(std::malloc(value.size() + 1));
    if (!copy) return nullptr;
    std::memcpy(copy, value.data(), value.size());
    copy[value.size()] = '\0';
    return copy;
}

size_t write_callback(char *data, size_t size, size_t count, void *user_data) {
    const size_t total = size * count;
    static_cast<std::string *>(user_data)->append(data, total);
    return total;
}

struct ResponseHeaders {
    std::string location;
};

size_t header_callback(char *data, size_t size, size_t count, void *user_data) {
    const size_t total = size * count;
    std::string line(data, total);
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string name = line.substr(0, colon);
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == "location") {
            std::string value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t");
            const auto last = value.find_last_not_of(" \t\r\n");
            static_cast<ResponseHeaders *>(user_data)->location =
                first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
        }
    }
    return total;
}

struct ParsedUrl {
    std::string scheme;
    std::string host;
};

std::string url_part(CURLU *url, CURLUPart part) {
    char *value = nullptr;
    if (curl_url_get(url, part, &value, 0) != CURLUE_OK || !value) return {};
    std::string result(value);
    curl_free(value);
    return result;
}

ParsedUrl parse_url(const std::string &value) {
    std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> url(curl_url(), curl_url_cleanup);
    if (!url || curl_url_set(url.get(), CURLUPART_URL, value.c_str(), 0) != CURLUE_OK) {
        throw std::runtime_error("invalid URL: " + value);
    }
    ParsedUrl parsed{url_part(url.get(), CURLUPART_SCHEME), url_part(url.get(), CURLUPART_HOST)};
    std::transform(parsed.scheme.begin(), parsed.scheme.end(), parsed.scheme.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(parsed.host.begin(), parsed.host.end(), parsed.host.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if ((parsed.scheme != "http" && parsed.scheme != "https") || parsed.host.empty()) {
        throw std::runtime_error("URL must use HTTP or HTTPS and include a host: " + value);
    }
    return parsed;
}

std::string resolve_redirect(const std::string &current, const std::string &location) {
    std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> url(curl_url(), curl_url_cleanup);
    if (!url || curl_url_set(url.get(), CURLUPART_URL, current.c_str(), 0) != CURLUE_OK ||
        curl_url_set(url.get(), CURLUPART_URL, location.c_str(), 0) != CURLUE_OK) {
        throw std::runtime_error("invalid redirect location: " + location);
    }
    const std::string result = url_part(url.get(), CURLUPART_URL);
    if (result.empty()) throw std::runtime_error("invalid redirect location: " + location);
    return result;
}

bool fnal_host(const std::string &host) {
    static const std::string suffix = ".fnal.gov";
    return host == "fnal.gov" ||
           (host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0);
}

bool redirect_allowed(const ParsedUrl &origin, const ParsedUrl &target, ecl_auth_mode auth_mode) {
    if (auth_mode == ECL_AUTH_PASSWORD && target.scheme != "https") return false;
    if (target.scheme != "http" && target.scheme != "https") return false;
    if (target.host == origin.host) return true;
    return fnal_host(origin.host) && fnal_host(target.host);
}

bool redirect_status(long status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

std::string query_from_url(const std::string &url) {
    const auto question = url.find('?');
    if (question == std::string::npos) return {};
    const auto fragment = url.find('#', question + 1);
    return url.substr(question + 1, fragment == std::string::npos ? std::string::npos : fragment - question - 1);
}

struct CurlGlobal {
    CurlGlobal() : status(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
    ~CurlGlobal() { if (status == CURLE_OK) curl_global_cleanup(); }
    CURLcode status;
};

CurlGlobal &curl_global() {
    static CurlGlobal global;
    return global;
}

std::string response_summary(long status, const std::string &body) {
    std::string compact = body;
    compact.erase(std::remove(compact.begin(), compact.end(), '\r'), compact.end());
    while (!compact.empty() && std::isspace(static_cast<unsigned char>(compact.back()))) compact.pop_back();
    if (compact.size() > 500) compact.resize(500);
    return "ECL returned HTTP " + std::to_string(status) + (compact.empty() ? "" : ": " + compact);
}

} // namespace

struct ecl_entry { EntryData data; std::string last_error; };
struct ecl_client { ClientData data; };

extern "C" {

const char *ecl_version(void) { return ECL_VERSION_STRING; }

const char *ecl_status_string(ecl_status status) {
    switch (status) {
    case ECL_OK: return "success";
    case ECL_INVALID_ARGUMENT: return "invalid argument";
    case ECL_IO_ERROR: return "I/O error";
    case ECL_HTTP_ERROR: return "HTTP error";
    case ECL_PROTOCOL_ERROR: return "ECL protocol error";
    case ECL_OUT_OF_MEMORY: return "out of memory";
    case ECL_INTERNAL_ERROR: return "internal error";
    }
    return "unknown error";
}

ecl_client *ecl_client_create(const char *instance_url, const char *username,
                              const char *password, ecl_auth_mode auth_mode) {
    if (!instance_url || !*instance_url || !username || !*username || !password || !*password ||
        !valid_auth_mode(auth_mode)) return nullptr;
    try {
        auto client = std::make_unique<ecl_client>();
        client->data.url = trim_trailing_slashes(instance_url);
        const ParsedUrl parsed = parse_url(client->data.url);
        client->data.username = username;
        client->data.password = password;
        client->data.auth_mode = auth_mode;
        if (auth_mode == ECL_AUTH_PASSWORD && parsed.scheme != "https") return nullptr;
        return client.release();
    } catch (...) {
        return nullptr;
    }
}

void ecl_client_destroy(ecl_client *client) { delete client; }

ecl_status ecl_client_set_timeout(ecl_client *client, long seconds) {
    if (!client || seconds <= 0) return ECL_INVALID_ARGUMENT;
    client->data.timeout = seconds;
    return ECL_OK;
}

ecl_status ecl_client_set_connect_timeout(ecl_client *client, long seconds) {
    if (!client || seconds <= 0) return ECL_INVALID_ARGUMENT;
    client->data.connect_timeout = seconds;
    return ECL_OK;
}

ecl_status ecl_client_set_tls_verify(ecl_client *client, int enabled) {
    if (!client) return ECL_INVALID_ARGUMENT;
    client->data.tls_verify = enabled != 0;
    return ECL_OK;
}

ecl_status ecl_client_set_user_agent(ecl_client *client, const char *user_agent) {
    if (!client || !user_agent || !*user_agent) return ECL_INVALID_ARGUMENT;
    try { client->data.user_agent = user_agent; return ECL_OK; }
    catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

const char *ecl_client_last_error(const ecl_client *client) {
    return client ? client->data.last_error.c_str() : "invalid client";
}

ecl_status ecl_client_post(ecl_client *client, const ecl_entry *entry, ecl_post_result *result) {
    if (!client || !entry || !result) return ECL_INVALID_ARGUMENT;
    *result = {};
    result->entry_id = -1;
    client->data.last_error.clear();
    try {
        if (curl_global().status != CURLE_OK) {
            client->data.last_error = "libcurl global initialization failed";
            return ECL_INTERNAL_ERROR;
        }
        const std::string body = serialize(entry->data);
        std::string current_url = client->data.url + "/E/xml_post?salt=" + make_salt();
        const ParsedUrl origin = parse_url(current_url);
        std::string response;
        long response_status = 0;
        for (unsigned int redirects = 0;; ++redirects) {
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
            if (!curl) {
                client->data.last_error = "cannot initialize libcurl";
                return ECL_INTERNAL_ERROR;
            }
            struct curl_slist *headers = nullptr;
            auto append_header = [&headers](const std::string &header) {
                curl_slist *next = curl_slist_append(headers, header.c_str());
                if (!next) throw std::bad_alloc();
                headers = next;
            };
            std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> header_guard(nullptr, curl_slist_free_all);
            append_header("Content-Type: text/xml; charset=utf-8");
            append_header("Expect:");
            append_header("X-User: " + client->data.username);
            if (client->data.auth_mode == ECL_AUTH_PASSWORD) {
                append_header("X-Password: " + client->data.password);
            } else {
                const std::string query = query_from_url(current_url);
                const std::string signature = md5_hex(query + ":" + client->data.password + ":" + body);
                append_header("X-Signature-Method: md5");
                append_header("X-Signature: " + signature);
            }
            header_guard.reset(headers);

            response.clear();
            ResponseHeaders response_headers;
            char error_buffer[CURL_ERROR_SIZE] = {};
            curl_easy_setopt(curl.get(), CURLOPT_URL, current_url.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.data());
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
            curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, header_callback);
            curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &response_headers);
            curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer);
            curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, client->data.user_agent.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, client->data.timeout);
            curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, client->data.connect_timeout);
            curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
            curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, client->data.tls_verify ? 1L : 0L);
            curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, client->data.tls_verify ? 2L : 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
            curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR,
                             client->data.auth_mode == ECL_AUTH_PASSWORD ? "https" : "http,https");
#else
            curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS,
                             client->data.auth_mode == ECL_AUTH_PASSWORD ? CURLPROTO_HTTPS :
                             (CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif

            const CURLcode code = curl_easy_perform(curl.get());
            if (code != CURLE_OK) {
                client->data.last_error = error_buffer[0] ? error_buffer : curl_easy_strerror(code);
                return ECL_IO_ERROR;
            }
            curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_status);
            if (!redirect_status(response_status)) break;
            if (response_headers.location.empty()) {
                client->data.last_error = "ECL redirect did not include a Location header";
                return ECL_PROTOCOL_ERROR;
            }
            if (redirects >= 5) {
                client->data.last_error = "too many ECL redirects";
                return ECL_PROTOCOL_ERROR;
            }
            const std::string target_url = resolve_redirect(current_url, response_headers.location);
            const ParsedUrl target = parse_url(target_url);
            if (!redirect_allowed(origin, target, client->data.auth_mode)) {
                client->data.last_error = "refusing ECL redirect from " + origin.host + " to " + target.host;
                return ECL_PROTOCOL_ERROR;
            }
            current_url = target_url;
        }

        result->http_status = response_status;
        result->response_body = copy_c_string(response);
        result->effective_url = copy_c_string(current_url);
        if (!result->response_body || !result->effective_url) {
            ecl_post_result_free(result);
            client->data.last_error = "cannot allocate response";
            return ECL_OUT_OF_MEMORY;
        }
        if (result->http_status < 200 || result->http_status >= 300) {
            client->data.last_error = response_summary(result->http_status, response);
            return ECL_HTTP_ERROR;
        }
        static const std::regex created_pattern(R"(^\s*Created\s+([0-9]+)\s*$)");
        std::smatch match;
        if (!std::regex_match(response, match, created_pattern)) {
            client->data.last_error = "unexpected ECL response: " + response;
            return ECL_PROTOCOL_ERROR;
        }
        result->entry_id = std::stoll(match[1].str());
        return ECL_OK;
    } catch (const std::bad_alloc &) {
        client->data.last_error = "out of memory";
        return ECL_OUT_OF_MEMORY;
    } catch (const std::exception &error) {
        client->data.last_error = error.what();
        return ECL_INTERNAL_ERROR;
    } catch (...) {
        client->data.last_error = "unknown internal error";
        return ECL_INTERNAL_ERROR;
    }
}

void ecl_post_result_free(ecl_post_result *result) {
    if (!result) return;
    std::free(result->response_body);
    std::free(result->effective_url);
    *result = {};
    result->entry_id = -1;
}

ecl_entry *ecl_entry_create(const char *category) {
    if (!category || !*category) return nullptr;
    try {
        auto entry = std::make_unique<ecl_entry>();
        entry->data.category = category;
        return entry.release();
    } catch (...) { return nullptr; }
}

void ecl_entry_destroy(ecl_entry *entry) { delete entry; }

ecl_status ecl_entry_set_subject(ecl_entry *entry, const char *subject) {
    if (!entry || !subject) return ECL_INVALID_ARGUMENT;
    try { entry->data.subject = subject; return ECL_OK; }
    catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_set_text(ecl_entry *entry, const char *text) {
    if (!entry || !text) return ECL_INVALID_ARGUMENT;
    try { entry->data.text = text; return ECL_OK; }
    catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_set_form(ecl_entry *entry, const char *form_name) {
    if (!entry || !form_name || !*form_name) return ECL_INVALID_ARGUMENT;
    try { entry->data.form = form_name; return ECL_OK; }
    catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_set_format(ecl_entry *entry, ecl_text_format format) {
    if (!entry || !valid_format(format)) return ECL_INVALID_ARGUMENT;
    entry->data.format = format;
    return ECL_OK;
}

ecl_status ecl_entry_set_private(ecl_entry *entry, int is_private) {
    if (!entry) return ECL_INVALID_ARGUMENT;
    entry->data.private_entry = is_private != 0;
    return ECL_OK;
}

ecl_status ecl_entry_set_related(ecl_entry *entry, int64_t entry_id) {
    if (!entry || entry_id <= 0) return ECL_INVALID_ARGUMENT;
    entry->data.related = entry_id;
    return ECL_OK;
}

ecl_status ecl_entry_add_tag(ecl_entry *entry, const char *tag) {
    if (!entry || !tag || !*tag) return ECL_INVALID_ARGUMENT;
    try { entry->data.tags.emplace_back(tag); return ECL_OK; }
    catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_add_field(ecl_entry *entry, const char *name, const char *value) {
    if (!entry || !name || !*name || !value) return ECL_INVALID_ARGUMENT;
    try { entry->data.fields.emplace_back(name, value); return ECL_OK; }
    catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

static ecl_status add_data(ecl_entry *entry, const char *name, const char *filename,
                           const void *data, size_t size, bool image) {
    if (!entry || !name || !*name || !filename || !*filename || (size > 0 && !data))
        return ECL_INVALID_ARGUMENT;
    try {
        Attachment attachment;
        attachment.name = name;
        attachment.filename = basename(filename);
        attachment.image = image;
        const auto *bytes = static_cast<const unsigned char *>(data);
        if (size > 0) attachment.data.assign(bytes, bytes + size);
        entry->data.attachments.push_back(std::move(attachment));
        return ECL_OK;
    } catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_add_attachment(ecl_entry *entry, const char *name, const char *filename,
                                    const void *data, size_t size) {
    return add_data(entry, name, filename, data, size, false);
}

ecl_status ecl_entry_add_attachment_file(ecl_entry *entry, const char *name, const char *path) {
    if (!entry || !name || !*name || !path || !*path) return ECL_INVALID_ARGUMENT;
    try {
        std::vector<unsigned char> data;
        std::string error;
        if (!read_file(path, data, error)) { entry->last_error = error; return ECL_IO_ERROR; }
        return add_data(entry, name, basename(path).c_str(), data.data(), data.size(), false);
    } catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_add_image(ecl_entry *entry, const char *name, const char *filename,
                               const void *data, size_t size) {
    return add_data(entry, name, filename, data, size, true);
}

ecl_status ecl_entry_add_image_file(ecl_entry *entry, const char *name, const char *path) {
    if (!entry || !name || !*name || !path || !*path) return ECL_INVALID_ARGUMENT;
    try {
        std::vector<unsigned char> data;
        std::string error;
        if (!read_file(path, data, error)) { entry->last_error = error; return ECL_IO_ERROR; }
        return add_data(entry, name, basename(path).c_str(), data.data(), data.size(), true);
    } catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

ecl_status ecl_entry_to_xml(const ecl_entry *entry, char **xml, size_t *size) {
    if (!entry || !xml || !size) return ECL_INVALID_ARGUMENT;
    *xml = nullptr;
    *size = 0;
    try {
        const std::string value = serialize(entry->data);
        *xml = copy_c_string(value);
        if (!*xml) return ECL_OUT_OF_MEMORY;
        *size = value.size();
        return ECL_OK;
    } catch (const std::bad_alloc &) { return ECL_OUT_OF_MEMORY; }
    catch (...) { return ECL_INTERNAL_ERROR; }
}

void ecl_free(void *memory) { std::free(memory); }

} // extern "C"
