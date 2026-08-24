#include "firewall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void clean(char *s){s[strcspn(s,"\r\n")]=0;}
void firewall_json(char *out,size_t n){
 char status[64]="unknown",backend[32]="unknown",rules[8192]="";
 FILE *f=popen("command -v nft >/dev/null 2>&1 && nft list ruleset 2>/dev/null || true","r");
 if(f){size_t z=fread(rules,1,sizeof(rules)-1,f);rules[z]=0;pclose(f);if(z){strcpy(backend,"nftables");strcpy(status,"active");}}
 if(!strcmp(status,"unknown")){f=popen("command -v iptables >/dev/null 2>&1 && iptables -S 2>/dev/null || true","r");if(f){size_t z=fread(rules,1,sizeof(rules)-1,f);rules[z]=0;pclose(f);if(z){strcpy(backend,"iptables");strcpy(status,"active");}}}
 if(!strcmp(status,"unknown")){f=popen("command -v ufw >/dev/null 2>&1 && ufw status 2>/dev/null || true","r");if(f){size_t z=fread(rules,1,sizeof(rules)-1,f);rules[z]=0;pclose(f);if(z){strcpy(backend,"ufw");strcpy(status,strstr(rules,"Status: active")?"active":"inactive");}}}
 size_t j=0;char esc[8192];for(size_t i=0;rules[i]&&j+2<sizeof(esc);i++){if(rules[i]=='\\'||rules[i]=='\"')esc[j++]='\\';if(rules[i]=='\n'){esc[j++]='\\';esc[j++]='n';}else if(rules[i]!='\r')esc[j++]=rules[i];}esc[j]=0;
 snprintf(out,n,"{\"status\":\"%s\",\"backend\":\"%s\",\"rules\":\"%s\"}",status,backend,esc);
}
