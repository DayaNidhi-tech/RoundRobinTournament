#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "server.h"
#include "tournament.h"
#include "team.h"
#include "scheduler.h"
#include "match.h"
#include "storage.h"
#include "json.h"
#include "auth.h"

#define BUF_SIZE 65536

static void send_bytes(
    SOCKET c,
    int code,
    const char *type,
    const void *body,
    size_t length
) {
    char h[512];

    const char *status =
        code == 200 ? "OK" :
        code == 404 ? "Not Found" :
        code == 401 ? "Unauthorized" :
        code == 403 ? "Forbidden" :
        code == 409 ? "Conflict" :
        code == 500 ? "Internal Server Error" :
        "Bad Request";

    int header_len = snprintf(
        h,
        sizeof(h),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "\r\n",
        code,
        status,
        type,
        (unsigned long)length
    );

    if (header_len <= 0) {
        return;
    }

    send(c, h, header_len, 0);

    if (body != NULL && length > 0) {
        const char *ptr = (const char *)body;
        size_t sent_total = 0;

        while (sent_total < length) {
            int chunk = (int)(length - sent_total);

            if (chunk > 65536) {
                chunk = 65536;
            }

            int sent = send(
                c,
                ptr + sent_total,
                chunk,
                0
            );

            if (sent <= 0) {
                break;
            }

            sent_total += (size_t)sent;
        }
    }
}

static void send_response(
    SOCKET c,
    int code,
    const char *type,
    const char *body
) {
    if (body == NULL) {
        body = "";
    }

    send_bytes(
        c,
        code,
        type,
        body,
        strlen(body)
    );
}

static void respond_json(
    SOCKET c,
    int code,
    char *body
) {
    send_response(
        c,
        code,
        "application/json; charset=utf-8",
        body ? body : "{\"success\":false}"
    );

    free(body);
}

static Tournament *get_owned_tournament(
    TournamentStore *s,
    const char *path,
    int owner_id
) {
    int id = 0;

    if (sscanf(path, "/api/tournaments/%d", &id) != 1) {
        return NULL;
    }

    return tournament_find_owned(s, id, owner_id);
}

/*
    Route comparisons must be exact.

    sscanf() alone can accept a converted ID even when
    a later literal segment does not match.
*/
static int tournament_path(
    const char *path,
    int tournament_id,
    const char *suffix
) {
    char expected[160];

    snprintf(
        expected,
        sizeof(expected),
        "/api/tournaments/%d%s",
        tournament_id,
        suffix
    );

    return strcmp(path, expected) == 0;
}

static int item_path(
    const char *path,
    int tournament_id,
    const char *collection,
    int *item_id,
    const char *suffix
) {
    char prefix[160];
    char *end;
    long value;

    snprintf(
        prefix,
        sizeof(prefix),
        "/api/tournaments/%d/%s/",
        tournament_id,
        collection
    );

    if (strncmp(
            path,
            prefix,
            strlen(prefix)
        ) != 0) {
        return 0;
    }

    value = strtol(
        path + strlen(prefix),
        &end,
        10
    );

    if (
        end == path + strlen(prefix) ||
        value < 1 ||
        value > 2147483647L ||
        strcmp(end, suffix) != 0
    ) {
        return 0;
    }

    *item_id = (int)value;

    return 1;
}

/*
    MIME types for static files.
*/
static const char *mime(const char *path) {
    const char *ext = strrchr(path, '.');

    if (!ext) {
        return "application/octet-stream";
    }

    if (strcmp(ext, ".html") == 0) {
        return "text/html; charset=utf-8";
    }

    if (strcmp(ext, ".css") == 0) {
        return "text/css; charset=utf-8";
    }

    if (strcmp(ext, ".js") == 0) {
        return "application/javascript; charset=utf-8";
    }

    if (strcmp(ext, ".json") == 0) {
        return "application/json; charset=utf-8";
    }

    if (strcmp(ext, ".jpg") == 0 ||
        strcmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    }

    if (strcmp(ext, ".png") == 0) {
        return "image/png";
    }

    if (strcmp(ext, ".gif") == 0) {
        return "image/gif";
    }

    if (strcmp(ext, ".webp") == 0) {
        return "image/webp";
    }

    if (strcmp(ext, ".svg") == 0) {
        return "image/svg+xml";
    }

    if (strcmp(ext, ".ico") == 0) {
        return "image/x-icon";
    }

    if (strcmp(ext, ".txt") == 0) {
        return "text/plain; charset=utf-8";
    }

    return "application/octet-stream";
}

