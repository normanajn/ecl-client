#include <ecl/ecl.hpp>

#include <getopt.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Options {
    std::string url;
    std::string instance;
    std::string username;
    std::string password;
    std::string auth = "xml";
    std::string category;
    std::string subject;
    std::string text;
    std::string text_file;
    std::string form = "default";
    std::string format = "plain";
    std::string config_path;
    std::vector<std::string> tags;
    std::vector<std::string> fields;
    std::vector<std::string> attachments;
    std::vector<std::string> images;
    std::int64_t related = 0;
    long timeout = 30;
    long connect_timeout = 10;
    bool private_entry = false;
    bool insecure = false;
    bool dry_run = false;
};

void usage(std::ostream &out) {
    out << R"(Usage: ecl-post [OPTIONS]

Post an entry to a Fermilab Electronic Logbook.

Connection and authentication:
  -i, --instance NAME         ECL instance alias, for example "demo"
  -U, --url URL              Full ECL instance URL
  -u, --username USER        ECL API username
      --auth xml|password    Authentication mode (default: xml)
      --config PATH          Configuration file
      --timeout SECONDS      Overall request timeout (default: 30)
      --connect-timeout SEC  Connection timeout (default: 10)
      --insecure             Disable TLS certificate verification

Entry:
  -c, --category PATH        Category path (required)
  -s, --subject TEXT         Entry subject
  -t, --text TEXT            Entry body
  -T, --text-file PATH       Read body from PATH; use - for stdin
  -f, --form NAME            Form name (default: default)
      --format FORMAT        plain, textile, or preformatted
      --tag TAG              Add a tag; repeat as needed
      --field NAME=VALUE     Set a form field; repeat as needed
      --attachment [NAME=]PATH
                             Attach a file; repeat as needed
      --image [NAME=]PATH    Attach an image; repeat as needed
      --related ID           Relate this entry to an existing entry
      --private              Mark the entry private
  -n, --dry-run              Print XML without posting
  -h, --help                 Show this help
      --version              Show the version

Secrets are read from ECL_PASSWORD, a mode-0600 config file, or a terminal
prompt. There is deliberately no password command-line option.
)";
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string default_config_path() {
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME")) {
        if (*xdg) return std::string(xdg) + "/ecl-client/config";
    }
    if (const char *home = std::getenv("HOME")) {
        if (*home) return std::string(home) + "/.config/ecl-client/config";
    }
    return {};
}

bool file_exists(const std::string &path) {
    struct stat info {};
    return !path.empty() && stat(path.c_str(), &info) == 0;
}

std::map<std::string, std::string> read_config(const std::string &path, bool required) {
    std::map<std::string, std::string> values;
    if (path.empty() || !file_exists(path)) {
        if (required) throw std::runtime_error("configuration file not found: " + path);
        return values;
    }
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open configuration file: " + path);
    std::string line;
    std::size_t number = 0;
    while (std::getline(input, line)) {
        ++number;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(number) + ": expected key=value");
        }
        const std::string key = trim(line.substr(0, equal));
        const std::string value = trim(line.substr(equal + 1));
        if (key.empty()) throw std::runtime_error(path + ":" + std::to_string(number) + ": empty key");
        values[key] = value;
    }
    if (values.count("password")) {
        struct stat info {};
        if (stat(path.c_str(), &info) != 0) {
            throw std::runtime_error("cannot inspect configuration permissions: " + path);
        }
        if ((info.st_mode & 0077) != 0) {
            throw std::runtime_error("configuration containing a password must not be accessible by group or others: " + path);
        }
    }
    return values;
}

void apply_config(Options &options, const std::map<std::string, std::string> &config) {
    auto set = [&config](const char *name, std::string &destination) {
        const auto found = config.find(name);
        if (found != config.end()) destination = found->second;
    };
    set("url", options.url);
    set("instance", options.instance);
    set("username", options.username);
    set("password", options.password);
    set("auth", options.auth);
    set("category", options.category);
    set("form", options.form);
    set("format", options.format);
    if (const auto found = config.find("timeout"); found != config.end()) options.timeout = std::stol(found->second);
    if (const auto found = config.find("connect_timeout"); found != config.end()) options.connect_timeout = std::stol(found->second);
}

void apply_environment(Options &options) {
    auto set = [](const char *name, std::string &destination) {
        if (const char *value = std::getenv(name)) destination = value;
    };
    set("ECL_URL", options.url);
    set("ECL_INSTANCE", options.instance);
    set("ECL_USERNAME", options.username);
    set("ECL_PASSWORD", options.password);
    set("ECL_AUTH", options.auth);
    set("ECL_CATEGORY", options.category);
    set("ECL_FORM", options.form);
}

std::string prescan_config(int argc, char **argv, bool &explicit_path) {
    std::string path = default_config_path();
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--config") {
            if (i + 1 >= argc) throw std::runtime_error("--config requires a path");
            path = argv[++i];
            explicit_path = true;
        } else if (argument.rfind("--config=", 0) == 0) {
            path = argument.substr(9);
            explicit_path = true;
        }
    }
    return path;
}

