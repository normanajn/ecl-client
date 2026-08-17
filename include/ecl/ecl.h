#ifndef ECL_ECL_H
#define ECL_ECL_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(ECL_SHARED)
#  if defined(ECL_BUILDING_LIBRARY)
#    define ECL_API __declspec(dllexport)
#  else
#    define ECL_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define ECL_API __attribute__((visibility("default")))
#else
#  define ECL_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecl_client ecl_client;
typedef struct ecl_entry ecl_entry;

typedef enum ecl_status {
    ECL_OK = 0,
    ECL_INVALID_ARGUMENT = 1,
    ECL_IO_ERROR = 2,
    ECL_HTTP_ERROR = 3,
    ECL_PROTOCOL_ERROR = 4,
    ECL_OUT_OF_MEMORY = 5,
    ECL_INTERNAL_ERROR = 6
} ecl_status;

typedef enum ecl_auth_mode {
    ECL_AUTH_XML_SIGNATURE = 0,
    ECL_AUTH_PASSWORD = 1
} ecl_auth_mode;

typedef enum ecl_text_format {
    ECL_FORMAT_PLAIN = 0,
    ECL_FORMAT_TEXTILE = 1,
    ECL_FORMAT_PREFORMATTED = 2
} ecl_text_format;

typedef struct ecl_post_result {
    long http_status;
    int64_t entry_id;
    char *response_body;
    char *effective_url;
} ecl_post_result;

ECL_API const char *ecl_version(void);
ECL_API const char *ecl_status_string(ecl_status status);

ECL_API ecl_client *ecl_client_create(const char *instance_url,
                                      const char *username,
                                      const char *password,
                                      ecl_auth_mode auth_mode);
ECL_API void ecl_client_destroy(ecl_client *client);
ECL_API ecl_status ecl_client_set_timeout(ecl_client *client, long seconds);
ECL_API ecl_status ecl_client_set_connect_timeout(ecl_client *client, long seconds);
ECL_API ecl_status ecl_client_set_tls_verify(ecl_client *client, int enabled);
ECL_API ecl_status ecl_client_set_user_agent(ecl_client *client, const char *user_agent);
ECL_API const char *ecl_client_last_error(const ecl_client *client);
ECL_API ecl_status ecl_client_post(ecl_client *client,
                                   const ecl_entry *entry,
                                   ecl_post_result *result);
ECL_API void ecl_post_result_free(ecl_post_result *result);

ECL_API ecl_entry *ecl_entry_create(const char *category);
ECL_API void ecl_entry_destroy(ecl_entry *entry);
ECL_API ecl_status ecl_entry_set_subject(ecl_entry *entry, const char *subject);
ECL_API ecl_status ecl_entry_set_text(ecl_entry *entry, const char *text);
ECL_API ecl_status ecl_entry_set_form(ecl_entry *entry, const char *form_name);
ECL_API ecl_status ecl_entry_set_format(ecl_entry *entry, ecl_text_format format);
ECL_API ecl_status ecl_entry_set_private(ecl_entry *entry, int is_private);
ECL_API ecl_status ecl_entry_set_related(ecl_entry *entry, int64_t entry_id);
ECL_API ecl_status ecl_entry_add_tag(ecl_entry *entry, const char *tag);
ECL_API ecl_status ecl_entry_add_field(ecl_entry *entry,
                                       const char *name,
                                       const char *value);
ECL_API ecl_status ecl_entry_add_attachment(ecl_entry *entry,
                                            const char *name,
                                            const char *filename,
                                            const void *data,
                                            size_t size);
ECL_API ecl_status ecl_entry_add_attachment_file(ecl_entry *entry,
                                                 const char *name,
                                                 const char *path);
ECL_API ecl_status ecl_entry_add_image(ecl_entry *entry,
                                       const char *name,
                                       const char *filename,
                                       const void *data,
                                       size_t size);
ECL_API ecl_status ecl_entry_add_image_file(ecl_entry *entry,
                                            const char *name,
                                            const char *path);
ECL_API ecl_status ecl_entry_to_xml(const ecl_entry *entry, char **xml, size_t *size);
ECL_API void ecl_free(void *memory);

#ifdef __cplusplus
}
#endif

#endif
