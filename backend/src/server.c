#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "server.h"
#include "tournament.h"
#include "team.h"
#include "scheduler.h"
#include "match.h"
#include "storage.h"
#include "json.h"
#define BUF_SIZE 65536
static void send_response(SOCKET c,int code,const char *type,const char *body){char h[512];int len=(int)strlen(body);snprintf(h,sizeof h,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",code,code==200?"OK":code==404?"Not Found":"Bad Request",type,len);send(c,h,(int)strlen(h),0);send(c,body,len,0);}
static void respond_json(SOCKET c,int code,char *body){send_response(c,code,"application/json; charset=utf-8",body?body:"{\"success\":false}");free(body);}
static Tournament *get_tournament(TournamentStore*s,const char*path){int id=0;if(sscanf(path,"/api/tournaments/%d",&id)!=1)return NULL;return tournament_find(s,id);}
/* Route comparisons must be exact: sscanf alone accepts a converted ID even
   when a later literal segment does not match (for example, /schedule vs /teams). */
static int tournament_path(const char *path,int tournament_id,const char *suffix){char expected[160];snprintf(expected,sizeof expected,"/api/tournaments/%d%s",tournament_id,suffix);return strcmp(path,expected)==0;}
static int item_path(const char *path,int tournament_id,const char *collection,int *item_id,const char *suffix){char prefix[160],*end;long value;snprintf(prefix,sizeof prefix,"/api/tournaments/%d/%s/",tournament_id,collection);if(strncmp(path,prefix,strlen(prefix))!=0)return 0;value=strtol(path+strlen(prefix),&end,10);if(end==path+strlen(prefix)||value<1||value>2147483647L||strcmp(end,suffix)!=0)return 0;*item_id=(int)value;return 1;}
static const char *mime(const char *p){const char*x=strrchr(p,'.');if(x&&strcmp(x,".css")==0)return "text/css";if(x&&strcmp(x,".js")==0)return "application/javascript";if(x&&strcmp(x,".html")==0)return "text/html";return "application/octet-stream";}
static void serve_file(SOCKET c,const char*path){char disk[512],*body;FILE*f;long z;if(strcmp(path,"/")==0)path="/index.html";if(strstr(path,"..")){send_response(c,404,"text/plain","Not found");return;}snprintf(disk,sizeof disk,"frontend%s",path);f=fopen(disk,"rb");if(!f){send_response(c,404,"text/plain","Not found");return;}fseek(f,0,SEEK_END);z=ftell(f);rewind(f);body=malloc((size_t)z+1);if(!body){fclose(f);send_response(c,400,"text/plain","Memory error");return;}fread(body,1,(size_t)z,f);body[z]=0;fclose(f);send_response(c,200,mime(disk),body);free(body);}
static void handle_api(SOCKET c,TournamentStore*s,const char*m,const char*p,const char*b){char err[256]={0},name[NAME_LEN]={0},city[NAME_LEN]={0},desc[TEXT_LEN]={0},logo[TEXT_LEN]={0};int sub,score1,score2;Tournament*t;
 if(strcmp(m,"GET")==0&&strcmp(p,"/api/tournaments")==0){respond_json(c,200,json_tournaments(s));return;}
 if(strcmp(m,"POST")==0&&strcmp(p,"/api/tournaments")==0){json_get_string(b,"name",name,sizeof name);json_get_string(b,"description",desc,sizeof desc);t=tournament_create(s,name,desc,err,sizeof err);respond_json(c,t?200:400,t?json_tournament(t):json_message(0,err));return;}
 t=get_tournament(s,p);if(!t){respond_json(c,404,json_message(0,"Tournament not found"));return;}
 if(strcmp(m,"GET")==0&&tournament_path(p,t->id,"")){respond_json(c,200,json_tournament(t));return;}
 if(strcmp(m,"PUT")==0&&tournament_path(p,t->id,"")){int st=0;json_get_string(b,"name",name,sizeof name);json_get_string(b,"description",desc,sizeof desc);json_get_int(b,"status",&st);respond_json(c,tournament_update(t,name,desc,st,err,sizeof err)?200:400,json_message(err[0]==0,"Tournament updated"));return;}
 if(strcmp(m,"DELETE")==0&&tournament_path(p,t->id,"")){respond_json(c,tournament_delete(s,t->id,err,sizeof err)?200:400,json_message(err[0]==0,"Tournament deleted"));return;}
 if(strcmp(m,"GET")==0&&tournament_path(p,t->id,"/teams")){respond_json(c,200,json_teams(t));return;}
 if(strcmp(m,"POST")==0&&tournament_path(p,t->id,"/teams")){json_get_string(b,"name",name,sizeof name);json_get_string(b,"city",city,sizeof city);json_get_string(b,"logo",logo,sizeof logo);respond_json(c,team_add(t,name,city,logo,err,sizeof err)?200:400,json_message(err[0]==0,"Team added"));return;}
 if(strcmp(m,"PUT")==0&&item_path(p,t->id,"teams",&sub,"")){json_get_string(b,"name",name,sizeof name);json_get_string(b,"city",city,sizeof city);json_get_string(b,"logo",logo,sizeof logo);respond_json(c,team_update(t,sub,name,city,logo,err,sizeof err)?200:400,json_message(err[0]==0,"Team updated"));return;}
 if(strcmp(m,"DELETE")==0&&item_path(p,t->id,"teams",&sub,"")){respond_json(c,team_delete(t,sub,err,sizeof err)?200:400,json_message(err[0]==0,"Team deleted"));return;}
 if(strcmp(m,"POST")==0&&tournament_path(p,t->id,"/schedule/generate")){respond_json(c,schedule_generate(t,err,sizeof err)?200:400,json_message(err[0]==0,"Schedule generated"));return;}
 if(strcmp(m,"GET")==0&&(tournament_path(p,t->id,"/schedule")||tournament_path(p,t->id,"/matches"))){respond_json(c,200,json_matches(t));return;}
 if(strcmp(m,"PUT")==0&&item_path(p,t->id,"matches",&sub,"/result")){if(!json_get_int(b,"homeScore",&score1)||!json_get_int(b,"awayScore",&score2)){respond_json(c,400,json_message(0,"Both scores are required"));return;}respond_json(c,match_set_result(t,sub,score1,score2,err,sizeof err)?200:400,json_message(err[0]==0,"Result saved"));return;}
 if(strcmp(m,"GET")==0&&tournament_path(p,t->id,"/standings")){respond_json(c,200,json_standings(t));return;}
 if(strcmp(m,"POST")==0&&tournament_path(p,t->id,"/save")){respond_json(c,storage_save(s,"data/tournaments.bin",err,sizeof err)?200:400,json_message(err[0]==0,"All tournaments saved"));return;}
 if(strcmp(m,"POST")==0&&tournament_path(p,t->id,"/load")){respond_json(c,storage_load(s,"data/tournaments.bin",err,sizeof err)?200:400,json_message(err[0]==0,"Saved tournaments loaded"));return;}
 respond_json(c,404,json_message(0,"Unknown API endpoint"));
}
static void client(SOCKET c,TournamentStore*s){char *buf=calloc(1,BUF_SIZE+1),method[8]={0},path[512]={0},*body,*end,*length_header;int got,content=0,header_size;if(!buf)return;got=recv(c,buf,BUF_SIZE,0);if(got<=0){free(buf);return;}end=strstr(buf,"\r\n\r\n");if(end){header_size=(int)(end-buf)+4;length_header=strstr(buf,"Content-Length:");if(length_header)sscanf(length_header,"Content-Length: %d",&content);while(got<header_size+content&&got<BUF_SIZE){int received=recv(c,buf+got,BUF_SIZE-got,0);if(received<=0)break;got+=received;buf[got]=0;}}sscanf(buf,"%7s %511s",method,path);body=end?end+4:"";if(strncmp(path,"/api/",5)==0)handle_api(c,s,method,path,body);else if(strcmp(method,"GET")==0)serve_file(c,path);else send_response(c,400,"text/plain","Bad request");free(buf);}
int server_run(TournamentStore*s,unsigned short port){WSADATA w;SOCKET sock,client_sock;struct sockaddr_in addr; if(WSAStartup(MAKEWORD(2,2),&w)!=0)return 1;sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(sock==INVALID_SOCKET){WSACleanup();return 1;}memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(port);if(bind(sock,(struct sockaddr*)&addr,sizeof addr)==SOCKET_ERROR||listen(sock,8)==SOCKET_ERROR){closesocket(sock);WSACleanup();return 1;}printf("Tournament Hub running at http://localhost:%u\nPress Ctrl+C to stop.\n",port);for(;;){client_sock=accept(sock,NULL,NULL);if(client_sock!=INVALID_SOCKET){client(client_sock,s);closesocket(client_sock);}}}
