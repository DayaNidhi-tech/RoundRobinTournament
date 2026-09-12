#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "models.h"
int schedule_generate(Tournament *t, char *err, size_t errlen); int schedule_validate(const Tournament *t, char *err, size_t errlen);
#endif
