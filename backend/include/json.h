#ifndef JSON_H
#define JSON_H
#include <stddef.h>
#include "models.h"
int json_get_string(const char *body, const char *key, char *out, size_t size);
int json_get_int(const char *body, const char *key, int *out);
char *json_tournaments(const TournamentStore *store); char *json_tournament(const Tournament *t); char *json_teams(const Tournament *t); char *json_matches(const Tournament *t); char *json_standings(const Tournament *t);
char *json_message(int ok, const char *message);
#endif