long parse_positive_long(const char *value, const char *option) {
    try {
        std::size_t used = 0;
        const long result = std::stol(value, &used);
        if (used != std::strlen(value) || result <= 0) throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw std::runtime_error(std::string(option) + " requires a positive integer");
    }
}

std::int64_t parse_positive_id(const char *value) {
    try {
        std::size_t used = 0;
        const auto result = std::stoll(value, &used);
        if (used != std::strlen(value) || result <= 0) throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw std::runtime_error("--related requires a positive entry ID");
    }
}

void parse_cli(int argc, char **argv, Options &options) {
    enum { OPT_AUTH = 1000, OPT_CONFIG, OPT_TIMEOUT, OPT_CONNECT_TIMEOUT, OPT_INSECURE,
           OPT_FORMAT, OPT_TAG, OPT_FIELD, OPT_ATTACHMENT, OPT_IMAGE, OPT_RELATED,
           OPT_PRIVATE, OPT_VERSION };
    static const option long_options[] = {
        {"instance", required_argument, nullptr, 'i'}, {"url", required_argument, nullptr, 'U'},
        {"username", required_argument, nullptr, 'u'}, {"auth", required_argument, nullptr, OPT_AUTH},
        {"config", required_argument, nullptr, OPT_CONFIG}, {"timeout", required_argument, nullptr, OPT_TIMEOUT},
        {"connect-timeout", required_argument, nullptr, OPT_CONNECT_TIMEOUT},
        {"insecure", no_argument, nullptr, OPT_INSECURE}, {"category", required_argument, nullptr, 'c'},
        {"subject", required_argument, nullptr, 's'}, {"text", required_argument, nullptr, 't'},
        {"text-file", required_argument, nullptr, 'T'}, {"form", required_argument, nullptr, 'f'},
        {"format", required_argument, nullptr, OPT_FORMAT}, {"tag", required_argument, nullptr, OPT_TAG},
        {"field", required_argument, nullptr, OPT_FIELD}, {"attachment", required_argument, nullptr, OPT_ATTACHMENT},
        {"image", required_argument, nullptr, OPT_IMAGE}, {"related", required_argument, nullptr, OPT_RELATED},
        {"private", no_argument, nullptr, OPT_PRIVATE}, {"dry-run", no_argument, nullptr, 'n'},
        {"help", no_argument, nullptr, 'h'}, {"version", no_argument, nullptr, OPT_VERSION},
        {nullptr, 0, nullptr, 0}
    };
    optind = 1;
    while (true) {
        const int choice = getopt_long(argc, argv, "i:U:u:c:s:t:T:f:nh", long_options, nullptr);
        if (choice == -1) break;
        switch (choice) {
        case 'i': options.instance = optarg; break;
        case 'U': options.url = optarg; break;
        case 'u': options.username = optarg; break;
        case 'c': options.category = optarg; break;
        case 's': options.subject = optarg; break;
        case 't': options.text = optarg; break;
        case 'T': options.text_file = optarg; break;
        case 'f': options.form = optarg; break;
        case 'n': options.dry_run = true; break;
        case 'h': usage(std::cout); std::exit(0);
        case OPT_AUTH: options.auth = optarg; break;
        case OPT_CONFIG: options.config_path = optarg; break;
        case OPT_TIMEOUT: options.timeout = parse_positive_long(optarg, "--timeout"); break;
        case OPT_CONNECT_TIMEOUT: options.connect_timeout = parse_positive_long(optarg, "--connect-timeout"); break;
        case OPT_INSECURE: options.insecure = true; break;
        case OPT_FORMAT: options.format = optarg; break;
        case OPT_TAG: options.tags.emplace_back(optarg); break;
        case OPT_FIELD: options.fields.emplace_back(optarg); break;
        case OPT_ATTACHMENT: options.attachments.emplace_back(optarg); break;
        case OPT_IMAGE: options.images.emplace_back(optarg); break;
        case OPT_RELATED: options.related = parse_positive_id(optarg); break;
        case OPT_PRIVATE: options.private_entry = true; break;
        case OPT_VERSION: std::cout << "ecl-post " << ecl_version() << '\n'; std::exit(0);
        default: throw std::runtime_error("invalid command-line arguments; use --help");
        }
    }
    if (optind != argc) throw std::runtime_error("unexpected positional argument: " + std::string(argv[optind]));
}

std::string read_stream(std::istream &input) {
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) throw std::runtime_error("failed while reading entry text");
    return contents.str();
}

std::string read_text_file(const std::string &path) {
    if (path == "-") return read_stream(std::cin);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open text file: " + path);
    return read_stream(input);
}

std::pair<std::string, std::string> split_assignment(const std::string &value,
                                                     const std::string &option,
                                                     bool name_optional) {
    const auto equal = value.find('=');
    if (equal == std::string::npos) {
        if (!name_optional) throw std::runtime_error(option + " requires NAME=VALUE");
        const auto slash = value.find_last_of("/\\");
        const std::string name = slash == std::string::npos ? value : value.substr(slash + 1);
        if (name.empty()) throw std::runtime_error(option + " has an empty filename");
        return {name, value};
    }
    if (equal == 0) throw std::runtime_error(option + " has an empty name");
    return {value.substr(0, equal), value.substr(equal + 1)};
}

