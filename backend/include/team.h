#ifndef TEAM_H
#define TEAM_H
#include "models.h"
Team *team_find(Tournament *t, int id); int team_add(Tournament *t, const char *name, const char *city, const char *logo, char *err, size_t errlen);
int team_update(Tournament *t, int id, const char *name, const char *city, const char *logo, char *err, size_t errlen);
int team_delete(Tournament *t, int id, char *err, size_t errlen);
#endif
