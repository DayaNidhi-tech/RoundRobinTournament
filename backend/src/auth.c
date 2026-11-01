#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "auth.h"

#define AUTH_FILE_VERSION 1
#define PASSWORD_HASH_ITERATIONS 310000

typedef LONG NTSTATUS;
typedef NTSTATUS (WINAPI *BCryptOpenAlgorithmProviderFn)(
    void **,
    LPCWSTR,
    LPCWSTR,
    ULONG
);
typedef NTSTATUS (WINAPI *BCryptDeriveKeyPBKDF2Fn)(
    void *,
    PUCHAR,
    ULONG,
    PUCHAR,
    ULONG,
    ULONGLONG,
    PUCHAR,
    ULONG,
    ULONG
);
typedef NTSTATUS (WINAPI *BCryptGenRandomFn)(
    void *,
    PUCHAR,
    ULONG,
    ULONG
);
typedef NTSTATUS (WINAPI *BCryptCloseAlgorithmProviderFn)(void *, ULONG);

typedef struct {
    unsigned int version;
    unsigned int count;
    int next_user_id;
    AuthUser users[AUTH_MAX_USERS];
} AuthDisk;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} JsonBuffer;

static void set_error(char *err, size_t errlen, const char *message) {
    if (err != NULL && errlen > 0) {
        snprintf(err, errlen, "%s", message);
    }
}

static int crypto_functions(
    HMODULE *library,
    BCryptOpenAlgorithmProviderFn *open_provider,
    BCryptDeriveKeyPBKDF2Fn *derive_key,
    BCryptGenRandomFn *generate_random,
    BCryptCloseAlgorithmProviderFn *close_provider
) {
    *library = LoadLibraryW(L"bcrypt.dll");
    if (*library == NULL) {
        return 0;
    }

    *open_provider = (BCryptOpenAlgorithmProviderFn)GetProcAddress(
        *library,
        "BCryptOpenAlgorithmProvider"
    );
    *derive_key = (BCryptDeriveKeyPBKDF2Fn)GetProcAddress(
        *library,
        "BCryptDeriveKeyPBKDF2"
    );
    *generate_random = (BCryptGenRandomFn)GetProcAddress(
        *library,
        "BCryptGenRandom"
    );
    *close_provider = (BCryptCloseAlgorithmProviderFn)GetProcAddress(
        *library,
        "BCryptCloseAlgorithmProvider"
    );

    if (
        *open_provider == NULL ||
        *derive_key == NULL ||
        *generate_random == NULL ||
        *close_provider == NULL
    ) {
        FreeLibrary(*library);
        *library = NULL;
        return 0;
    }

    return 1;
}

static int random_bytes(unsigned char *bytes, size_t length) {
    HMODULE library = NULL;
    BCryptOpenAlgorithmProviderFn open_provider = NULL;
    BCryptDeriveKeyPBKDF2Fn derive_key = NULL;
    BCryptGenRandomFn generate_random = NULL;
    BCryptCloseAlgorithmProviderFn close_provider = NULL;
    int ok = 0;

    if (!crypto_functions(
            &library,
            &open_provider,
            &derive_key,
            &generate_random,
            &close_provider
        )) {
        return 0;
    }

    if (length <= 0xffffffffu) {
        ok = generate_random(
            NULL,
            bytes,
            (ULONG)length,
            0x00000002
        ) >= 0;
    }

    FreeLibrary(library);
    return ok;
}

static int password_hash(
    const unsigned char *salt,
    const char *password,
    unsigned char output[32]
) {
    HMODULE library = NULL;
    BCryptOpenAlgorithmProviderFn open_provider = NULL;
    BCryptDeriveKeyPBKDF2Fn derive_key = NULL;
    BCryptGenRandomFn generate_random = NULL;
    BCryptCloseAlgorithmProviderFn close_provider = NULL;
    void *algorithm = NULL;
    int ok = 0;

    if (!crypto_functions(
            &library,
            &open_provider,
            &derive_key,
            &generate_random,
            &close_provider
        )) {
        return 0;
    }

    {
        NTSTATUS status = open_provider(
            &algorithm,
            L"SHA256",
            NULL,
            0x00000008
        );
        if (status >= 0) {
            status = derive_key(
                algorithm,
                (PUCHAR)password,
                (ULONG)strlen(password),
                (PUCHAR)salt,
                16,
                PASSWORD_HASH_ITERATIONS,
                output,
                32,
                0
            );
        }
        ok = status >= 0;
    }

    if (algorithm != NULL) {
        close_provider(algorithm, 0);
    }
    FreeLibrary(library);
    return ok;
}

