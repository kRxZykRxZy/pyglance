cd /mnt/drive/pyglance

cat > pi_monitor.c <<'EOF'
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 80
#define USERNAME "admin"
#define PASSWORD "Hm361485%"

#define MAX_REQ 8192
#define MAX_RESP 65536

static unsigned long long last_total;
static unsigned long long last_idle;
static double cpu_usage;

static const char *HTML =
"<!doctype html><html><head>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Pi Monitor</title>"
"<style>"
"*{box-sizing:border-box}"
"body{margin:0;background:#0b0f14;color:#e7edf5;font:14px system-ui,-apple-system,sans-serif}"
"header{padding:18px 22px;background:#111720;border-bottom:1px solid #26303d;display:flex;justify-content:space-between;align-items:center}"
"h1{font-size:20px;margin:0}"
".online{font-size:12px;color:#6ee7a0}"
"nav{display:flex;gap:6px;padding:12px 18px;background:#0e131b;border-bottom:1px solid #26303d;overflow:auto}"
"button,.tab{border:1px solid #303b4b;background:#151c26;color:#dce5ef;padding:9px 13px;border-radius:8px;cursor:pointer}"
".tab.active{background:#263448;border-color:#4b6280}"
"main{padding:18px;max-width:1300px;margin:auto}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}"
".card{background:#111720;border:1px solid #26303d;border-radius:12px;padding:16px}"
".label{color:#8492a5;font-size:12px;margin-bottom:6px}"
".value{font-size:25px;font-weight:600}"
".bar{height:7px;background:#202a36;border-radius:8px;margin-top:12px;overflow:hidden}"
".fill{height:100%;background:#62a8ff;border-radius:8px}"
"section{display:none}.show{display:block}"
"table{width:100%;border-collapse:collapse;margin-top:12px;background:#111720;border:1px solid #26303d;border-radius:12px;overflow:hidden}"
"th,td{text-align:left;padding:10px;border-bottom:1px solid #202a35;font-size:13px}"
"th{color:#8d9bad;background:#151c25}"
".danger{background:#36191d;border-color:#6b2b32;color:#ff9ba3}"
".warn{background:#332b16;border-color:#6a5a28;color:#ffd66b}"
".muted{color:#8290a2}"
".login{position:fixed;inset:0;background:#090d12;display:flex;align-items:center;justify-content:center}"
".box{width:330px;padding:25px;background:#111720;border:1px solid #293544;border-radius:14px}"
"input{width:100%;padding:11px;margin:7px 0;background:#0b1016;border:1px solid #303b49;color:white;border-radius:8px}"
".box button{width:100%;margin-top:8px}"
".err{color:#ff8992;margin-top:10px}"
"@media(max-width:600px){main{padding:12px}th,td{padding:7px;font-size:12px}}"
"</style></head><body>"

"<div id=login class=login><div class=box>"
"<h2>Pi Monitor</h2>"
"<p class=muted>Administrator login</p>"
"<input id=user placeholder=Username autocomplete=username>"
"<input id=pass type=password placeholder=Password autocomplete=current-password>"
"<button onclick=login()>Sign in</button>"
"<div id=err class=err></div></div></div>"

"<div id=app style='display:none'>"
"<header><h1>Pi Monitor</h1><span class=online>● ONLINE</span></header>"
"<nav>"
"<button class='tab active' onclick=tab('overview',this)>Overview</button>"
"<button class=tab onclick=tab('processes',this)>Processes</button>"
"<button class=tab onclick=tab('ports',this)>Ports</button>"
"<button class=tab onclick=tab('network',this)>Network</button>"
"</nav>"

