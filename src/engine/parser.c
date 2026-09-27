#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#define MAXV 256
#define MAXLINE 4096

struct V { char n[64]; char v[1024]; };
static struct V vars[MAXV]; static int vc;

static char *dupstr(const char*s){size_t n=strlen(s)+1;char*p=(char*)malloc(n);if(p)memcpy(p,s,n);return p;}
static char *trim(char*s){char*e;while(*s&&isspace((unsigned char)*s))s++;e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))--e;*e=0;return s;}
static struct V*findv(const char*n){for(int i=0;i<vc;i++)if(!strcmp(vars[i].n,n))return &vars[i];return NULL;}
static void setv(const char*n,const char*v){struct V*x=findv(n);if(!x&&vc<MAXV){x=&vars[vc++];strncpy_s(x->n,sizeof(x->n),n,_TRUNCATE);}if(x)strncpy_s(x->v,sizeof(x->v),v,_TRUNCATE);}

static int is_number(const char*s){if(!*s)return 0;char*e;strtod(s,&e);return *e==0;}
static void eval(const char*e,char*out,size_t cap){
 char b[4096];out[0]=0;strncpy_s(b,sizeof(b),e,_TRUNCATE);
 char*part=b;for(;;){char*q=strchr(part,'+');if(q)*q=0;part=trim(part);
  if(*part=='"'&&strlen(part)>=2&&part[strlen(part)-1]=='"'){part[strlen(part)-1]=0;part++;strcat_s(out,cap,part);}
  else {struct V*v=findv(part);if(v)strcat_s(out,cap,v->v);else strcat_s(out,cap,part);}
  if(!q)break;part=q+1;
 }
}
static void printline(char*s){char o[4096];eval(trim(s+12),o,sizeof(o));printf("%s\n",o);}
static int value_num(const char*s){struct V*v=findv(s);return v?atoi(v->v):atoi(s);}
static int condition(char*s){
 char left[256],right[256];char* p;int a,b;
 if((p=strstr(s," is greater than "))) { *p=0;strcpy_s(left,sizeof(left),trim(s));strcpy_s(right,sizeof(right),trim(p+17));return value_num(left)>value_num(right);}
 if((p=strstr(s," is less than "))) { *p=0;strcpy_s(left,sizeof(left),trim(s));strcpy_s(right,sizeof(right),trim(p+14));return value_num(left)<value_num(right);}
 if((p=strstr(s," is equal to "))) { *p=0;strcpy_s(left,sizeof(left),trim(s));strcpy_s(right,sizeof(right),trim(p+13));return !strcmp(findv(left)?findv(left)->v:left,right);}
 if((p=strstr(s," is not equal to "))) { *p=0;strcpy_s(left,sizeof(left),trim(s));strcpy_s(right,sizeof(right),trim(p+17));return strcmp(findv(left)?findv(left)->v:left,right)!=0;}
 a=value_num(trim(s));b=0;return a!=b;
}

int run_eplus(const char*src){
 char*c=dupstr(src),*ctx=NULL,*line,*ls[4096];int n=0;
 if(!c)return 1;vc=0;srand((unsigned)time(NULL));
 line=strtok_s(c,"\r\n",&ctx);while(line&&n<4096){ls[n++]=line;line=strtok_s(NULL,"\r\n",&ctx);}
 for(int i=0;i<n;i++){
  char*s=trim(ls[i]);if(!*s||*s=='#')continue;
  if(!strncmp(s,"print words ",12)){printline(s);continue;}
  if(!strncmp(s,"ask user ",9)){
   char prompt[2048],name[64];char*and=strstr(s," and save answer as ");
   if(!and){fprintf(stderr,"E#+ error: invalid ask statement: %s\n",s);free(c);return 1;}
   char tmp[MAXLINE];strncpy_s(tmp,sizeof(tmp),s+9,_TRUNCATE);char*save=strstr(tmp," and save answer as ");
   if(!save){free(c);return 1;}*save=0;strcpy_s(name,sizeof(name),trim(save+21));eval(trim(tmp),prompt,sizeof(prompt));
   char answer[1024];printf("%s",prompt);if(!fgets(answer,sizeof(answer),stdin))answer[0]=0;answer[strcspn(answer,"\r\n")]=0;setv(name,answer);continue;
  }
  if(!strncmp(s,"set ",4)){
   char*p=strstr(s+4," to ");if(!p){fprintf(stderr,"E#+ error: invalid set statement: %s\n",s);free(c);return 1;}*p=0;
   char name[64],val[1024];strncpy_s(name,sizeof(name),trim(s+4),_TRUNCATE);strncpy_s(val,sizeof(val),trim(p+4),_TRUNCATE);
   if(!strncmp(val,"random number between ",22)){int a=0,b=0;sscanf_s(val+22,"%d and %d",&a,&b);snprintf(val,sizeof(val),"%d",a+(rand()%(b-a+1)));}
   else {char evaluated[1024];eval(val,evaluated,sizeof(evaluated));strncpy_s(val,sizeof(val),evaluated,_TRUNCATE);if(val[0]=='"'&&val[strlen(val)-1]=='"'){size_t z=strlen(val)-2;memmove(val,val+1,z);val[z]=0;}}
   setv(name,val);continue;
  }
  if(!strncmp(s,"repeat ",7)){
   int times=0,end=i+1;sscanf_s(s,"repeat %d times",&times);while(end<n&&strcmp(trim(ls[end]),"end"))end++;
   for(int r=0;r<times;r++)for(int j=i+1;j<end;j++){char*t=trim(ls[j]);if(!strncmp(t,"print words ",12))printline(t);else if(!strncmp(t,"set ",4)){char*p=strstr(t+4," to ");if(p){*p=0;char name[64],val[1024];strncpy_s(name,sizeof(name),trim(t+4),_TRUNCATE);strncpy_s(val,sizeof(val),trim(p+4),_TRUNCATE);char e[1024];eval(val,e,sizeof(e));setv(name,e);}}}
   i=end;continue;
  }
  if(!strncmp(s,"if ",3)){
   char cond[1024];strncpy_s(cond,sizeof(cond),s+3,_TRUNCATE);char*then=strstr(cond," then");if(then)*then=0;
   int yes=condition(trim(cond)),end=i+1,el=-1;while(end<n&&strcmp(trim(ls[end]),"end")){if(!strcmp(trim(ls[end]),"else"))el=end;end++;}
   int a=yes?i+1:(el>=0?el+1:end),b=yes?(el>=0?el:end):end;
   for(int j=a;j<b;j++){char*t=trim(ls[j]);if(!strncmp(t,"print words ",12))printline(t);}
   i=end;continue;
  }
  /* Game-language statements are compiled by main.c; ordinary E#+ keeps running here. */
  if(!strncmp(s,"game ",5)||!strncmp(s,"window ",7)||!strncmp(s,"camera ",7)||!strncmp(s,"player ",7)||!strncmp(s,"arena ",6)||!strncmp(s,"enemy ",6)||!strncmp(s,"weapon ",7)||!strncmp(s,"create ",7)||!strncmp(s,"load model ",11)||!strncmp(s,"spawn model ",12)||!strncmp(s,"while game is running",21)||!strncmp(s,"end",3)||!strncmp(s,"else",4)||!strncmp(s,"if key ",7)||!strncmp(s,"if mouse ",9))continue;
  fprintf(stderr,"E#+ error: unknown statement: %s\n",s);free(c);return 1;
 }
 free(c);return 0;
}