/*
    Serve static files.

    IMPORTANT:
    Images are binary data.
    We must NOT use strlen() to determine their size.
*/
static void serve_file(
    SOCKET c,
    const char *path
) {
    char disk[512];
    FILE *f;
    long file_size;
    unsigned char *body;

    if (strcmp(path, "/") == 0) {
        path = "/login.html";
    }

    /*
        Basic path traversal protection.
    */
    if (strstr(path, "..")) {
        send_response(
            c,
            404,
            "text/plain; charset=utf-8",
            "Not found"
        );
        return;
    }

    /*
        Only serve files inside frontend/.
    */
    snprintf(
        disk,
        sizeof(disk),
        "frontend%s",
        path
    );

    f = fopen(disk, "rb");

    if (!f) {
        send_response(
            c,
            404,
            "text/plain; charset=utf-8",
            "Not found"
        );
        return;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);

        send_response(
            c,
            400,
            "text/plain; charset=utf-8",
            "File error"
        );

        return;
    }

    file_size = ftell(f);

    if (file_size < 0) {
        fclose(f);

        send_response(
            c,
            400,
            "text/plain; charset=utf-8",
            "File error"
        );

        return;
    }

    rewind(f);

    body = (unsigned char *)malloc(
        (size_t)file_size
    );

    if (file_size > 0 && body == NULL) {
        fclose(f);

        send_response(
            c,
            400,
            "text/plain; charset=utf-8",
            "Memory error"
        );

        return;
    }

    if (
        file_size > 0 &&
        fread(
            body,
            1,
            (size_t)file_size,
            f
        ) != (size_t)file_size
    ) {
        free(body);
        fclose(f);

        send_response(
            c,
            400,
            "text/plain; charset=utf-8",
            "File read error"
        );

        return;
    }

    fclose(f);

    send_bytes(
        c,
        200,
        mime(disk),
        body,
        (size_t)file_size
    );

    free(body);
}

static int user_id_path(const char *path, int *user_id, const char *suffix) {
    char *end;
    long value;

    if (strncmp(path, "/api/users/", 11) != 0) {
        return 0;
    }
    value = strtol(path + 11, &end, 10);
    if (
        end == path + 11 ||
        value < 1 ||
        value > 2147483647L ||
        strcmp(end, suffix) != 0
    ) {
        return 0;
    }
    *user_id = (int)value;
    return 1;
}

static int json_has_field(const char *body, const char *field) {
    char key[96];
    snprintf(key, sizeof(key), "\"%s\"", field);
    return strstr(body, key) != NULL;
}

static int reset_all_data(TournamentStore *tournament_store, AuthStore *auth_store, char *err, size_t errlen) {
    char auth_path[260] = {0};
    char tournament_path[260] = {0};

    if (tournament_store == NULL || auth_store == NULL) {
        if (err != NULL && errlen > 0) {
            snprintf(err, errlen, "Invalid reset request");
        }
        return 0;
    }

    if (auth_store->storage_path[0] != 0) {
        snprintf(auth_path, sizeof(auth_path), "%s", auth_store->storage_path);
    } else {
        snprintf(auth_path, sizeof(auth_path), "data/users.bin");
    }
    snprintf(tournament_path, sizeof(tournament_path), "data/tournaments.bin");

    if (remove(auth_path) != 0 && errno != ENOENT) {
        if (err != NULL && errlen > 0) {
            snprintf(err, errlen, "Could not reset stored user accounts");
        }
        return 0;
    }
    if (remove(tournament_path) != 0 && errno != ENOENT) {
        if (err != NULL && errlen > 0) {
            snprintf(err, errlen, "Could not reset stored tournament data");
        }
        return 0;
    }

    store_free(tournament_store);
    memset(auth_store, 0, sizeof(*auth_store));
    strcpy(auth_store->storage_path, auth_path);
    auth_store->next_user_id = 1;
    return 1;
}