"<main>"
"<section id=overview class=show>"
"<div class=grid>"
"<div class=card><div class=label>CPU</div><div class=value id=cpu>--</div><div class=bar><div id=cpub class=fill></div></div></div>"
"<div class=card><div class=label>RAM</div><div class=value id=ram>--</div><div class=bar><div id=ramb class=fill></div></div></div>"
"<div class=card><div class=label>Disk</div><div class=value id=disk>--</div><div class=bar><div id=diskb class=fill></div></div></div>"
"<div class=card><div class=label>Uptime</div><div class=value id=uptime>--</div></div>"
"</div></section>"

"<section id=processes>"
"<div class=card><b>Running processes</b><div id=procs>Loading...</div></div>"
"</section>"

"<section id=ports>"
"<div class=card><b>Open network ports</b><div id=portslist>Loading...</div></div>"
"</section>"

"<section id=network>"
"<div class=grid>"
"<div class=card><div class=label>Received</div><div class=value id=rx>--</div></div>"
"<div class=card><div class=label>Transmitted</div><div class=value id=tx>--</div></div>"
"</div></section>"
"</main></div>"

"<script>"
"let auth='';"
"function login(){"
" let u=btoa(document.getElementById('user').value+':'+document.getElementById('pass').value);"
" fetch('/api/status',{headers:{Authorization:'Basic '+u}}).then(r=>{"
" if(!r.ok)throw 0;auth=u;document.getElementById('login').style.display='none';document.getElementById('app').style.display='block';refresh();"
" }).catch(()=>document.getElementById('err').textContent='Invalid username or password');"
"}"
"function api(u,o={}){o.headers={...(o.headers||{}),Authorization:'Basic '+auth};return fetch(u,o).then(r=>r.json())}"
"function tab(id,b){document.querySelectorAll('section').forEach(x=>x.classList.remove('show'));document.getElementById(id).classList.add('show');document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));b.classList.add('active');if(id==='processes')loadProc();if(id==='ports')loadPorts()}"
"function mb(x){return (x/1048576).toFixed(0)+' MB'}"
"function refresh(){api('/api/status').then(x=>{"
"document.getElementById('cpu').textContent=x.cpu.toFixed(1)+'%';document.getElementById('cpub').style.width=x.cpu+'%';"
"document.getElementById('ram').textContent=mb(x.ram_used)+' / '+mb(x.ram_total);document.getElementById('ramb').style.width=x.ram_percent+'%';"
"document.getElementById('disk').textContent=x.disk_percent.toFixed(1)+'%';document.getElementById('diskb').style.width=x.disk_percent+'%';"
"document.getElementById('uptime').textContent=x.uptime;"
"document.getElementById('rx').textContent=mb(x.rx)+' total';document.getElementById('tx').textContent=mb(x.tx)+' total';"
"}).catch(()=>{});}"
"function loadProc(){api('/api/processes').then(x=>{let s='<table><tr><th>PID</th><th>Name</th><th>State</th><th>Action</th></tr>';x.forEach(p=>{s+=`<tr><td>${p.pid}</td><td>${p.name}</td><td>${p.state}</td><td><button onclick=\"sig(${p.pid},19)\">Stop</button> <button onclick=\"sig(${p.pid},18)\">Resume</button> <button class=danger onclick=\"sig(${p.pid},15)\">Kill</button></td></tr>`});s+='</table>';document.getElementById('procs').innerHTML=s})}"
"function sig(pid,s){if(confirm(s===15?'Kill process '+pid+'?':s===19?'Stop process '+pid+'?':'Resume process '+pid+'?'))api('/api/signal?pid='+pid+'&sig='+s,{method:'POST'}).then(loadProc)}"
"function loadPorts(){api('/api/ports').then(x=>{let s='<table><tr><th>Protocol</th><th>Local</th><th>PID</th><th>Process</th><th>Action</th></tr>';x.forEach(p=>{s+=`<tr><td>${p.proto}</td><td>${p.addr}:${p.port}</td><td>${p.pid||'-'}</td><td>${p.name||'-'}</td><td>${p.pid?`<button class=danger onclick=\"sig(${p.pid},15)\">Stop owner</button>`:''}</td></tr>`});s+='</table>';document.getElementById('portslist').innerHTML=s})}"
"setInterval(refresh,3000);"
"</script></body></html>";

