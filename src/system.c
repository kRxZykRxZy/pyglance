#include "system.h"
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

static unsigned long long mem_value(const char *wanted) {
    FILE *f=fopen("/proc/meminfo","r"); char line[128], name[64]; unsigned long long value;
    if(!f)return 0;
    while(fgets(line,sizeof(line),f)){
        if(sscanf(line,"%63[^:]: %llu",name,&value)==2&&!strcmp(name,wanted)){fclose(f);return value*1024ULL;}
    }
    fclose(f); return 0;
}

static double cpu_percent(void){
    FILE *f=fopen("/proc/stat","r"); unsigned long long u,n,s,i,w,x,y,z; static unsigned long long old_total,old_idle; double result=0;
    if(!f)return 0;
    if(fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",&u,&n,&s,&i,&w,&x,&y,&z)==8){
        unsigned long long total=u+n+s+i+w+x+y+z,idle=i+w,dt=total-old_total,di=idle-old_idle;
        if(old_total&&dt)result=100.0*(double)(dt-di)/(double)dt;
        old_total=total; old_idle=idle;
    }
    fclose(f); return result;
}

static unsigned long long uptime_seconds(void){FILE *f=fopen("/proc/uptime","r");double u=0;if(!f)return 0;fscanf(f,"%lf",&u);fclose(f);return (unsigned long long)u;}

static void network_bytes(unsigned long long *rx,unsigned long long *tx){
    FILE *f=fopen("/proc/net/dev","r"); char line[512],iface[64]; unsigned long long a[16]; *rx=*tx=0;
    if(!f)return;
    while(fgets(line,sizeof(line),f)){
        if(sscanf(line," %63[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",iface,&a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&a[6],&a[7],&a[8],&a[9],&a[10],&a[11],&a[12],&a[13],&a[14],&a[15])==17&&strcmp(iface,"lo")){*rx+=a[0];*tx+=a[8];}
    }
    fclose(f);
}

void system_status_json(char *out,size_t size){
    struct statvfs fs; unsigned long long total=0,free_space=0,ram_total=mem_value("MemTotal"),ram_available=mem_value("MemAvailable"),rx,tx;
    if(!statvfs("/",&fs)){total=(unsigned long long)fs.f_blocks*fs.f_frsize;free_space=(unsigned long long)fs.f_bavail*fs.f_frsize;}
    network_bytes(&rx,&tx); unsigned long long used=ram_total>ram_available?ram_total-ram_available:0;
    double disk=total?100.0*(double)(total-free_space)/(double)total:0;
    snprintf(out,size,"{\"cpu\":%.1f,\"ram_total\":%llu,\"ram_used\":%llu,\"ram_percent\":%.1f,\"disk_percent\":%.1f,\"uptime\":%llu,\"rx\":%llu,\"tx\":%llu}",cpu_percent(),ram_total,used,ram_total?100.0*used/ram_total:0,disk,uptime_seconds(),rx,tx);
}

void processes_json(char *out,size_t size){
    DIR *d=opendir("/proc");struct dirent *e;size_t used=0;int first=1;used+=snprintf(out+used,size-used,"[");
    if(!d){snprintf(out+used,size-used,"]");return;}
    while((e=readdir(d))&&used+256<size){
        if(e->d_type!=DT_DIR)continue;char *end;long pid=strtol(e->d_name,&end,10);if(*end||pid<=0)continue;
        char path[64],name[128],state='?';snprintf(path,sizeof(path),"/proc/%ld/stat",pid);FILE *f=fopen(path,"r");if(!f)continue;
        if(fscanf(f,"%*d (%127[^)]) %c",name,&state)!=2){fclose(f);continue;}fclose(f);
        used+=snprintf(out+used,size-used,"%s{\"pid\":%ld,\"name\":\"%s\",\"state\":\"%c\"}",first?"":",",pid,name,state);first=0;
    }
    closedir(d);snprintf(out+used,size-used,"]");
}

void ports_json(char *out,size_t size){
    FILE *f=fopen("/proc/net/tcp","r");char line[512];unsigned int local;size_t used=0;int first=1;used+=snprintf(out+used,size-used,"[");
    if(f){fgets(line,sizeof(line),f);while(fgets(line,sizeof(line),f)&&used+128<size){if(sscanf(line," %*d: %*8X:%X",&local)==1){used+=snprintf(out+used,size-used,"%s{\"proto\":\"tcp\",\"port\":%u}",first?"":",",local);first=0;}}fclose(f);}snprintf(out+used,size-used,"]");
}

int process_signal(int pid,int sig){if(pid<=1||(sig!=SIGSTOP&&sig!=SIGCONT&&sig!=SIGTERM&&sig!=SIGKILL))return -1;return kill(pid,sig);}