static int handle_auth_api(
    SOCKET c,
    TournamentStore *tournament_store,
    AuthStore *store,
    const char *method,
    const char *path,
    const char *body,
    const char *token
) {
    char username[AUTH_USERNAME_LEN] = {0};
    char name[AUTH_NAME_LEN] = {0};
    char password[AUTH_PASSWORD_MAX + 1] = {0};
    char err[256] = {0};
    char session_token[AUTH_TOKEN_LEN] = {0};
    AuthUser *user;

    if (
        strcmp(method, "GET") == 0 &&
        strcmp(path, "/api/auth/status") == 0
    ) {
        respond_json(c, 200, auth_status_json(auth_setup_required(store)));
        return 1;
    }

    if (
        strcmp(method, "DELETE") == 0 &&
        strcmp(path, "/api/auth/reset") == 0
    ) {
        user = auth_find_session(store, token);
        if (user == NULL) {
            respond_json(c, 401, json_message(0, "Please sign in"));
            return 1;
        }
        if (!reset_all_data(tournament_store, store, err, sizeof(err))) {
            respond_json(c, 500, json_message(0, err));
            return 1;
        }
        respond_json(c, 200, json_message(1, "Account reset complete. All saved data was cleared."));
        return 1;
    }

    if (
        strcmp(method, "POST") == 0 &&
        strcmp(path, "/api/auth/setup") == 0
    ) {
        if (!auth_setup_required(store)) {
            respond_json(c, 409, json_message(0, "Initial setup has already been completed"));
            return 1;
        }
        if (
            !json_get_string(body, "username", username, sizeof(username)) ||
            !json_get_string(body, "name", name, sizeof(name)) ||
            !json_get_string(body, "password", password, sizeof(password))
        ) {
            respond_json(c, 400, json_message(0, "Name, username, and password are required"));
            return 1;
        }
        if (!auth_create_user(
                store,
                username,
                name,
                password,
                AUTH_ROLE_ADMIN,
                &user,
                err,
                sizeof(err)
            )) {
            respond_json(c, 400, json_message(0, err));
            return 1;
        }
        if (!auth_store_save(store, err, sizeof(err))) {
            user->active = 0;
            store->user_count--;
            store->next_user_id--;
            respond_json(c, 500, json_message(0, err));
            return 1;
        }
        if (!auth_create_session(store, user->id, session_token)) {
            respond_json(c, 500, json_message(0, "Administrator created. Please sign in."));
            return 1;
        }
        respond_json(c, 200, auth_session_json(user, session_token));
        return 1;
    }

    if (
        strcmp(method, "POST") == 0 &&
        strcmp(path, "/api/auth/login") == 0
    ) {
        if (
            !json_get_string(body, "username", username, sizeof(username)) ||
            !json_get_string(body, "password", password, sizeof(password))
        ) {
            respond_json(c, 400, json_message(0, "Username and password are required"));
            return 1;
        }
        user = auth_find_username(store, username);
        if (user == NULL || !auth_verify_password(user, password)) {
            respond_json(c, 401, json_message(0, "Username or password is incorrect"));
            return 1;
        }
        if (!auth_create_session(store, user->id, session_token)) {
            respond_json(c, 500, json_message(0, "Could not create a session"));
            return 1;
        }
        respond_json(c, 200, auth_session_json(user, session_token));
        return 1;
    }

    if (
        strcmp(path, "/api/auth/me") == 0 &&
        strcmp(method, "GET") == 0
    ) {
        user = auth_find_session(store, token);
        if (user == NULL) {
            respond_json(c, 401, json_message(0, "Please sign in"));
        } else {
            respond_json(c, 200, auth_user_json(user));
        }
        return 1;
    }

    if (
        strcmp(path, "/api/auth/logout") == 0 &&
        strcmp(method, "POST") == 0
    ) {
        if (auth_find_session(store, token) == NULL) {
            respond_json(c, 401, json_message(0, "Please sign in"));
        } else {
            auth_revoke_session(store, token);
            respond_json(c, 200, json_message(1, "Signed out"));
        }
        return 1;
    }

    return 0;
}

