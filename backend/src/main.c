#include <stdio.h>
#include <direct.h>
#include <stdlib.h>
#include "tournament.h"
#include "storage.h"
#include "server.h"
int main(int argc,char **argv){TournamentStore store;char err[256];unsigned short port=8080;if(argc>1){long value=strtol(argv[1],NULL,10);if(value>0&&value<65536)port=(unsigned short)value;}store_init(&store);_mkdir("data");if(!storage_load(&store,"data/tournaments.bin",err,sizeof err))printf("Starting with an empty tournament store.\n");return server_run(&store,port);}
