#include "stns.h"

static json_t *entries = NULL;
static int entry_idx   = 0;

#define SHADOW_DEFAULT(buf, name, def)                                                                                 \
  char buf[MAXBUF];                                                                                                    \
  if (name != NULL && strlen(name) > 0) {                                                                              \
    strcpy(buf, name);                                                                                                 \
  } else {                                                                                                             \
    strcpy(buf, def);                                                                                                  \
  }                                                                                                                    \
  name = buf;

#define SHADOW_GET_SINGLE(method, first, format, value)                                                                \
  enum nss_status _nss_stns_##method(first, struct spwd *rbuf, char *buf, size_t buflen, int *errnop)                  \
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
    return ensure_spwd_by_##value(r.data, &c, value, rbuf, buf, buflen, errnop);                                       \
  }

#define SHADOW_SET_ATTRBUTE(name, sp_name)                                                                             \
  int name##_length = strlen(name) + 1;                                                                                \
                                                                                                                       \
  if (buflen < name##_length) {                                                                                        \
    *errnop = ERANGE;                                                                                                  \
    return NSS_STATUS_TRYAGAIN;                                                                                        \
  }                                                                                                                    \
                                                                                                                       \
  strcpy(buf, name);                                                                                                   \
  rbuf->sp_##sp_name = buf;                                                                                            \
  buf += name##_length;                                                                                                \
  buflen -= name##_length;

#define SHADOW_ENSURE(user)                                                                                            \
  const char *name     = json_string_value(json_object_get(user, "name"));                                             \
  const char *password = json_string_value(json_object_get(user, "password"));                                         \
  SHADOW_SET_ATTRBUTE(name, namp);                                                                                     \
  SHADOW_DEFAULT(pw, password, "!!");                                                                                  \
  SHADOW_SET_ATTRBUTE(password, pwdp);                                                                                 \
  rbuf->sp_lstchg = -1;                                                                                                \
  rbuf->sp_min    = -1;                                                                                                \
  rbuf->sp_max    = -1;                                                                                                \
  rbuf->sp_warn   = -1;                                                                                                \
  rbuf->sp_inact  = -1;                                                                                                \
  rbuf->sp_expire = -1;                                                                                                \
  rbuf->sp_flag   = ~0ul;

#define SHADOW_ENSURE_BY(method_key, key_type, key_name, json_type, json_key, match_method)                            \
  enum nss_status ensure_spwd_by_##method_key(char *data, stns_conf_t *c, key_type key_name, struct spwd *rbuf,        \
                                              char *buf, size_t buflen, int *errnop)                                   \
  {                                                                                                                    \
    int i;                                                                                                             \
    json_error_t error;                                                                                                \
    json_t *user;                                                                                                      \
    json_t *users = json_loads(data, 0, &error);                                                                       \
                                                                                                                       \
    if (users == NULL) {                                                                                               \
      syslog(LOG_ERR, "%s[L%d] json parse error: %s", __func__, __LINE__, error.text);                                 \
      goto leave;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    json_array_foreach(users, i, user)                                                                                 \
    {                                                                                                                  \
      if (!json_is_object(user)) {                                                                                     \
        continue;                                                                                                      \
      }                                                                                                                \
      key_type current = json_##json_type##_value(json_object_get(user, #json_key));                                   \
                                                                                                                       \
      if (match_method) {                                                                                              \
        SHADOW_ENSURE(user)                                                                                            \
        free(data);                                                                                                    \
        json_decref(users);                                                                                            \
        return NSS_STATUS_SUCCESS;                                                                                     \
      }                                                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    free(data);                                                                                                        \
    json_decref(users);                                                                                                \
    return NSS_STATUS_NOTFOUND;                                                                                        \
  leave:                                                                                                               \
    return NSS_STATUS_UNAVAIL;                                                                                         \
  }

SHADOW_ENSURE_BY(name, const char *, user_name, string, name, (strcmp(current, user_name) == 0))
SHADOW_ENSURE_BY(uid, uid_t, uid, integer, id, current == uid)

SHADOW_GET_SINGLE(getspnam_r, const char *name, "users?name=%s", name)
SHADOW_GET_SINGLE(getspuid_r, uid_t uid, "users?id=%d", uid)
STNS_SET_ENTRIES(sp, SHADOW, spwd, users)