static void handle_users_api(
    SOCKET c,
    AuthStore *store,
    const char *method,
    const char *path,
    const char *body,
    AuthUser *current_user
) {
    char username[AUTH_USERNAME_LEN] = {0};
    char name[AUTH_NAME_LEN] = {0};
    char password[AUTH_PASSWORD_MAX + 1] = {0};
    char role_name[16] = {0};
    char err[256] = {0};
    AuthRole role = AUTH_ROLE_VIEWER;
    AuthUser *user;
    AuthUser previous;
    int id;
    int change_role = 0;
    size_t previous_count;
    size_t slot;
    AuthUser previous_slot;
    int previous_next_user_id;

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/users") == 0) {
        respond_json(c, 200, auth_users_json(store));
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/users") == 0) {
        if (
            !json_get_string(body, "username", username, sizeof(username)) ||
            !json_get_string(body, "name", name, sizeof(name)) ||
            !json_get_string(body, "password", password, sizeof(password))
        ) {
            respond_json(c, 400, json_message(0, "Name, username, and password are required"));
            return;
        }
        if (json_has_field(body, "role")) {
            if (!json_get_string(body, "role", role_name, sizeof(role_name))) {
                respond_json(c, 400, json_message(0, "Invalid user role"));
                return;
            }
            if (strcmp(role_name, "admin") == 0) {
                role = AUTH_ROLE_ADMIN;
            } else if (strcmp(role_name, "viewer") != 0) {
                respond_json(c, 400, json_message(0, "Invalid user role"));
                return;
            }
        }
        previous_count = store->user_count;
        previous_next_user_id = store->next_user_id;
        for (slot = 0; slot < store->user_count; slot++) {
            if (!store->users[slot].active) {
                break;
            }
        }
        memset(&previous_slot, 0, sizeof(previous_slot));
        if (slot < store->user_count) {
            previous_slot = store->users[slot];
        }
        if (!auth_create_user(
                store,
                username,
                name,
                password,
                role,
                &user,
                err,
                sizeof(err)
            )) {
            respond_json(c, 400, json_message(0, err));
            return;
        }
        if (!auth_store_save(store, err, sizeof(err))) {
            store->users[slot] = previous_slot;
            store->user_count = previous_count;
            store->next_user_id = previous_next_user_id;
            respond_json(c, 500, json_message(0, err));
            return;
        }
        respond_json(c, 200, auth_user_json(user));
        return;
    }

    if (!user_id_path(path, &id, "")) {
        respond_json(c, 404, json_message(0, "Unknown user endpoint"));
        return;
    }
    user = auth_find_user(store, id);
    if (user == NULL) {
        respond_json(c, 404, json_message(0, "User not found"));
        return;
    }

    if (strcmp(method, "PUT") == 0) {
        previous = *user;
        if (json_has_field(body, "name")) {
            if (!json_get_string(body, "name", name, sizeof(name))) {
                respond_json(c, 400, json_message(0, "Invalid name"));
                return;
            }
        }
        if (json_has_field(body, "password") &&
            !json_get_string(body, "password", password, sizeof(password))) {
            respond_json(c, 400, json_message(0, "Invalid password"));
            return;
        }
        if (json_has_field(body, "role")) {
            if (!json_get_string(body, "role", role_name, sizeof(role_name))) {
                respond_json(c, 400, json_message(0, "Invalid user role"));
                return;
            }
            if (strcmp(role_name, "admin") == 0) {
                role = AUTH_ROLE_ADMIN;
            } else if (strcmp(role_name, "viewer") == 0) {
                role = AUTH_ROLE_VIEWER;
            } else {
                respond_json(c, 400, json_message(0, "Invalid user role"));
                return;
            }
            change_role = 1;
        }
        if (!auth_update_user(
                store,
                user,
                json_has_field(body, "name") ? name : NULL,
                json_has_field(body, "password") ? password : NULL,
                change_role,
                role,
                err,
                sizeof(err)
            )) {
            *user = previous;
            respond_json(c, 400, json_message(0, err));
            return;
        }
        if (!auth_store_save(store, err, sizeof(err))) {
            *user = previous;
            respond_json(c, 500, json_message(0, err));
            return;
        }
        respond_json(c, 200, auth_user_json(user));
        return;
    }

    if (strcmp(method, "DELETE") == 0) {
        if (user->id == current_user->id) {
            respond_json(c, 400, json_message(0, "You cannot delete your own account"));
            return;
        }
        previous = *user;
        if (!auth_delete_user(store, user, err, sizeof(err))) {
            respond_json(c, 400, json_message(0, err));
            return;
        }
        if (!auth_store_save(store, err, sizeof(err))) {
            *user = previous;
            respond_json(c, 500, json_message(0, err));
            return;
        }
        respond_json(c, 200, json_message(1, "User deleted"));
        return;
    }

    respond_json(c, 404, json_message(0, "Unknown user endpoint"));
}

