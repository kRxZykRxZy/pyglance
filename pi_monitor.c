
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PASS "Hm361485%"
#define PORT 80
#define BUF 8192

static unsigned long long prev_total=0, prev_idle=0;

static void json_str(int fd,const char *s){
    dprintf(fd,"\"");
    for(;*s;s++){ if(*s=='"'||*s=='\\') dprintf(fd,"\\%c",*s); else if((unsigned char)*s<32)dprintf(fd," "); else dprintf(fd,"%c",*s); }
    dprintf(fd,"\"");
}
static int auth(const char *req){
    const char *p=strstr(req,"Authorization: Basic ");
    if(!p) return 0;
    p+=21;
    /* "pi:password" base64 = cGk6SG0zNjE0ODUl */
    return !strncmp(p,"cGk6SG0zNjE0ODUl",16);
}
static void headers(int fd,int code,const char *type){
    dprintf(fd,"HTTP/1.1 %d OK\r\nContent-Type: %s\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",code,type);
}
static double cpu_total(){
    FILE*f=fopen("/proc/stat","r"); if(!f)return 0;
    unsigned long long u,n,s,i,io,irq,si,st,gu,gn;
    int r=fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",&u,&n,&s,&i,&io,&irq,&si,&st,&gu,&gn); fclose(f);
    if(r<4)return 0;
    unsigned long long idle=i+io,total=u+n+s+i+io+irq+si+st+gu+gn;
    double pct=0; if(prev_total) pct=100.0*(double)(total-prev_total-(idle-prev_idle))/(double)(total-prev_total);
    prev_total=total; prev_idle=idle; return pct;
}
static void mem(int fd){
    FILE*f=fopen("/proc/meminfo","r"); unsigned long long total=0,avail=0,x; char k[64];
    while(f && fscanf(f,"%63s %llu kB",k,&x)==2){ if(!strcmp(k,"MemTotal:"))total=x*1024; else if(!strcmp(k,"MemAvailable:"))avail=x*1024; }
    if(f)fclose(f);
    dprintf(fd,"\"memory\":{\"total\":%llu,\"used\":%llu,\"available\":%llu}",total,total>avail?total-avail:0,avail);
}
static void disks(int fd){
    DIR*d=opendir("/proc/mounts"); struct statvfs v; char dev[256],mnt[256],fs[64],flags[128]; int first=1;
    dprintf(fd,"\"disks\":[");
    while(d && fscanf(d,"%255s %255s %63s %127s %*s %*s",dev,mnt,fs,flags)==4){
        if(strncmp(mnt,"/proc",5)==0||strncmp(mnt,"/sys",4)==0||strncmp(mnt,"/dev",4)==0||strncmp(mnt,"/run",4)==0)continue;
        if(statvfs(mnt,&v))continue;
        if(!first)dprintf(fd,","); first=0;
        dprintf(fd,"{\"mount\":");json_str(fd,mnt);dprintf(fd,",\"total\":%llu,\"free\":%llu}",(unsigned long long)v.f_blocks*v.f_frsize,(unsigned long long)v.f_bavail*v.f_frsize);
    }
    if(d)closedir(d); dprintf(fd,"]");
}
static void net(int fd){
    DIR*d=opendir("/sys/class/net"); struct dirent*e; int first=1; unsigned long long rx,tx; char p[256]; FILE*f;
    dprintf(fd,"\"network\":[");
    while(d&&(e=readdir(d))){
        if(e->d_name[0]=='.')continue;
        snprintf(p,sizeof p,"/sys/class/net/%s/statistics/rx_bytes",e->d_name);f=fopen(p,"r");if(!f)continue; if(fscanf(f,"%llu",&rx)!=1){fclose(f);continue;}fclose(f);
        snprintf(p,sizeof p,"/sys/class/net/%s/statistics/tx_bytes",e->d_name);f=fopen(p,"r");if(!f)continue;if(fscanf(f,"%llu",&tx)!=1){fclose(f);continue;}fclose(f);
        if(!first)dprintf(fd,",");first=0;dprintf(fd,"{\"name\":");json_str(fd,e->d_name);dprintf(fd,",\"rx\":%llu,\"tx\":%llu}",rx,tx);
    }if(d)closedir(d);dprintf(fd,"]");
}
static void processes(int fd){
    DIR*d=opendir("/proc"); struct dirent*e; int first=1; char p[128],name[256],state; unsigned long long ut,st; long rss;
    dprintf(fd,"\"processes\":[");
    while(d&&(e=readdir(d))){
        if(!isdigit((unsigned char)e->d_name[0]))continue;
        snprintf(p,sizeof p,"/proc/%s/stat",e->d_name);FILE*f=fopen(p,"r");if(!f)continue;
        int pid=atoi(e->d_name); char comm[256]; 
        if(fscanf(f,"%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %llu %llu",&pid,comm,&state,&ut,&st)!=5){fclose(f);continue;}fclose(f);
        snprintf(p,sizeof p,"/proc/%d/status",pid);f=fopen(p,"r");rss=0;
        if(f){char line[256];while(fgets(line,sizeof line,f))if(sscanf(line,"VmRSS: %ld",&rss)==1)break;fclose(f);}
        if(!first)dprintf(fd,",");first=0;
        dprintf(fd,"{\"pid\":%d,\"name\":",pid);json_str(fd,comm);dprintf(fd,",\"state\":\"%c\",\"rss_kb\":%ld,\"cpu_ticks\":%llu}",state,rss,ut+st);
    }if(d)closedir(d);dprintf(fd,"]");
}
static void api(int fd){
    headers(fd,200,"application/json"); dprintf(fd,"{\"cpu\":%.2f,",cpu_total());mem(fd);dprintf(fd,",");net(fd);dprintf(fd,",");disks(fd);dprintf(fd,",");processes(fd);dprintf(fd,"}\n");
}
static void page(int fd){
    headers(fd,200,"text/html; charset=utf-8");
    dprintf(fd,"%s", "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Pi Monitor</title><style>*{box-sizing:border-box}body{margin:0;background:#0b0d10;color:#e9edf2;font:14px system-ui;padding:16px}nav{display:flex;gap:8px;margin-bottom:14px}button{background:#171b21;color:#fff;border:1px solid #303741;border-radius:8px;padding:9px 13px}button.on{background:#fff;color:#111}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}.card{background:#11151a;border:1px solid #252b33;border-radius:12px;padding:14px}.big{font-size:28px;font-weight:700}table{width:100%;border-collapse:collapse;margin-top:12px}td,th{text-align:left;padding:8px;border-bottom:1px solid #222830}#app{max-width:1100px;margin:auto}.danger{color:#ff7777}</style><div id=app><nav><button class=on onclick='tab(0,this)'>Overview</button><button onclick='tab(1,this)'>Processes</button><button onclick='tab(2,this)'>Network</button><button onclick='tab(3,this)'>Disks</button></nav><div id=v></div></div><script>let data={};function esc(s){return String(s).replace(/[&<>\"']/g,x=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[x]))}async function load(){try{data=await (await fetch('/api')).json();render()}catch(e){}}function tab(n,b){document.querySelectorAll('button').forEach(x=>x.classList.remove('on'));b.classList.add('on');b.dataset.t=n;render(n)}function render(n=+document.querySelector('button.on').dataset.t||0){if(n==0)v.innerHTML=`<div class=grid><div class=card>CPU<div class=big>${data.cpu||0}%</div></div><div class=card>RAM<div class=big>${data.memory?((data.memory.used/data.memory.total)*100).toFixed(1):0}%</div></div><div class=card>RAM used<div class=big>${data.memory?(data.memory.used/1048576).toFixed(0):0} MB</div></div><div class=card>Processes<div class=big>${data.processes?.length||0}</div></div></div>`;if(n==1)v.innerHTML='<table><tr><th>PID</th><th>Process</th><th>State</th><th>RAM</th><th>CPU ticks</th><th></th></tr>'+data.processes.map(p=>`<tr><td>${p.pid}</td><td>${esc(p.name)}</td><td>${p.state}</td><td>${p.rss_kb} KB</td><td>${p.cpu_ticks}</td><td><button class=danger onclick=\"killp(${p.pid})\">End</button></td></tr>`).join('')+'</table>';if(n==2)v.innerHTML='<div class=grid>'+data.network.map(x=>`<div class=card>${esc(x.name)}<br>RX ${(x.rx/1048576).toFixed(1)} MB<br>TX ${(x.tx/1048576).toFixed(1)} MB</div>`).join('')+'</div>';if(n==3)v.innerHTML='<div class=grid>'+data.disks.map(x=>`<div class=card>${esc(x.mount)}<br>Total ${(x.total/1073741824).toFixed(1)} GB<br>Free ${(x.free/1073741824).toFixed(1)} GB</div>`).join('')+'</div>'}async function killp(p){if(confirm('End PID '+p+'?')){await fetch('/api/kill?pid='+p,{method:'POST'});load()}}load();setInterval(load,3000)</script>");
}
int main(){
    signal(SIGPIPE,SIG_IGN);
    int s=socket(AF_INET,SOCK_STREAM,0),one=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=INADDR_ANY;a.sin_port=htons(PORT);
    if(bind(s,(struct sockaddr*)&a,sizeof a)||listen(s,16))return 1;
    for(;;){int c=accept(s,0,0);if(c<0)continue;char r[BUF]={0};int n=read(c,r,sizeof r-1);if(!auth(r)){dprintf(c,"HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Pi Monitor\"\r\nContent-Length:0\r\n\r\n");close(c);continue;}
        if(!strncmp(r,"GET /api ",8))api(c);
        else if(!strncmp(r,"POST /api/kill?pid=",19)){int pid=atoi(r+19);if(pid>1)kill(pid,SIGTERM);headers(c,200,"application/json");dprintf(c,"{\"ok\":true}");}
        else page(c); close(c);
    }
}