static int secure_equal(
    const unsigned char *left,
    const unsigned char *right,
    size_t length
) {
    unsigned char difference = 0;
    size_t i;

    for (i = 0; i < length; i++) {
        difference |= left[i] ^ right[i];
    }

    return difference == 0;
}

static int username_normalize(const char *input, char output[AUTH_USERNAME_LEN]) {
    size_t i;
    size_t length;

    if (input == NULL) {
        return 0;
    }

    length = strlen(input);
    if (length < 3 || length >= AUTH_USERNAME_LEN) {
        return 0;
    }

    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)input[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) {
            return 0;
        }
        output[i] = (char)tolower(c);
    }
    output[length] = 0;
    return 1;
}

static int set_password(AuthUser *user, const char *password) {
    size_t length;

    if (password == NULL) {
        return 0;
    }
    length = strlen(password);
    if (length < 8 || length > AUTH_PASSWORD_MAX) {
        return 0;
    }

    return random_bytes(user->salt, sizeof(user->salt)) &&
        password_hash(user->salt, password, user->password_hash);
}

static AuthUser *find_username(AuthStore *store, const char *username) {
    char normalized[AUTH_USERNAME_LEN];
    size_t i;

    if (!username_normalize(username, normalized)) {
        return NULL;
    }
    for (i = 0; i < store->user_count; i++) {
        AuthUser *user = &store->users[i];
        if (user->active && strcmp(user->username, normalized) == 0) {
            return user;
        }
    }
    return NULL;
}

AuthUser *auth_find_username(AuthStore *store, const char *username) {
    return find_username(store, username);
}

static int admin_count(const AuthStore *store) {
    size_t i;
    int count = 0;

    for (i = 0; i < store->user_count; i++) {
        if (store->users[i].active && store->users[i].role == AUTH_ROLE_ADMIN) {
            count++;
        }
    }
    return count;
}

int auth_store_load(AuthStore *store, const char *path, char *err, size_t errlen) {
    AuthDisk disk;
    FILE *file;

    memset(store, 0, sizeof(*store));
    store->next_user_id = 1;
    if (path == NULL || strlen(path) >= sizeof(store->storage_path)) {
        set_error(err, errlen, "Invalid user data path");
        return 0;
    }
    strcpy(store->storage_path, path);

    file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            return 1;
        }
        set_error(err, errlen, "Could not open user data file");
        return 0;
    }

    if (fread(&disk, sizeof(disk), 1, file) != 1) {
        fclose(file);
        set_error(err, errlen, "User data file is incomplete or unreadable");
        return 0;
    }
    if (fgetc(file) != EOF) {
        fclose(file);
        set_error(err, errlen, "User data file is incomplete or unreadable");
        return 0;
    }
    if (fclose(file) != 0) {
        set_error(err, errlen, "Could not close user data file");
        return 0;
    }

    if (
        disk.version != AUTH_FILE_VERSION ||
        disk.count > AUTH_MAX_USERS ||
        disk.next_user_id < 1
    ) {
        set_error(err, errlen, "User data file has an unsupported format");
        return 0;
    }

    {
        size_t i;
        for (i = 0; i < disk.count; i++) {
            AuthUser *user = &disk.users[i];
            size_t j;
            if (!user->active) {
                continue;
            }
            if (
                user->id < 1 ||
                user->id >= disk.next_user_id ||
                (user->role != AUTH_ROLE_ADMIN && user->role != AUTH_ROLE_VIEWER) ||
                memchr(user->username, 0, sizeof(user->username)) == NULL ||
                memchr(user->name, 0, sizeof(user->name)) == NULL ||
                user->username[0] == 0 ||
                user->name[0] == 0
            ) {
                set_error(err, errlen, "User data file contains an invalid account");
                return 0;
            }
            for (j = 0; j < i; j++) {
                if (
                    disk.users[j].active &&
                    disk.users[j].id == user->id
                ) {
                    set_error(err, errlen, "User data file contains duplicate account IDs");
                    return 0;
                }
            }
        }
    }

    store->user_count = disk.count;
    store->next_user_id = disk.next_user_id;
    memcpy(store->users, disk.users, sizeof(store->users));
    return 1;
}

