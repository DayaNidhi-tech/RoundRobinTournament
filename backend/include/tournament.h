#ifndef TOURNAMENT_H
#define TOURNAMENT_H
#include "models.h"
void store_init(TournamentStore *store); void store_free(TournamentStore *store);
Tournament *tournament_create(TournamentStore *store, const char *name, const char *description, char *err, size_t errlen);
Tournament *tournament_find(TournamentStore *store, int id); int tournament_update(Tournament *t, const char *name, const char *desc, int status, char *err, size_t errlen);
int tournament_delete(TournamentStore *store, int id, char *err, size_t errlen);
#endif
