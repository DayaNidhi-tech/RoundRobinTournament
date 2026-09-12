#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include "tournament.h"
#include "team.h"
#include "scheduler.h"
#include "match.h"
#include "standings.h"
#include "storage.h"
#define CHECK(x) do{if(!(x)){printf("FAIL: %s (line %d)\n",#x,__LINE__);return 1;}}while(0)
static int schedule_tests(void){int n;for(n=2;n<=8;n++){TournamentStore s;Tournament*t;char e[128];int i;store_init(&s);t=tournament_create(&s,"Test","",e,sizeof e);for(i=0;i<n;i++){char x[20];sprintf(x,"Team%d",i);CHECK(team_add(t,x,"","",e,sizeof e));}CHECK(schedule_generate(t,e,sizeof e));CHECK(t->rounds==(n%2?n:n-1));CHECK(schedule_validate(t,e,sizeof e));store_free(&s);}return 0;}
int main(void){TournamentStore s,s2;Tournament*t;char e[128];size_t n;Standing*x;_mkdir("data");CHECK(schedule_tests()==0);store_init(&s);t=tournament_create(&s,"Cup","",e,sizeof e);CHECK(team_add(t,"A","","",e,sizeof e));CHECK(team_add(t,"B","","",e,sizeof e));CHECK(schedule_generate(t,e,sizeof e));CHECK(!match_set_result(t,999,1,0,e,sizeof e));CHECK(match_set_result(t,t->matches[0].id,2,1,e,sizeof e));x=standings_calculate(t,&n);CHECK(n==2&&x[0].points==3);free(x);CHECK(storage_save(&s,"data/test.bin",e,sizeof e));store_init(&s2);CHECK(storage_load(&s2,"data/test.bin",e,sizeof e));CHECK(s2.head&&s2.head->tournament.team_count==2);store_free(&s);store_free(&s2);printf("All scheduler, standings, validation, and storage tests passed.\n");return 0;}