int auth_store_save(const AuthStore *store, char *err, size_t errlen) {
    AuthDisk disk;
    char temporary_path[300];
    FILE *file;

    memset(&disk, 0, sizeof(disk));
    disk.version = AUTH_FILE_VERSION;
    disk.count = (unsigned int)store->user_count;
    disk.next_user_id = store->next_user_id;
    memcpy(disk.users, store->users, sizeof(disk.users));

    if (snprintf(
            temporary_path,
            sizeof(temporary_path),
            "%s.tmp",
            store->storage_path
        ) >= (int)sizeof(temporary_path)) {
        set_error(err, errlen, "User data path is too long");
        return 0;
    }

    file = fopen(temporary_path, "wb");
    if (file == NULL) {
        set_error(err, errlen, "Could not write user data file");
        return 0;
    }
    if (fwrite(&disk, sizeof(disk), 1, file) != 1 || fflush(file) != 0) {
        fclose(file);
        remove(temporary_path);
        set_error(err, errlen, "Could not finish writing user data file");
        return 0;
    }
    if (fclose(file) != 0) {
        remove(temporary_path);
        set_error(err, errlen, "Could not finish writing user data file");
        return 0;
    }

    if (!MoveFileExA(
            temporary_path,
            store->storage_path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        remove(temporary_path);
        set_error(err, errlen, "Could not replace user data file");
        return 0;
    }

    return 1;
}

int auth_setup_required(const AuthStore *store) {
    return store->user_count == 0;
}

AuthUser *auth_find_user(AuthStore *store, int id) {
    size_t i;

    for (i = 0; i < store->user_count; i++) {
        if (store->users[i].active && store->users[i].id == id) {
            return &store->users[i];
        }
    }
    return NULL;
}

AuthUser *auth_find_session(AuthStore *store, const char *token) {
    size_t i;

    if (token == NULL || token[0] == 0) {
        return NULL;
    }
    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        AuthSession *session = &store->sessions[i];
        if (
            session->active &&
            strcmp(session->token, token) == 0
        ) {
            return auth_find_user(store, session->user_id);
        }
    }
    return NULL;
}

int auth_create_user(
    AuthStore *store,
    const char *username,
    const char *name,
    const char *password,
    AuthRole role,
    AuthUser **created,
    char *err,
    size_t errlen
) {
    AuthUser user;
    char normalized[AUTH_USERNAME_LEN];
    size_t slot;

    for (slot = 0; slot < store->user_count; slot++) {
        if (!store->users[slot].active) {
            break;
        }
    }
    if (slot == AUTH_MAX_USERS) {
        set_error(err, errlen, "The user limit has been reached");
        return 0;
    }
    if (!username_normalize(username, normalized)) {
        set_error(err, errlen, "Username must be 3-63 letters, numbers, dots, hyphens, or underscores");
        return 0;
    }
    if (find_username(store, username) != NULL) {
        set_error(err, errlen, "That username is already in use");
        return 0;
    }
    if (name == NULL || name[0] == 0 || strlen(name) >= AUTH_NAME_LEN) {
        set_error(err, errlen, "Name is required and must be shorter than 96 characters");
        return 0;
    }
    if (
        role != AUTH_ROLE_ADMIN &&
        role != AUTH_ROLE_VIEWER
    ) {
        set_error(err, errlen, "Invalid user role");
        return 0;
    }
    if (
        password == NULL ||
        strlen(password) < 8 ||
        strlen(password) > AUTH_PASSWORD_MAX
    ) {
        set_error(err, errlen, "Password must be 8-128 characters");
        return 0;
    }

    memset(&user, 0, sizeof(user));
    user.id = store->next_user_id;
    user.active = 1;
    user.role = role;
    strcpy(user.username, normalized);
    strcpy(user.name, name);
    if (!set_password(&user, password)) {
        set_error(err, errlen, "Could not hash password using Windows cryptography");
        return 0;
    }

    store->users[slot] = user;
    if (slot == store->user_count) {
        store->user_count++;
    }
    store->next_user_id++;
    if (created != NULL) {
        *created = &store->users[slot];
    }
    return 1;
}

