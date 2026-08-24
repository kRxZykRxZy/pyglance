#include "system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static unsigned long long prev_total[256],prev_idle[256];
static int read_core_freq(int id,const char *name){char p[160],b[64];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/%s",id,name);FILE*f=fopen(p,"r");if(!f)return 0;if(!fgets(b,sizeof(b),f)){fclose(f);return 0;}fclose(f);unsigned long v=strtoul(b,NULL,10);return v>10000?v/1000:v;}
static void governor(int id,char*out,size_t n){char p[160],b[64];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",id);FILE*f=fopen(p,"r");if(!f){snprintf(out,n,"unknown");return;}if(!fgets(b,sizeof(b),f)){fclose(f);snprintf(out,n,"unknown");return;}fclose(f);b[strcspn(b,"\r\n")]=0;snprintf(out,n,"%s",b);}
void cpu_cores_json(char *out,size_t size){FILE*f=fopen("/proc/stat","r");if(!f){snprintf(out,size,"[]");return;}char line[320];size_t u=0;int first=1;u+=snprintf(out+u,size-u,"[");while(fgets(line,sizeof(line),f)&&u+500<size){unsigned long long user,nice,sys,idle,io,irq,soft,steal,guest,gnt;int id;if(sscanf(line,"cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",&id,&user,&nice,&sys,&idle,&io,&irq,&soft,&steal,&guest,&gnt)>=8){unsigned long long total=user+nice+sys+idle+io+irq+soft+steal;unsigned long long idleall=idle+io;double pct=0;if(prev_total[id]&&total>prev_total[id]){unsigned long long dt=total-prev_total[id],di=idleall-prev_idle[id];pct=100.0*(double)(dt>di?dt-di:0)/(double)dt;}prev_total[id]=total;prev_idle[id]=idleall;char gov[32];governor(id,gov,sizeof(gov));unsigned long cur=read_core_freq(id,"scaling_cur_freq"),max=read_core_freq(id,"scaling_max_freq");u+=snprintf(out+u,size-u,"%s{\"core\":%d,\"usage\":%.1f,\"user\":%llu,\"nice\":%llu,\"system\":%llu,\"idle\":%llu,\"iowait\":%llu,\"irq\":%llu,\"softirq\":%llu,\"frequency_mhz\":%lu,\"max_frequency_mhz\":%lu,\"governor\":\"%s\"}",first?"":",",id,pct,user,nice,sys,idle,io,irq,soft,cur,max,gov);first=0;}}fclose(f);snprintf(out+u,size-u,"]");}
