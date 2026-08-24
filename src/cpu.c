#include "system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
void cpu_cores_json(char *out,size_t size){FILE*f=fopen("/proc/stat","r");if(!f){snprintf(out,size,"[]");return;}char line[256];size_t u=0;int first=1;u+=snprintf(out+u,size-u,"[");while(fgets(line,sizeof(line),f)&&u+220<size){unsigned long long user,nice,sys,idle,io,irq,soft,steal;int id;if(sscanf(line,"cpu%d %llu %llu %llu %llu %llu %llu %llu %llu",&id,&user,&nice,&sys,&idle,&io,&irq,&soft,&steal)==9){char gov[32]="unknown";char p[128];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",id);FILE*g=fopen(p,"r");if(g){fgets(gov,sizeof(gov),g);fclose(g);gov[strcspn(gov,"\r\n")]=0;}u+=snprintf(out+u,size-u,"%s{\"core\":%d,\"user\":%llu,\"system\":%llu,\"idle\":%llu,\"iowait\":%llu,\"governor\":\"%s\"}",first?"":",",id,user,sys,idle,io,gov);first=0;}}fclose(f);snprintf(out+u,size-u,"]");}