std::string prompt_password() {
    FILE *terminal = std::fopen("/dev/tty", "r+");
    if (!terminal) throw std::runtime_error("no password provided and no terminal is available");
    const int descriptor = fileno(terminal);
    termios old_settings {};
    if (tcgetattr(descriptor, &old_settings) != 0) {
        std::fclose(terminal);
        throw std::runtime_error("cannot configure terminal for password input");
    }
    termios new_settings = old_settings;
    new_settings.c_lflag &= static_cast<tcflag_t>(~ECHO);
    std::fputs("ECL password: ", terminal);
    std::fflush(terminal);
    if (tcsetattr(descriptor, TCSAFLUSH, &new_settings) != 0) {
        std::fclose(terminal);
        throw std::runtime_error("cannot disable terminal echo");
    }
    char buffer[4096] = {};
    const bool read_ok = std::fgets(buffer, sizeof(buffer), terminal) != nullptr;
    tcsetattr(descriptor, TCSAFLUSH, &old_settings);
    std::fputc('\n', terminal);
    std::fclose(terminal);
    if (!read_ok) throw std::runtime_error("could not read password");
    std::string password(buffer);
    while (!password.empty() && (password.back() == '\n' || password.back() == '\r')) password.pop_back();
    volatile char *wipe = buffer;
    for (std::size_t i = 0; i < sizeof(buffer); ++i) wipe[i] = 0;
    if (password.empty()) throw std::runtime_error("password cannot be empty");
    return password;
}

ecl::TextFormat parse_format(const std::string &value) {
    if (value == "plain") return ecl::TextFormat::Plain;
    if (value == "textile") return ecl::TextFormat::Textile;
    if (value == "preformatted") return ecl::TextFormat::Preformatted;
    throw std::runtime_error("--format must be plain, textile, or preformatted");
}

ecl::AuthMode parse_auth(const std::string &value) {
    if (value == "xml") return ecl::AuthMode::XmlSignature;
    if (value == "password") return ecl::AuthMode::Password;
    throw std::runtime_error("--auth must be xml or password");
}

void validate(Options &options) {
    if (options.category.empty()) throw std::runtime_error("--category is required");
    if (!options.text_file.empty() && !options.text.empty()) {
        throw std::runtime_error("use only one of --text and --text-file");
    }
    if (!options.text_file.empty()) options.text = read_text_file(options.text_file);
    if (options.url.empty() && !options.instance.empty()) options.url = ecl::instance_url(options.instance);
    if (!options.dry_run) {
        if (options.url.empty()) throw std::runtime_error("--url or --instance is required");
        if (options.username.empty()) throw std::runtime_error("--username is required");
        if (options.password.empty()) options.password = prompt_password();
        if (options.auth == "password" && options.url.rfind("https://", 0) != 0) {
            throw std::runtime_error("password authentication requires an HTTPS URL");
        }
    }
    if (options.timeout <= 0 || options.connect_timeout <= 0) {
        throw std::runtime_error("timeouts must be positive");
    }
    (void)parse_auth(options.auth);
    (void)parse_format(options.format);
}

} // namespace

int main(int argc, char **argv) {
    try {
        bool explicit_config = false;
        const std::string config_path = prescan_config(argc, argv, explicit_config);
        Options options;
        options.config_path = config_path;
        apply_config(options, read_config(config_path, explicit_config));
        apply_environment(options);
        parse_cli(argc, argv, options);
        validate(options);

        ecl::Entry entry(options.category);
        entry.subject(options.subject).text(options.text).form(options.form).format(parse_format(options.format));
        if (options.private_entry) entry.private_entry();
        if (options.related > 0) entry.related(options.related);
        for (const auto &tag : options.tags) entry.tag(tag);
        for (const auto &field : options.fields) {
            const auto item = split_assignment(field, "--field", false);
            entry.field(item.first, item.second);
        }
        for (const auto &attachment : options.attachments) {
            const auto item = split_assignment(attachment, "--attachment", true);
            entry.attachment(item.first, item.second);
        }
        for (const auto &image : options.images) {
            const auto item = split_assignment(image, "--image", true);
            entry.image(item.first, item.second);
        }

        if (options.dry_run) {
            std::cout << entry.xml() << '\n';
            return 0;
        }

        ecl::Client client(options.url, options.username, options.password, parse_auth(options.auth));
        client.timeout(options.timeout).connect_timeout(options.connect_timeout).tls_verify(!options.insecure);
        const ecl::PostResult result = client.post(entry);
        std::cout << "Created ECL entry " << result.entry_id << '\n';
        std::cout << "Endpoint: " << result.effective_url << '\n';
        return 0;
    } catch (const ecl::Error &error) {
        std::cerr << "ecl-post: " << error.what() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "ecl-post: " << error.what() << '\n';
        return 2;
    }
}
