#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>

#define AUTH_USERNAME_LEN 64
#define AUTH_NAME_LEN 96
#define AUTH_PASSWORD_MAX 128
#define AUTH_MAX_USERS 100
#define AUTH_MAX_SESSIONS 200
#define AUTH_TOKEN_LEN 65

typedef enum {
    AUTH_ROLE_VIEWER,
    AUTH_ROLE_ADMIN
} AuthRole;

typedef struct {
    int id;
    int active;
    AuthRole role;
    char username[AUTH_USERNAME_LEN];
    char name[AUTH_NAME_LEN];
    unsigned char salt[16];
    unsigned char password_hash[32];
} AuthUser;

typedef struct {
    int active;
    int user_id;
    char token[AUTH_TOKEN_LEN];
} AuthSession;

typedef struct {
    int next_user_id;
    size_t user_count;
    AuthUser users[AUTH_MAX_USERS];
    AuthSession sessions[AUTH_MAX_SESSIONS];
    char storage_path[260];
} AuthStore;

int auth_store_load(AuthStore *store, const char *path, char *err, size_t errlen);
int auth_store_save(const AuthStore *store, char *err, size_t errlen);
int auth_setup_required(const AuthStore *store);
AuthUser *auth_find_user(AuthStore *store, int id);
AuthUser *auth_find_username(AuthStore *store, const char *username);
AuthUser *auth_find_session(AuthStore *store, const char *token);
int auth_create_user(
    AuthStore *store,
    const char *username,
    const char *name,
    const char *password,
    AuthRole role,
    AuthUser **created,
    char *err,
    size_t errlen
);
int auth_verify_password(const AuthUser *user, const char *password);
int auth_create_session(AuthStore *store, int user_id, char token[AUTH_TOKEN_LEN]);
void auth_revoke_session(AuthStore *store, const char *token);
int auth_update_user(
    AuthStore *store,
    AuthUser *user,
    const char *name,
    const char *password,
    int change_role,
    AuthRole role,
    char *err,
    size_t errlen
);
int auth_delete_user(
    AuthStore *store,
    AuthUser *user,
    char *err,
    size_t errlen
);
char *auth_users_json(const AuthStore *store);
char *auth_user_json(const AuthUser *user);
char *auth_session_json(const AuthUser *user, const char *token);
char *auth_status_json(int setup_required);

#endif