int auth_verify_password(const AuthUser *user, const char *password) {
    unsigned char candidate[32];

    if (user == NULL || password == NULL || strlen(password) > AUTH_PASSWORD_MAX) {
        return 0;
    }
    if (!password_hash(user->salt, password, candidate)) {
        return 0;
    }
    return secure_equal(candidate, user->password_hash, sizeof(candidate));
}

int auth_create_session(AuthStore *store, int user_id, char token[AUTH_TOKEN_LEN]) {
    static const char hex[] = "0123456789abcdef";
    unsigned char random[32];
    size_t i;
    size_t slot;

    if (!random_bytes(random, sizeof(random))) {
        return 0;
    }
    for (slot = 0; slot < AUTH_MAX_SESSIONS; slot++) {
        if (!store->sessions[slot].active) {
            break;
        }
    }
    if (slot == AUTH_MAX_SESSIONS) {
        return 0;
    }

    for (i = 0; i < sizeof(random); i++) {
        token[i * 2] = hex[random[i] >> 4];
        token[i * 2 + 1] = hex[random[i] & 15];
    }
    token[AUTH_TOKEN_LEN - 1] = 0;
    store->sessions[slot].active = 1;
    store->sessions[slot].user_id = user_id;
    strcpy(store->sessions[slot].token, token);
    return 1;
}

void auth_revoke_session(AuthStore *store, const char *token) {
    size_t i;

    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (
            store->sessions[i].active &&
            strcmp(store->sessions[i].token, token) == 0
        ) {
            memset(&store->sessions[i], 0, sizeof(store->sessions[i]));
            return;
        }
    }
}

int auth_update_user(
    AuthStore *store,
    AuthUser *user,
    const char *name,
    const char *password,
    int change_role,
    AuthRole role,
    char *err,
    size_t errlen
) {
    if (user == NULL || !user->active) {
        set_error(err, errlen, "User not found");
        return 0;
    }
    if (name != NULL) {
        if (name[0] == 0 || strlen(name) >= AUTH_NAME_LEN) {
            set_error(err, errlen, "Name is required and must be shorter than 96 characters");
            return 0;
        }
        strcpy(user->name, name);
    }
    if (change_role) {
        if (
            role != AUTH_ROLE_ADMIN &&
            role != AUTH_ROLE_VIEWER
        ) {
            set_error(err, errlen, "Invalid user role");
            return 0;
        }
        if (
            user->role == AUTH_ROLE_ADMIN &&
            role != AUTH_ROLE_ADMIN &&
            admin_count(store) <= 1
        ) {
            set_error(err, errlen, "At least one administrator must remain");
            return 0;
        }
        user->role = role;
    }
    if (password != NULL && password[0] != 0) {
        if (
            strlen(password) < 8 ||
            strlen(password) > AUTH_PASSWORD_MAX
        ) {
            set_error(err, errlen, "Password must be 8-128 characters");
            return 0;
        }
        if (!set_password(user, password)) {
            set_error(err, errlen, "Could not hash password using Windows cryptography");
            return 0;
        }
        for (size_t i = 0; i < AUTH_MAX_SESSIONS; i++) {
            if (
                store->sessions[i].active &&
                store->sessions[i].user_id == user->id
            ) {
                memset(&store->sessions[i], 0, sizeof(store->sessions[i]));
            }
        }
    }
    return 1;
}