static void bearer_token(const char *request, char token[AUTH_TOKEN_LEN]) {
    const char *line = strstr(request, "\r\n");

    token[0] = 0;
    if (line == NULL) {
        return;
    }
    while (*line != 0) {
        const char *end;
        const char *value;
        size_t length;

        line += 2;
        end = strstr(line, "\r\n");
        if (end == NULL || end == line) {
            return;
        }
        {
            size_t i;
            int is_authorization = 1;
            static const char authorization[] = "Authorization:";
            for (i = 0; i < sizeof(authorization) - 1; i++) {
                if (tolower((unsigned char)line[i]) !=
                    tolower((unsigned char)authorization[i])) {
                    is_authorization = 0;
                    break;
                }
            }
            if (!is_authorization) {
                line = end;
                continue;
            }
        }
        {
            value = line + 14;
            while (value < end && (*value == ' ' || *value == '\t')) {
                value++;
            }
            if (
                (size_t)(end - value) < 7 ||
                tolower((unsigned char)value[0]) != 'b' ||
                tolower((unsigned char)value[1]) != 'e' ||
                tolower((unsigned char)value[2]) != 'a' ||
                tolower((unsigned char)value[3]) != 'r' ||
                tolower((unsigned char)value[4]) != 'e' ||
                tolower((unsigned char)value[5]) != 'r' ||
                value[6] != ' '
            ) {
                return;
            }
            value += 7;
            length = (size_t)(end - value);
            while (length > 0 && (value[length - 1] == ' ' || value[length - 1] == '\t')) {
                length--;
            }
            if (length > 0 && length < AUTH_TOKEN_LEN) {
                memcpy(token, value, length);
                token[length] = 0;
            }
            return;
        }
        line = end;
    }
}

