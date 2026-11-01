#ifndef SERVER_H
#define SERVER_H
#include "models.h"
#include "auth.h"
int server_run(TournamentStore *store, AuthStore *auth, unsigned short port);
#endif