static unsigned long long cpu_total(void) {
    FILE *f=fopen("/proc/stat","r");
    if(!f)return 0;
    unsigned long long a,b,c,d,e,g,h,i;
    int ok=fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                  &a,&b,&c,&d,&e,&g,&h,&i);
    fclose(f);
    if(ok!=8)return 0;
    return a+b+c+d+e+g+h+i;
}

static unsigned long long cpu_idle(void) {
    FILE *f=fopen("/proc/stat","r");
    if(!f)return 0;
    unsigned long long a,b,c,d,e,g,h,i;
    int ok=fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                  &a,&b,&c,&d,&e,&g,&h,&i);
    fclose(f);
    if(ok!=8)return 0;
    return d+e;
}

static void update_cpu(void) {
    unsigned long long t=cpu_total(), id=cpu_idle();
    if(last_total) {
        unsigned long long dt=t-last_total;
        unsigned long long di=id-last_idle;
        if(dt) cpu_usage=100.0*(double)(dt-di)/(double)dt;
        if(cpu_usage<0)cpu_usage=0;
        if(cpu_usage>100)cpu_usage=100;
    }
    last_total=t;
    last_idle=id;
}

static unsigned long long mem_value(const char *wanted) {
    FILE *f=fopen("/proc/meminfo","r");
    if(!f)return 0;
    char key[64];
    unsigned long long value;
    while(fscanf(f,"%63s %llu",key,&value)==2) {
        if(!strcmp(key,wanted)) {
            fclose(f);
            return value*1024ULL;
        }
        char line[64];
        fgets(line,sizeof(line),f);
    }
    fclose(f);
    return 0;
}

static unsigned long long net_bytes(int rx) {
    FILE *f=fopen("/proc/net/dev","r");
    if(!f)return 0;
    char line[512],iface[64];
    unsigned long long a,b,c,d,e,g,h,i,j,k,l,m,n,o,p;
    unsigned long long total=0;

    fgets(line,sizeof(line),f);
    fgets(line,sizeof(line),f);

    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line," %63[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            iface,&a,&b,&c,&d,&e,&g,&h,&i,&j,&k,&l,&m,&n,&o,&p)==17) {
            if(!strcmp(iface,"lo"))continue;
            total += rx?a:i;
        }
    }
    fclose(f);
    return total;
}

static void json_status(int fd) {
    struct sysinfo si;
    sysinfo(&si);

    unsigned long long total=mem_value("MemTotal:");
    unsigned long long avail=mem_value("MemAvailable:");
    unsigned long long used=total>avail?total-avail:0;

    struct statvfs st;
    statvfs("/",&st);

    unsigned long long dtotal=(unsigned long long)st.f_blocks*st.f_frsize;
    unsigned long long dfree=(unsigned long long)st.f_bavail*st.f_frsize;
    unsigned long long duse=dtotal>dfree?dtotal-dfree:0;

    double rp=total?100.0*(double)used/(double)total:0;
    double dp=dtotal?100.0*(double)duse/(double)dtotal:0;

    char out[4096];

    snprintf(out,sizeof(out),
        "{\"cpu\":%.2f,\"ram_total\":%llu,\"ram_used\":%llu,"
        "\"ram_percent\":%.2f,\"disk_percent\":%.2f,"
        "\"rx\":%llu,\"tx\":%llu,\"uptime\":%llu}",
        cpu_usage,total,used,rp,dp,
        net_bytes(1),net_bytes(0),(unsigned long long)si.uptime);

    dprintf(fd,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
               "Cache-Control: no-store\r\nConnection: close\r\n"
               "Content-Length: %zu\r\n\r\n%s",strlen(out),out);
}

