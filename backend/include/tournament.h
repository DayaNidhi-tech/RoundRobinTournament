#ifndef TOURNAMENT_H
#define TOURNAMENT_H
#include "models.h"
void store_init(TournamentStore *store); void store_free(TournamentStore *store);
Tournament *tournament_create(TournamentStore *store, int owner_id, const char *name, const char *description, char *err, size_t errlen);
Tournament *tournament_find(TournamentStore *store, int id);
Tournament *tournament_find_owned(TournamentStore *store, int id, int owner_id);
int tournament_update(Tournament *t, const char *name, const char *desc, int status, char *err, size_t errlen);
int tournament_delete(TournamentStore *store, int id, char *err, size_t errlen);
#endif
