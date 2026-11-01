#include <stdio.h>
#include <direct.h>
#include <stdlib.h>
#include "tournament.h"
#include "storage.h"
#include "server.h"
#include "auth.h"

int main(int argc,char **argv){
    TournamentStore store;
    AuthStore auth;
    char err[256];
    unsigned short port=8080;

    if(argc>1){
        long value=strtol(argv[1],NULL,10);
        if(value>0&&value<65536)port=(unsigned short)value;
    }
    store_init(&store);
    _mkdir("data");
    if(!storage_load(&store,"data/tournaments.bin",err,sizeof err))
        printf("Starting with an empty tournament store.\n");
    if(!auth_store_load(&auth,"data/users.bin",err,sizeof err)){
        fprintf(stderr,"Could not load user accounts: %s\n",err);
        store_free(&store);
        return 1;
    }
    return server_run(&store,&auth,port);
}
