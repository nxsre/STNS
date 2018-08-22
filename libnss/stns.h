#ifndef STNS_H
#define STNS_H
#define DEBUG 1

#include <curl/curl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "toml.h"
#include <syslog.h>
#include <nss.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <jansson.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#define STNS_VERSION "2.0.0"
#define STNS_VERSION_WITH_NAME "stns/" STNS_VERSION
// 10MB
#define STNS_MAX_BUFFER_SIZE (10 * 1024 * 1024)
#define STNS_CONFIG_FILE "/etc/stns/client/stns.conf"
#define MAXBUF 1024
#define STNS_LOCK_FILE "/var/tmp/.stns.lock"

typedef struct stns_http_response_t stns_http_response_t;
struct stns_http_response_t {
  char *data;
  size_t size;
  long *status_code;
};

typedef struct stns_conf_t stns_conf_t;
struct stns_conf_t {
  char *api_endpoint;
  char *auth_token;
  char *user;
  char *password;
  char *chain_ssh_wrapper;
  char *http_proxy;
  int uid_shift;
  int gid_shift;
  int ssl_verify;
  int request_timeout;
  int request_retry;
  int request_locktime;
};

extern void stns_load_config(char *, stns_conf_t *);
extern int stns_request(stns_conf_t *, char *, stns_http_response_t *);
extern int stns_request_available(char *, stns_conf_t *);
extern void stns_make_lockfile(char *);

