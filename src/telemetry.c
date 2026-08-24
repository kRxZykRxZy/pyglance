#include "telemetry.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

typedef struct { double cpu,ram,temp,load1; unsigned long long mem_total,mem_free,mem_available,swap_total,swap_free; time_t ts; } telemetry_t;
static telemetry_t t;
static double number(const char *s){char *e;double v=strtod(s,&e);return e==s?0:v;}
static unsigned long long kv(const char *key){FILE*f=fopen("/proc/meminfo","r");if(!f)return 0;char line[160],k[64];unsigned long long v;while(fgets(line,sizeof(line),f)){if(sscanf(line,"%63[^:]: %llu",k,&v)==2&&strcmp(k,key)==0){fclose(f);return v*1024ULL;}}fclose(f);return 0;}
void telemetry_collect(void){t.mem_total=kv("MemTotal");t.mem_free=kv("MemFree");t.mem_available=kv("MemAvailable");t.swap_total=kv("SwapTotal");t.swap_free=kv("SwapFree");FILE*f=fopen("/proc/loadavg","r");if(f){fscanf(f,"%lf",&t.load1);fclose(f);}t.ram=t.mem_total?t.mem_available?100.0*(double)(t.mem_total-t.mem_available)/(double)t.mem_total:0:0;t.ts=time(NULL);}
void telemetry_json(char*out,size_t n){double ram=t.ram;if(ram<0)ram=0;if(ram>100)ram=100;snprintf(out,n,"{\"ram_percent\":%.1f,\"ram_total\":%llu,\"ram_free\":%llu,\"ram_available\":%llu,\"swap_total\":%llu,\"swap_free\":%llu,\"load1\":%.2f,\"timestamp\":%lld}",ram,t.mem_total,t.mem_free,t.mem_available,t.swap_total,t.swap_free,t.load1,(long long)t.ts);}