static void handle_api(
    SOCKET c,
    TournamentStore *s,
    const char *m,
    const char *p,
    const char *b,
    const AuthUser *current_user
) {
    char err[256] = {0};

    char name[NAME_LEN] = {0};
    char city[NAME_LEN] = {0};
    char desc[TEXT_LEN] = {0};
    char logo[TEXT_LEN] = {0};

    int sub;
    int score1;
    int score2;

    Tournament *t;

    /*
        GET /api/tournaments
    */
    if (
        strcmp(m, "GET") == 0 &&
        strcmp(p, "/api/tournaments") == 0
    ) {
        respond_json(
            c,
            200,
            json_tournaments(s, current_user->id)
        );

        return;
    }

    /*
        POST /api/tournaments
    */
    if (
        strcmp(m, "POST") == 0 &&
        strcmp(p, "/api/tournaments") == 0
    ) {
        json_get_string(
            b,
            "name",
            name,
            sizeof(name)
        );

        json_get_string(
            b,
            "description",
            desc,
            sizeof(desc)
        );

        t = tournament_create(
            s,
            current_user->id,
            name,
            desc,
            err,
            sizeof(err)
        );

        respond_json(
            c,
            t ? 200 : 400,
            t
                ? json_tournament(t)
                : json_message(0, err)
        );

        return;
    }

    /*
        All remaining API routes require a tournament.
    */
    t = get_owned_tournament(s, p, current_user->id);

    if (!t) {
        respond_json(
            c,
            404,
            json_message(
                0,
                "Tournament not found"
            )
        );

        return;
    }

    /*
        GET /api/tournaments/{id}
    */
    if (
        strcmp(m, "GET") == 0 &&
        tournament_path(p, t->id, "")
    ) {
        respond_json(
            c,
            200,
            json_tournament(t)
        );

        return;
    }

    /*
        PUT /api/tournaments/{id}
    */
    if (
        strcmp(m, "PUT") == 0 &&
        tournament_path(p, t->id, "")
    ) {
        int st = 0;

        json_get_string(
            b,
            "name",
            name,
            sizeof(name)
        );

        json_get_string(
            b,
            "description",
            desc,
            sizeof(desc)
        );

        json_get_int(
            b,
            "status",
            &st
        );

        respond_json(
            c,
            tournament_update(
                t,
                name,
                desc,
                st,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Tournament updated"
            )
        );

        return;
    }

    /*
        DELETE /api/tournaments/{id}
    */
    if (
        strcmp(m, "DELETE") == 0 &&
        tournament_path(p, t->id, "")
    ) {
        respond_json(
            c,
            tournament_delete(
                s,
                t->id,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Tournament deleted"
            )
        );

        return;
    }

    /*
        GET /api/tournaments/{id}/teams
    */
    if (
        strcmp(m, "GET") == 0 &&
        tournament_path(
            p,
            t->id,
            "/teams"
        )
    ) {
        respond_json(
            c,
            200,
            json_teams(t)
        );

        return;
    }

    /*
        POST /api/tournaments/{id}/teams
    */
    if (
        strcmp(m, "POST") == 0 &&
        tournament_path(
            p,
            t->id,
            "/teams"
        )
    ) {
        json_get_string(
            b,
            "name",
            name,
            sizeof(name)
        );

        json_get_string(
            b,
            "city",
            city,
            sizeof(city)
        );

        json_get_string(
            b,
            "logo",
            logo,
            sizeof(logo)
        );

        respond_json(
            c,
            team_add(
                t,
                name,
                city,
                logo,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Team added"
            )
        );

        return;
    }

    /*
        PUT /api/tournaments/{id}/teams/{teamId}
    */
    if (
        strcmp(m, "PUT") == 0 &&
        item_path(
            p,
            t->id,
            "teams",
            &sub,
            ""
        )
    ) {
        json_get_string(
            b,
            "name",
            name,
            sizeof(name)
        );

        json_get_string(
            b,
            "city",
            city,
            sizeof(city)
        );

        json_get_string(
            b,
            "logo",
            logo,
            sizeof(logo)
        );

        respond_json(
            c,
            team_update(
                t,
                sub,
                name,
                city,
                logo,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Team updated"
            )
        );

        return;
    }

    /*
        DELETE /api/tournaments/{id}/teams/{teamId}
    */
    if (
        strcmp(m, "DELETE") == 0 &&
        item_path(
            p,
            t->id,
            "teams",
            &sub,
            ""
        )
    ) {
        respond_json(
            c,
            team_delete(
                t,
                sub,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Team deleted"
            )
        );

        return;
    }

    /*
        POST /api/tournaments/{id}/schedule/generate
    */
    if (
        strcmp(m, "POST") == 0 &&
        tournament_path(
            p,
            t->id,
            "/schedule/generate"
        )
    ) {
        respond_json(
            c,
            schedule_generate(
                t,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Schedule generated"
            )
        );

        return;
    }

    /*
        GET /api/tournaments/{id}/schedule
        GET /api/tournaments/{id}/matches
    */
    if (
        strcmp(m, "GET") == 0 &&
        (
            tournament_path(
                p,
                t->id,
                "/schedule"
            ) ||
            tournament_path(
                p,
                t->id,
                "/matches"
            )
        )
    ) {
        respond_json(
            c,
            200,
            json_matches(t)
        );

        return;
    }

    /*
        PUT /api/tournaments/{id}/matches/{matchId}/result
    */
    if (
        strcmp(m, "PUT") == 0 &&
        item_path(
            p,
            t->id,
            "matches",
            &sub,
            "/result"
        )
    ) {
        if (
            !json_get_int(
                b,
                "homeScore",
                &score1
            ) ||
            !json_get_int(
                b,
                "awayScore",
                &score2
            )
        ) {
            respond_json(
                c,
                400,
                json_message(
                    0,
                    "Both scores are required"
                )
            );

            return;
        }

        respond_json(
            c,
            match_set_result(
                t,
                sub,
                score1,
                score2,
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Result saved"
            )
        );

        return;
    }

    /*
        GET /api/tournaments/{id}/standings
    */
    if (
        strcmp(m, "GET") == 0 &&
        tournament_path(
            p,
            t->id,
            "/standings"
        )
    ) {
        respond_json(
            c,
            200,
            json_standings(t)
        );

        return;
    }

    /*
        POST /api/tournaments/{id}/save
    */
    if (
        strcmp(m, "POST") == 0 &&
        tournament_path(
            p,
            t->id,
            "/save"
        )
    ) {
        respond_json(
            c,
            storage_save(
                s,
                "data/tournaments.bin",
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "All tournaments saved"
            )
        );

        return;
    }

    /*
        POST /api/tournaments/{id}/load
    */
    if (
        strcmp(m, "POST") == 0 &&
        tournament_path(
            p,
            t->id,
            "/load"
        )
    ) {
        respond_json(
            c,
            storage_load(
                s,
                "data/tournaments.bin",
                err,
                sizeof(err)
            ) ? 200 : 400,
            json_message(
                err[0] == 0,
                "Saved tournaments loaded"
            )
        );

        return;
    }

    respond_json(
        c,
        404,
        json_message(
            0,
            "Unknown API endpoint"
        )
    );
}