#define STNS_ENSURE_BY(method_key, key_type, key_name, json_type, json_key, match_method, resource, ltype)             \
  enum nss_status ensure_##resource##_by_##method_key(char *data, stns_conf_t *c, key_type key_name,                   \
                                                      struct resource *rbuf, char *buf, size_t buflen, int *errnop)    \
  {                                                                                                                    \
    int i;                                                                                                             \
    json_error_t error;                                                                                                \
    json_t *leaf;                                                                                                      \
    json_t *root = json_loads(data, 0, &error);                                                                        \
                                                                                                                       \
    if (root == NULL) {                                                                                                \
      syslog(LOG_ERR, "%s[L%d] json parse error: %s", __func__, __LINE__, error.text);                                 \
      goto leave;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    json_array_foreach(root, i, leaf)                                                                                  \
    {                                                                                                                  \
      if (!json_is_object(leaf)) {                                                                                     \
        continue;                                                                                                      \
      }                                                                                                                \
      key_type current = json_##json_type##_value(json_object_get(leaf, #json_key));                                   \
                                                                                                                       \
      if (match_method) {                                                                                              \
        ltype##_ENSURE(leaf) free(data);                                                                               \
        json_decref(root);                                                                                             \
        return NSS_STATUS_SUCCESS;                                                                                     \
      }                                                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    free(data);                                                                                                        \
    json_decref(root);                                                                                                 \
    return NSS_STATUS_NOTFOUND;                                                                                        \
  leave:                                                                                                               \
    return NSS_STATUS_UNAVAIL;                                                                                         \
  }

#define STNS_SET_DEFAULT_VALUE(buf, name, def)                                                                         \
  char buf[MAXBUF];                                                                                                    \
  if (name != NULL && strlen(name) > 0) {                                                                              \
    strcpy(buf, name);                                                                                                 \
  } else {                                                                                                             \
    strcpy(buf, def);                                                                                                  \
  }                                                                                                                    \
  name = buf;

#define STNS_GET_SINGLE_VALUE_METHOD(method, first, format, value, resource)                                           \
  enum nss_status _nss_stns_##method(first, struct resource *rbuf, char *buf, size_t buflen, int *errnop)              \
  {                                                                                                                    \
    int curl_result;                                                                                                   \
    stns_http_response_t r;                                                                                            \
    stns_conf_t c;                                                                                                     \
    char url[MAXBUF];                                                                                                  \
                                                                                                                       \
    stns_load_config(STNS_CONFIG_FILE, &c);                                                                            \
                                                                                                                       \
    sprintf(url, format, value);                                                                                       \
    curl_result = stns_request(&c, url, &r);                                                                           \
                                                                                                                       \
    if (curl_result != CURLE_OK) {                                                                                     \
      return NSS_STATUS_UNAVAIL;                                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    return ensure_##resource##_by_##value(r.data, &c, value, rbuf, buf, buflen, errnop);                               \
  }

#define SET_ATTRBUTE(type, name, attr)                                                                                 \
  int name##_length = strlen(name) + 1;                                                                                \
                                                                                                                       \
  if (buflen < name##_length) {                                                                                        \
    *errnop = ERANGE;                                                                                                  \
    return NSS_STATUS_TRYAGAIN;                                                                                        \
  }                                                                                                                    \
                                                                                                                       \
  strcpy(buf, name);                                                                                                   \
  rbuf->type##_##attr = buf;                                                                                           \
  buf += name##_length;                                                                                                \
  buflen -= name##_length;

#define STNS_SET_ENTRIES(type, ltype, resource, query)                                                                 \
  pthread_mutex_t type##ent_mutex = PTHREAD_MUTEX_INITIALIZER;                                                         \
  enum nss_status inner_nss_stns_set##type##ent(char *data, stns_conf_t *c)                                            \
  {                                                                                                                    \
    pthread_mutex_lock(&type##ent_mutex);                                                                              \
    json_error_t error;                                                                                                \
                                                                                                                       \
    entries = json_loads(data, 0, &error);                                                                             \
                                                                                                                       \
    if (entries == NULL) {                                                                                             \
      syslog(LOG_ERR, "%s[L%d] json parse error: %s", __func__, __LINE__, error.text);                                 \
      free(data);                                                                                                      \
      pthread_mutex_unlock(&type##ent_mutex);                                                                          \
      return NSS_STATUS_UNAVAIL;                                                                                       \
    }                                                                                                                  \
    entry_idx = 0;                                                                                                     \
                                                                                                                       \
    pthread_mutex_unlock(&type##ent_mutex);                                                                            \
    return NSS_STATUS_SUCCESS;                                                                                         \
  }                                                                                                                    \
                                                                                                                       \
  enum nss_status _nss_stns_set##type##ent(void)                                                                       \
  {                                                                                                                    \
    int curl_result;                                                                                                   \
    stns_http_response_t r;                                                                                            \
    stns_conf_t c;                                                                                                     \
    stns_load_config(STNS_CONFIG_FILE, &c);                                                                            \
                                                                                                                       \
    curl_result = stns_request(&c, #query, &r);                                                                        \
    if (curl_result != CURLE_OK) {                                                                                     \
      return NSS_STATUS_UNAVAIL;                                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    return inner_nss_stns_set##type##ent(r.data, &c);                                                                  \
  }                                                                                                                    \
                                                                                                                       \
  enum nss_status _nss_stns_end##type##ent(void)                                                                       \
  {                                                                                                                    \
    pthread_mutex_lock(&type##ent_mutex);                                                                              \
    json_decref(entries);                                                                                              \
    entry_idx = 0;                                                                                                     \
    pthread_mutex_unlock(&type##ent_mutex);                                                                            \
    return NSS_STATUS_SUCCESS;                                                                                         \
  }                                                                                                                    \
                                                                                                                       \
  enum nss_status inner_nss_stns_get##type##ent_r(stns_conf_t *c, struct resource *rbuf, char *buf, size_t buflen,     \
                                                  int *errnop)                                                         \
  {                                                                                                                    \
    enum nss_status ret = NSS_STATUS_SUCCESS;                                                                          \
    pthread_mutex_lock(&type##ent_mutex);                                                                              \
                                                                                                                       \
    if (entries == NULL) {                                                                                             \
      ret = _nss_stns_set##type##ent();                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    if (ret != NSS_STATUS_SUCCESS) {                                                                                   \
      pthread_mutex_unlock(&type##ent_mutex);                                                                          \
      return ret;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    if (entry_idx >= json_array_size(entries)) {                                                                       \
      *errnop = ENOENT;                                                                                                \
      pthread_mutex_unlock(&type##ent_mutex);                                                                          \
      return NSS_STATUS_NOTFOUND;                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    json_t *user = json_array_get(entries, entry_idx);                                                                 \
                                                                                                                       \
    ltype##_ENSURE(user);                                                                                              \
    entry_idx++;                                                                                                       \
    pthread_mutex_unlock(&type##ent_mutex);                                                                            \
    return NSS_STATUS_SUCCESS;                                                                                         \
  }                                                                                                                    \
                                                                                                                       \
  enum nss_status _nss_stns_get##type##ent_r(struct resource *rbuf, char *buf, size_t buflen, int *errnop)             \
  {                                                                                                                    \
    stns_conf_t c;                                                                                                     \
    stns_load_config(STNS_CONFIG_FILE, &c);                                                                            \
    return inner_nss_stns_get##type##ent_r(&c, rbuf, buf, buflen, errnop);                                             \
  }

#endif /* STNS_H */
