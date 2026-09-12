#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tournament.h"
static void fail(char *e,size_t n,const char *s){ if(e&&n) snprintf(e,n,"%s",s); }
void store_init(TournamentStore *s){ s->head=NULL; s->next_tournament_id=1; }
static void free_tournament(Tournament *t){ free(t->teams); free(t->matches); }
void store_free(TournamentStore *s){ TournamentNode *n=s->head; while(n){TournamentNode *next=n->next;free_tournament(&n->tournament);free(n);n=next;} store_init(s); }
Tournament *tournament_create(TournamentStore *s,const char *name,const char *desc,char *e,size_t n){
 TournamentNode *node; if(!name||!*name){fail(e,n,"Tournament name is required");return NULL;} node=calloc(1,sizeof *node);if(!node){fail(e,n,"Out of memory");return NULL;}
 node->tournament.id=s->next_tournament_id++; node->tournament.next_team_id=1;node->tournament.next_match_id=1;node->tournament.status=TOURNAMENT_DRAFT;
 snprintf(node->tournament.name,NAME_LEN,"%s",name);snprintf(node->tournament.description,TEXT_LEN,"%s",desc?desc:""); node->next=s->head;s->head=node;return &node->tournament;
}
Tournament *tournament_find(TournamentStore *s,int id){TournamentNode *n=s->head;while(n){if(n->tournament.id==id)return &n->tournament;n=n->next;}return NULL;}
int tournament_update(Tournament *t,const char *name,const char *desc,int status,char *e,size_t n){if(!t||!name||!*name){fail(e,n,"A tournament name is required");return 0;}if(status<0||status>2){fail(e,n,"Invalid tournament status");return 0;}snprintf(t->name,NAME_LEN,"%s",name);snprintf(t->description,TEXT_LEN,"%s",desc?desc:"");t->status=(TournamentStatus)status;return 1;}
int tournament_delete(TournamentStore *s,int id,char *e,size_t z){TournamentNode **p=&s->head;while(*p){if((*p)->tournament.id==id){TournamentNode *old=*p;*p=old->next;free_tournament(&old->tournament);free(old);return 1;}p=&(*p)->next;}fail(e,z,"Tournament not found");return 0;}
