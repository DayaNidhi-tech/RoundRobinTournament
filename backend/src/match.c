#include <stdio.h>
#include "match.h"
static void fail(char *e,size_t n,const char *s){if(e&&n)snprintf(e,n,"%s",s);} Match *match_find(Tournament *t,int id){size_t i;for(i=0;i<t->match_count;i++)if(t->matches[i].id==id)return &t->matches[i];return NULL;}
int match_set_result(Tournament *t,int id,int h,int a,char *e,size_t n){Match *m=match_find(t,id);if(!m){fail(e,n,"Match not found");return 0;}if(m->status==MATCH_BYE){fail(e,n,"A BYE cannot receive a result");return 0;}if(h<0||a<0){fail(e,n,"Scores must be non-negative integers");return 0;}m->home_score=h;m->away_score=a;m->status=MATCH_COMPLETED;return 1;}
