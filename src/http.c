#include "http.h"
#include "config.h"
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void response(int fd,const char *status,const char *type,const char *body,const char *extra){char h[512];int n=snprintf(h,sizeof(h),"HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n%s\r\n",status,type,strlen(body),extra?extra:"");send(fd,h,n,0);send(fd,body,strlen(body),0);}
static int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static void url_decode(const char *src,char *dst,size_t size){size_t j=0;for(size_t i=0;src[i]&&j+1<size;i++){if(src[i]=='%'&&src[i+1]&&src[i+2]){int a=hex(src[i+1]),b=hex(src[i+2]);if(a>=0&&b>=0){dst[j++]=(char)((a<<4)|b);i+=2;continue;}}dst[j++]=src[i]=='+'?' ':src[i];}dst[j]=0;}

/* Check the cookie by parsing the Cookie header instead of requiring an exact
   header substring. This works with browsers that send multiple cookies. */
static int logged_in(const char *req){
    const char *p=req;
    while((p=strstr(p,"Cookie:"))!=NULL){
        p+=7;
        const char *end=strstr(p,"\r\n");
        size_t len=end?(size_t)(end-p):strlen(p);
        char cookie[512];
        if(len>=sizeof(cookie))len=sizeof(cookie)-1;
        memcpy(cookie,p,len);cookie[len]=0;
        char *v=strstr(cookie,PI_MONITOR_COOKIE);
        if(v && (v==cookie || v[-1]==';' || v[-1]==' ' || v[-1]=='\t')) return 1;
        p=end?end:p+len;
    }
    return 0;
}

static void file_response(int fd,const char *path,const char *type){FILE *f=fopen(path,"rb");char *data;long len;if(!f){response(fd,"404 Not Found","text/plain","Not found",NULL);return;}fseek(f,0,SEEK_END);len=ftell(f);rewind(f);if(len<0||len>1024*1024){fclose(f);response(fd,"500 Internal Server Error","text/plain","File error",NULL);return;}data=malloc((size_t)len+1);if(!data){fclose(f);response(fd,"500 Internal Server Error","text/plain","Memory error",NULL);return;}fread(data,1,(size_t)len,f);data[len]=0;fclose(f);response(fd,"200 OK",type,data,NULL);free(data);}

static void login_api(int fd,const char *req){const char *body=strstr(req,"\r\n\r\n");char raw_user[128]={0},raw_pass[256]={0},user[128],pass[256];if(body)body+=4;if(body){const char *u=strstr(body,"username="),*p=strstr(body,"password=");if(u)sscanf(u+9,"%127[^&]",raw_user);if(p)sscanf(p+9,"%255[^&]",raw_pass);}url_decode(raw_user,user,sizeof(user));url_decode(raw_pass,pass,sizeof(pass));if(!strcmp(user,PI_MONITOR_USER)&&!strcmp(pass,PI_MONITOR_PASSWORD))response(fd,"200 OK","application/json","{\"ok\":true}","Set-Cookie: " PI_MONITOR_COOKIE "; Path=/; HttpOnly\r\n");else response(fd,"401 Unauthorized","application/json","{\"ok\":false,\"error\":\"Invalid username or password\"}",NULL);}

static void terminal_api(int fd,const char *req){const char *body=strstr(req,"\r\n\r\n");char raw[2048]={0},cmd[2048],out[8192]={0};if(body){body+=4;const char *c=strstr(body,"command=");if(c)sscanf(c+8,"%2047[^&]",raw);}url_decode(raw,cmd,sizeof(cmd));if(!cmd[0]){response(fd,"400 Bad Request","application/json","{\"error\":\"command required\"}",NULL);return;}if(strstr(cmd,"shutdown")||strstr(cmd,"reboot")||strstr(cmd,"poweroff")){response(fd,"400 Bad Request","application/json","{\"error\":\"power commands use the Shutdown button\"}",NULL);}FILE *p=popen("/bin/sh -c 'exec 2>&1; printf %s\\n \"$1\"' sh \"\"","r");if(p)pclose(p);snprintf(out,sizeof(out),"{\"output\":\"\"}");char escaped[8192]={0};FILE *q=popen(cmd,"r");if(q){fread(escaped,1,sizeof(escaped)-1,q);pclose(q);}size_t j=0;for(size_t i=0;escaped[i]&&j+2<sizeof(out);i++){if(escaped[i]=='\\'||escaped[i]=='\"')out[j++]='\\';if(escaped[i]=='\n'){out[j++]='\\';out[j++]='n';}else if(escaped[i]!='\r')out[j++]=escaped[i];}out[j]=0;char json[8200];snprintf(json,sizeof(json),"{\"output\":\"%s\"}",out);response(fd,"200 OK","application/json",json,NULL);}

static void api(int fd,const char *req,const char *path){char json[65536];if(!logged_in(req)){response(fd,"401 Unauthorized","application/json","{\"error\":\"login required\"}",NULL);return;}if(!strcmp(path,"/api/status")){system_status_json(json,sizeof(json));response(fd,"200 OK","application/json",json,NULL);return;}if(!strcmp(path,"/api/processes")){processes_json(json,sizeof(json));response(fd,"200 OK","application/json",json,NULL);return;}if(!strcmp(path,"/api/ports")){ports_json(json,sizeof(json));response(fd,"200 OK","application/json",json,NULL);return;}if(!strncmp(path,"/api/signal?",12)){int pid=0,sig=0;sscanf(path,"/api/signal?pid=%d&sig=%d",&pid,&sig);int ok=process_signal(pid,sig)==0;response(fd,ok?"200 OK":"400 Bad Request","application/json",ok?"{\"ok\":true}":"{\"ok\":false}",NULL);return;}if(!strcmp(path,"/api/terminal")){terminal_api(fd,req);return;}if(!strcmp(path,"/api/shutdown")){response(fd,"200 OK","application/json","{\"ok\":true}",NULL);sync();system("/sbin/shutdown -h now");return;}if(!strcmp(path,"/api/logout")){response(fd,"200 OK","application/json","{\"ok\":true}","Set-Cookie: " PI_MONITOR_COOKIE "; Max-Age=0; Path=/; HttpOnly\r\n");return;}response(fd,"404 Not Found","application/json","{\"error\":\"not found\"}",NULL);}

void handle_connection(int fd){char req[16384]={0},method[16]={0},path[512]={0};int n=recv(fd,req,sizeof(req)-1,0);if(n<=0){close(fd);return;}sscanf(req,"%15s %511s",method,path);char *query=strchr(path,'?');if(query && strncmp(path,"/api/signal?",12)!=0)*query='\0';if(!strcmp(method,"POST")&&!strcmp(path,"/api/login"))login_api(fd,req);else if(!strcmp(path,"/")||!strcmp(path,"/login"))file_response(fd,PI_MONITOR_WEB_ROOT "/login.html","text/html; charset=utf-8");else if(!strcmp(path,"/app.css"))file_response(fd,PI_MONITOR_WEB_ROOT "/app.css","text/css");else if(!strcmp(path,"/app.js"))file_response(fd,PI_MONITOR_WEB_ROOT "/app.js","application/javascript");else if(!strcmp(path,"/dashboard")){if(logged_in(req))file_response(fd,PI_MONITOR_WEB_ROOT "/dashboard.html","text/html; charset=utf-8");else file_response(fd,PI_MONITOR_WEB_ROOT "/login.html","text/html; charset=utf-8");}else if(!strncmp(path,"/api/",5))api(fd,req,path);else response(fd,"404 Not Found","text/plain","Not found",NULL);close(fd);}
