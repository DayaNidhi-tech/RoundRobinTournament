#ifndef MATCH_H
#define MATCH_H
#include "models.h"
Match *match_find(Tournament *t, int id); int match_set_result(Tournament *t, int id, int home, int away, char *err, size_t errlen);
#endif