static int auth_ok(const char *req) {
    /*
     * Browser sends HTTP Basic authentication.
     * We intentionally keep the credentials in the executable as requested.
     */
    char expected[256];
    snprintf(expected,sizeof(expected),"%s:%s",USERNAME,PASSWORD);

    const char *p=strstr(req,"Authorization: Basic ");
    if(!p)p=strstr(req,"authorization: Basic ");
    if(!p)return 0;

    p+=21;

    char encoded[256]={0};
    int i=0;

    while(*p && *p!='\r' && *p!='\n' && i<255)
        encoded[i++]=*p++;

    encoded[i]=0;

    /* Minimal Base64 decoder */
    const char *b64="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char decoded[256]={0};
    int val=0,bits=-8,pos=0;

    for(i=0;encoded[i] && pos<255;i++) {
        if(encoded[i]=='=')break;
        const char *q=strchr(b64,encoded[i]);
        if(!q)return 0;
        val=(val<<6)+(q-b64);
        bits+=6;
        if(bits>=0) {
            decoded[pos++]=(char)((val>>bits)&255);
            bits-=8;
        }
    }

    decoded[pos]=0;
    return strcmp(decoded,expected)==0;
}

static void unauthorized(int fd) {
    const char *body="Authentication required";
    dprintf(fd,"HTTP/1.1 401 Unauthorized\r\n"
               "WWW-Authenticate: Basic realm=\"Pi Monitor\"\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
               strlen(body),body);
}

static void notfound(int fd) {
    const char *body="{\"error\":\"not found\"}";
    dprintf(fd,"HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n"
               "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
               strlen(body),body);
}

static void bad(int fd,const char *msg) {
    char body[256];
    snprintf(body,sizeof(body),"{\"error\":\"%s\"}",msg);
    dprintf(fd,"HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
               "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
               strlen(body),body);
}

static void processes(int fd) {
    DIR *d=opendir("/proc");
    if(!d){bad(fd,"proc");return;}

    char *buf=malloc(60000);
    if(!buf){closedir(d);bad(fd,"memory");return;}

    int pos=0;
    buf[pos++]='[';
    int first=1;

    struct dirent *e;

    while((e=readdir(d))) {
        if(e->d_type!=DT_DIR)continue;

        int pid=atoi(e->d_name);
        if(pid<=0)continue;

        char path[128],name[128]="?",state='?';

        snprintf(path,sizeof(path),"/proc/%d/stat",pid);
        FILE *f=fopen(path,"r");
        if(!f)continue;

        char line[512];
        if(fgets(line,sizeof(line),f)) {
            char *lp=strchr(line,'(');
            char *rp=strrchr(line,')');

            if(lp&&rp&&rp>lp) {
                size_t n=(size_t)(rp-lp-1);
                if(n>=sizeof(name))n=sizeof(name)-1;
                memcpy(name,lp+1,n);
                name[n]=0;

                if(rp[2])state=rp[2];
            }
        }
        fclose(f);

        int n=snprintf(buf+pos,60000-pos,
            "%s{\"pid\":%d,\"name\":\"%s\",\"state\":\"%c\"}",
            first?"":",",pid,name,state);

        if(n<0||pos+n>=59990)break;
        pos+=n;
        first=0;
    }

    buf[pos++]=']';
    buf[pos]=0;

    closedir(d);

    dprintf(fd,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
               "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
               pos,buf);

    free(buf);
}

static void signal_process(int fd,const char *path) {
    const char *p=strstr(path,"pid=");
    const char *s=strstr(path,"sig=");

    if(!p||!s){bad(fd,"missing pid or sig");return;}

    int pid=atoi(p+4);
    int sig=atoi(s+4);

    /*
     * Only allow the process-control signals exposed by the UI.
     */
    if(sig!=SIGSTOP && sig!=SIGCONT && sig!=SIGTERM) {
        bad(fd,"signal not allowed");
        return;
    }

    if(pid<=1 || pid==getpid()) {
        bad(fd,"protected process");
        return;
    }

    if(kill(pid,sig)!=0) {
        bad(fd,strerror(errno));
        return;
    }

    dprintf(fd,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
               "Content-Length: 15\r\nConnection: close\r\n\r\n{\"ok\":true}");
}

