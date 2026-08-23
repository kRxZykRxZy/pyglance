#include "http.h"
#include "config.h"
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void response(int fd,const char *status,const char *type,const char *body,const char *extra){
    char h[512];
    int n=snprintf(h,sizeof(h),"HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n%s\r\n",status,type,strlen(body),extra?extra:"");
    send(fd,h,n,0); send(fd,body,strlen(body),0);
}

static int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static void url_decode(const char *src,char *dst,size_t size){
    size_t j=0; for(size_t i=0;src[i]&&j+1<size;i++){
        if(src[i]=='%'&&src[i+1]&&src[i+2]){int a=hex(src[i+1]),b=hex(src[i+2]);if(a>=0&&b>=0){dst[j++]=(char)((a<<4)|b);i+=2;continue;}}
        dst[j++]=(src[i]=='+'?' ':src[i]);
    } dst[j]=0;
}

static int logged_in(const char *req){return strstr(req,"Cookie: " PI_MONITOR_COOKIE)!=NULL;}

static void login_page(int fd){
    const char *html="<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'><title>Pi Monitor</title><link rel=stylesheet href=/app.css></head><body><main class=login><form id=loginForm class=card><h1>Pi Monitor</h1><p>Sign in to manage this Raspberry Pi.</p><input name=username placeholder=Username autocomplete=username required><input name=password type=password placeholder=Password autocomplete=current-password required><button id=loginButton type=submit>Sign in</button><div id=error></div></form></main><script src=/app.js></script></body></html>";
    response(fd,"200 OK","text/html; charset=utf-8",html,NULL);
}

static void file_response(int fd,const char *path,const char *type){
    FILE *f=fopen(path,"rb"); char *data; long len;
    if(!f){response(fd,"404 Not Found","text/plain","Not found",NULL);return;}
    fseek(f,0,SEEK_END); len=ftell(f); rewind(f);
    if(len<0||len>1024*1024){fclose(f);response(fd,"500 Internal Server Error","text/plain","File error",NULL);return;}
    data=malloc((size_t)len+1); if(!data){fclose(f);response(fd,"500 Internal Server Error","text/plain","Memory error",NULL);return;}
    fread(data,1,(size_t)len,f); data[len]=0; fclose(f); response(fd,"200 OK",type,data,NULL); free(data);
}

static void login_api(int fd,const char *req){
    const char *body=strstr(req,"\r\n\r\n"); char raw_user[128]={0},raw_pass[256]={0},user[128],pass[256];
    if(body) body+=4;
    if(body){const char *u=strstr(body,"username=");const char *p=strstr(body,"password=");if(u)sscanf(u+9,"%127[^&]",raw_user);if(p)sscanf(p+9,"%255[^&]",raw_pass);}
    url_decode(raw_user,user,sizeof(user)); url_decode(raw_pass,pass,sizeof(pass));
    if(!strcmp(user,PI_MONITOR_USER)&&!strcmp(pass,PI_MONITOR_PASSWORD))
        response(fd,"200 OK","application/json","{\"ok\":true}","Set-Cookie: " PI_MONITOR_COOKIE "; Path=/; HttpOnly; SameSite=Strict\r\n");
    else response(fd,"401 Unauthorized","application/json","{\"ok\":false,\"error\":\"Invalid username or password\"}",NULL);
}

static void api(int fd,const char *req,const char *path){
    char json[65536];
    if(!logged_in(req)){response(fd,"401 Unauthorized","application/json","{\"error\":\"login required\"}",NULL);return;}
    if(!strcmp(path,"/api/status")){system_status_json(json,sizeof(json));response(fd,"200 OK","application/json",json,NULL);return;}
    if(!strcmp(path,"/api/processes")){processes_json(json,sizeof(json));response(fd,"200 OK","application/json",json,NULL);return;}
    if(!strcmp(path,"/api/ports")){ports_json(json,sizeof(json));response(fd,"200 OK","application/json",json,NULL);return;}
    if(!strncmp(path,"/api/signal?",12)){
        int pid=0,sig=0; sscanf(path,"/api/signal?pid=%d&sig=%d",&pid,&sig); int ok=process_signal(pid,sig)==0;
        response(fd,ok?"200 OK":"400 Bad Request","application/json",ok?"{\"ok\":true}":"{\"ok\":false}",NULL); return;
    }
    response(fd,"404 Not Found","application/json","{\"error\":\"not found\"}",NULL);
}

void handle_connection(int fd){
    char req[16384]={0},method[16]={0},path[512]={0};
    int n=recv(fd,req,sizeof(req)-1,0); if(n<=0){close(fd);return;}
    sscanf(req,"%15s %511s",method,path);
    if(!strcmp(method,"POST")&&!strcmp(path,"/api/login"))login_api(fd,req);
    else if(!strcmp(path,"/")||!strcmp(path,"/login"))login_page(fd);
    else if(!strcmp(path,"/app.css"))file_response(fd,PI_MONITOR_WEB_ROOT "/app.css","text/css");
    else if(!strcmp(path,"/app.js"))file_response(fd,PI_MONITOR_WEB_ROOT "/app.js","application/javascript");
    else if(!strcmp(path,"/dashboard")){if(logged_in(req))file_response(fd,PI_MONITOR_WEB_ROOT "/dashboard.html","text/html; charset=utf-8");else login_page(fd);}
    else if(!strncmp(path,"/api/",5))api(fd,req,path);
    else response(fd,"404 Not Found","text/plain","Not found",NULL);
    close(fd);
}