int auth_delete_user(
    AuthStore *store,
    AuthUser *user,
    char *err,
    size_t errlen
) {
    size_t i;

    if (user == NULL || !user->active) {
        set_error(err, errlen, "User not found");
        return 0;
    }
    if (user->role == AUTH_ROLE_ADMIN && admin_count(store) <= 1) {
        set_error(err, errlen, "At least one administrator must remain");
        return 0;
    }
    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (
            store->sessions[i].active &&
            store->sessions[i].user_id == user->id
        ) {
            memset(&store->sessions[i], 0, sizeof(store->sessions[i]));
        }
    }
    user->active = 0;
    return 1;
}

static int json_reserve(JsonBuffer *buffer, size_t extra) {
    size_t capacity;
    char *data;

    if (buffer->length + extra + 1 <= buffer->capacity) {
        return 1;
    }
    capacity = buffer->capacity == 0 ? 512 : buffer->capacity;
    while (capacity < buffer->length + extra + 1) {
        capacity *= 2;
    }
    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL) {
        return 0;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

static void json_add(JsonBuffer *buffer, const char *text) {
    size_t length = strlen(text);
    if (json_reserve(buffer, length)) {
        memcpy(buffer->data + buffer->length, text, length);
        buffer->length += length;
        buffer->data[buffer->length] = 0;
    }
}

static void json_string(JsonBuffer *buffer, const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    json_add(buffer, "\"");
    while (p != NULL && *p != 0) {
        char escaped[7];
        if (*p == '"' || *p == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*p;
            escaped[2] = 0;
            json_add(buffer, escaped);
        } else if (*p < 32) {
            snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
            json_add(buffer, escaped);
        } else {
            escaped[0] = (char)*p;
            escaped[1] = 0;
            json_add(buffer, escaped);
        }
        p++;
    }
    json_add(buffer, "\"");
}

static void json_user(JsonBuffer *buffer, const AuthUser *user) {
    char id[32];
    snprintf(id, sizeof(id), "%d", user->id);
    json_add(buffer, "{\"id\":");
    json_add(buffer, id);
    json_add(buffer, ",\"username\":");
    json_string(buffer, user->username);
    json_add(buffer, ",\"name\":");
    json_string(buffer, user->name);
    json_add(buffer, ",\"role\":");
    json_string(
        buffer,
        user->role == AUTH_ROLE_ADMIN ? "admin" : "viewer"
    );
    json_add(buffer, "}");
}

char *auth_users_json(const AuthStore *store) {
    JsonBuffer buffer = {0};
    size_t i;
    int count = 0;

    json_add(&buffer, "{\"success\":true,\"data\":[");
    for (i = 0; i < store->user_count; i++) {
        if (!store->users[i].active) {
            continue;
        }
        if (count++ > 0) {
            json_add(&buffer, ",");
        }
        json_user(&buffer, &store->users[i]);
    }
    json_add(&buffer, "]}");
    return buffer.data;
}

char *auth_user_json(const AuthUser *user) {
    JsonBuffer buffer = {0};
    json_add(&buffer, "{\"success\":true,\"data\":");
    json_user(&buffer, user);
    json_add(&buffer, "}");
    return buffer.data;
}

char *auth_session_json(const AuthUser *user, const char *token) {
    JsonBuffer buffer = {0};
    json_add(&buffer, "{\"success\":true,\"data\":{\"token\":");
    json_string(&buffer, token);
    json_add(&buffer, ",\"user\":");
    json_user(&buffer, user);
    json_add(&buffer, "}}");
    return buffer.data;
}

char *auth_status_json(int setup_required) {
    JsonBuffer buffer = {0};
    json_add(
        &buffer,
        setup_required
            ? "{\"success\":true,\"data\":{\"setupRequired\":true}}"
            : "{\"success\":true,\"data\":{\"setupRequired\":false}}"
    );
    return buffer.data;
}