static void ports(int fd) {
    /*
     * Lightweight /proc/net/tcp + tcp6 + udp + udp6 reader.
     * Mapping sockets to processes is intentionally only done when
     * this endpoint is requested, keeping background CPU usage tiny.
     */

    char *out=malloc(60000);
    if(!out){bad(fd,"memory");return;}

    int pos=0,first=1;
    out[pos++]='[';

    const char *files[]={
        "/proc/net/tcp",
        "/proc/net/tcp6",
        "/proc/net/udp",
        "/proc/net/udp6"
    };

    for(int fi=0;fi<4;fi++) {
        FILE *f=fopen(files[fi],"r");
        if(!f)continue;

        char line[512];
        fgets(line,sizeof(line),f);

        while(fgets(line,sizeof(line),f)) {
            unsigned int local_port,state;
            char local[64],remote[64];

            int n=sscanf(line," %*d: %63s %63s %x",local,remote,&state);
            if(n!=3)continue;

            char *colon=strrchr(local,':');
            if(!colon)continue;

            local_port=strtoul(colon+1,NULL,16);

            const char *proto=(fi<2)?"tcp":"udp";

            n=snprintf(out+pos,60000-pos,
                "%s{\"proto\":\"%s\",\"addr\":\"0.0.0.0\","
                "\"port\":%u,\"pid\":0,\"name\":\"\"}",
                first?"":",",proto,local_port);

            if(n<0||pos+n>=59900)break;

            pos+=n;
            first=0;
        }

        fclose(f);
    }

    out[pos++]=']';
    out[pos]=0;

    dprintf(fd,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
               "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
               pos,out);

    free(out);
}

static void handle(int fd) {
    char req[MAX_REQ+1];
    int n=recv(fd,req,MAX_REQ,0);

    if(n<=0)return;
    req[n]=0;

    char method[16],path[2048];

    if(sscanf(req,"%15s %2047s",method,path)!=2) {
        bad(fd,"request");
        return;
    }

    if(strcmp(path,"/")!=0 && !auth_ok(req)) {
        unauthorized(fd);
        return;
    }

    if(!strcmp(path,"/")) {
        dprintf(fd,"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                   "Cache-Control: no-store\r\nContent-Length: %zu\r\n"
                   "Connection: close\r\n\r\n%s",
                   strlen(HTML),HTML);
    }
    else if(!strcmp(path,"/api/status")) {
        update_cpu();
        json_status(fd);
    }
    else if(!strcmp(path,"/api/processes")) {
        processes(fd);
    }
    else if(!strcmp(path,"/api/ports")) {
        ports(fd);
    }
    else if(!strncmp(path,"/api/signal?",12) && !strcmp(method,"POST")) {
        signal_process(fd,path);
    }
    else {
        notfound(fd);
    }
}

int main(void) {
    /*
     * Prime CPU counters.
     */
    last_total=cpu_total();
    last_idle=cpu_idle();

    int server=socket(AF_INET,SOCK_STREAM,0);
    if(server<0) {
        perror("socket");
        return 1;
    }

    int yes=1;
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(PORT);

    if(bind(server,(struct sockaddr*)&addr,sizeof(addr))<0) {
        perror("bind");
        close(server);
        return 1;
    }

    if(listen(server,8)<0) {
        perror("listen");
        close(server);
        return 1;
    }

    printf("Pi Monitor listening on port %d\n",PORT);
    fflush(stdout);

    while(1) {
        int client=accept(server,NULL,NULL);
        if(client<0) {
            if(errno==EINTR)continue;
            continue;
        }

        /*
         * One request at a time keeps memory and CPU extremely low.
         */
        handle(client);
        close(client);
    }

    close(server);
    return 0;
}
EOF
