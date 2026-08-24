#include "firewall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

static void run_capture(const char *cmd,char *out,size_t n){FILE*f=popen(cmd,"r");if(!f){out[0]=0;return;}size_t z=fread(out,1,n-1,f);out[z]=0;pclose(f);}
static void esc_json(const char *in,char *out,size_t n){size_t j=0;for(size_t i=0;in[i]&&j+2<n;i++){unsigned char c=(unsigned char)in[i];if(c=='\\'||c=='\"')out[j++]='\\';if(c=='\n'){if(j+2>=n)break;out[j++]='\\';out[j++]='n';}else if(c=='\r'){}else if(c<32){out[j++]=' ';}else out[j++]=(char)c;}out[j]=0;}
void firewall_json(char *out,size_t n){
 char backend[32]="none",status[32]="not-detected",rules[12288]="",esc[12288]="";
 char test[128];
 run_capture("command -v nft 2>/dev/null",test,sizeof(test));
 if(test[0]){strcpy(backend,"nftables");run_capture("nft list ruleset 2>/dev/null",rules,sizeof(rules));if(rules[0])strcpy(status,"active");else strcpy(status,"inactive");}
 else{
  run_capture("command -v iptables 2>/dev/null",test,sizeof(test));
  if(test[0]){strcpy(backend,"iptables");run_capture("iptables -S 2>/dev/null",rules,sizeof(rules));if(rules[0]){int restrictive=0;for(char*p=rules;*p;p++){if(!strncmp(p,"-P ",3)&&(strstr(p," DROP")||strstr(p," REJECT")))restrictive=1;if(*p=='\n'&&p[1]=='-'&&p[2]=='A')restrictive=1;}strcpy(status,restrictive?"active":"inactive");}else strcpy(status,"unavailable");}
  else{run_capture("command -v ufw 2>/dev/null",test,sizeof(test));if(test[0]){strcpy(backend,"ufw");run_capture("ufw status verbose 2>/dev/null",rules,sizeof(rules));if(strstr(rules,"Status: active"))strcpy(status,"active");else if(strstr(rules,"Status: inactive"))strcpy(status,"inactive");else strcpy(status,"unavailable");}}
 }
 esc_json(rules,esc,sizeof(esc));snprintf(out,n,"{\"status\":\"%s\",\"backend\":\"%s\",\"rules\":\"%s\"}",status,backend,esc);
}
