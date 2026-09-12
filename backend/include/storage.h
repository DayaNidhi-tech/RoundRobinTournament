#ifndef STORAGE_H
#define STORAGE_H
#include "models.h"
int storage_save(const TournamentStore *store, const char *path, char *err, size_t errlen);
int storage_load(TournamentStore *store, const char *path, char *err, size_t errlen);
#endif