static void client(
    SOCKET c,
    TournamentStore *s,
    AuthStore *auth
) {
    char *buf;
    char method[8] = {0};
    char path[512] = {0};
    char token[AUTH_TOKEN_LEN] = {0};

    char *body;
    char *end;
    char *length_header;

    int got;
    int content = 0;
    int header_size;

    buf = (char *)calloc(
        1,
        BUF_SIZE + 1
    );

    if (!buf) {
        return;
    }

    got = recv(
        c,
        buf,
        BUF_SIZE,
        0
    );

    if (got <= 0) {
        free(buf);
        return;
    }

    buf[got] = 0;

    end = strstr(
        buf,
        "\r\n\r\n"
    );

    if (end) {
        header_size =
            (int)(end - buf) + 4;

        length_header = strstr(
            buf,
            "Content-Length:"
        );

        if (length_header) {
            sscanf(
                length_header,
                "Content-Length: %d",
                &content
            );
        }

        while (
            got < header_size + content &&
            got < BUF_SIZE
        ) {
            int received = recv(
                c,
                buf + got,
                BUF_SIZE - got,
                0
            );

            if (received <= 0) {
                break;
            }

            got += received;
            buf[got] = 0;
        }
    }

    sscanf(
        buf,
        "%7s %511s",
        method,
        path
    );

    body = end ? end + 4 : "";
    bearer_token(buf, token);

    if (strncmp(path, "/api/", 5) == 0) {
        if (strncmp(path, "/api/auth/", 10) == 0) {
            if (!handle_auth_api(c, s, auth, method, path, body, token)) {
                respond_json(c, 404, json_message(0, "Unknown authentication endpoint"));
            }
        } else {
            AuthUser *user = auth_find_session(auth, token);
            if (user == NULL) {
                respond_json(c, 401, json_message(0, "Please sign in"));
            } else if (
                strncmp(path, "/api/users", 10) == 0
            ) {
                if (user->role != AUTH_ROLE_ADMIN) {
                    respond_json(c, 403, json_message(0, "Administrator access required"));
                } else {
                    handle_users_api(c, auth, method, path, body, user);
                }
            } else if (
                strcmp(method, "GET") != 0 &&
                user->role != AUTH_ROLE_ADMIN
            ) {
                respond_json(c, 403, json_message(0, "View-only accounts cannot change tournament data"));
            } else {
                handle_api(c, s, method, path, body, user);
            }
        }
    }
    else if (strcmp(method, "GET") == 0) {
        serve_file(
            c,
            path
        );
    }
    else {
        send_response(
            c,
            400,
            "text/plain; charset=utf-8",
            "Bad request"
        );
    }

    free(buf);
}

int server_run(
    TournamentStore *s,
    AuthStore *auth,
    unsigned short port
) {
    WSADATA w;
    SOCKET sock;
    SOCKET client_sock;

    struct sockaddr_in addr;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &w
        ) != 0
    ) {
        return 1;
    }

    sock = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    memset(
        &addr,
        0,
        sizeof(addr)
    );

    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    addr.sin_port =
        htons(port);

    if (
        bind(
            sock,
            (struct sockaddr *)&addr,
            sizeof(addr)
        ) == SOCKET_ERROR ||
        listen(sock, 8) == SOCKET_ERROR
    ) {
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf(
        "Tournament Hub running at http://localhost:%u\n"
        "Press Ctrl+C to stop.\n",
        port
    );

    for (;;) {
        client_sock = accept(
            sock,
            NULL,
            NULL
        );

        if (
            client_sock != INVALID_SOCKET
        ) {
            client(
                client_sock,
                s,
                auth
            );

            closesocket(
                client_sock
            );
        }
    }
}
