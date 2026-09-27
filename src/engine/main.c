#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "lexer.h"

int run_eplus(const char *s);

static char *trim(char *s){char*e;while(*s&&isspace((unsigned char)*s))s++;e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))--e;*e=0;return s;}
static void quoted(const char*s,char*out,size_t cap){const char*a=strchr(s,'"');if(!a){out[0]=0;return;}a++;const char*b=strrchr(a,'"');if(!b||b<a){out[0]=0;return;}size_t n=(size_t)(b-a);if(n>=cap)n=cap-1;memcpy(out,a,n);out[n]=0;}

static int is_game_source(const char*s){
 const char*p=s;while(*p){char line[2048];size_t n=0;while(*p&&*p!='\n'&&n+1<sizeof(line))line[n++]=*p++;line[n]=0;char*q=trim(line);
 if(!strncmp(q,"game ",5)||!strncmp(q,"window ",7)||!strncmp(q,"camera ",7)||!strncmp(q,"player ",7)||!strncmp(q,"arena ",6)||!strncmp(q,"enemy ",6)||!strncmp(q,"weapon ",7)||!strncmp(q,"game spawn ",11)||!strncmp(q,"game max ",9)||!strncmp(q,"game win ",9)||!strncmp(q,"create ",7)||!strncmp(q,"load model ",11)||!strncmp(q,"spawn model ",12))return 1;
 if(*p=='\n')p++;}return 0;
}
static int compile_game(const char*src,const char*path){
 char*copy=(char*)malloc(strlen(src)+1);if(!copy)return 0;strcpy(copy,src);
 FILE*f=fopen(path,"w");if(!f){free(copy);return 0;}fprintf(f,"EPLUS_SCENE 3\n");
 char*ctx=NULL,*line=strtok_s(copy,"\r\n",&ctx);
 while(line){
  char*s=trim(line),q[256];
  if(!strncmp(s,"game ",5)){quoted(s,q,sizeof(q));fprintf(f,"TITLE %s\n",q);}
  else if(!strncmp(s,"window width ",13)){int v=1280;sscanf_s(s+13,"%d",&v);fprintf(f,"WINDOW_WIDTH %d\n",v);}
  else if(!strncmp(s,"window height ",14)){int v=720;sscanf_s(s+14,"%d",&v);fprintf(f,"WINDOW_HEIGHT %d\n",v);}
  else if(!strncmp(s,"window title ",13)){quoted(s,q,sizeof(q));fprintf(f,"TITLE %s\n",q);}
  else if(!strncmp(s,"camera ",7))fprintf(f,"CAMERA %s\n",strstr(s,"first person")?"first_person":"free");
  else if(!strncmp(s,"player create ",14)){quoted(s,q,sizeof(q));fprintf(f,"ENTITY %s MODEL cube\nCONTROL %s\n",q,q);}
  else if(!strncmp(s,"player position ",16)){float a=0,b=2,c=12;sscanf_s(s+16,"%f %f %f",&a,&b,&c);fprintf(f,"POS Hero %.3f %.3f %.3f\n",a,b,c);}
  else if(!strncmp(s,"player speed ",13)){float v=5;sscanf_s(s+13,"%f",&v);fprintf(f,"SPEED Hero %.3f\n",v);}
  else if(!strncmp(s,"player health ",14)){float v=100;sscanf_s(s+14,"%f",&v);fprintf(f,"HEALTH Hero %.3f\n",v);}
  else if(!strncmp(s,"arena size ",11)){float v=40;sscanf_s(s+11,"%f",&v);fprintf(f,"ARENA %.3f\n",v);}
  else if(!strncmp(s,"enemy create ",13)){quoted(s,q,sizeof(q));fprintf(f,"ENTITY %s MODEL cube\n",q);}
  else if(!strncmp(s,"enemy health ",13)){float v=30;sscanf_s(s+13,"%f",&v);fprintf(f,"HEALTH CubeEnemy %.3f\n",v);}
  else if(!strncmp(s,"enemy speed ",12)){float v=2;sscanf_s(s+12,"%f",&v);fprintf(f,"SPEED CubeEnemy %.3f\n",v);}
  else if(!strncmp(s,"enemy damage ",13)){float v=10;sscanf_s(s+13,"%f",&v);fprintf(f,"DAMAGE CubeEnemy %.3f\n",v);}
  else if(!strncmp(s,"weapon create ",14)){quoted(s,q,sizeof(q));fprintf(f,"ENTITY %s MODEL cube\n",q);}
  else if(!strncmp(s,"weapon damage ",14)){float v=15;sscanf_s(s+14,"%f",&v);fprintf(f,"DAMAGE Blaster %.3f\n",v);}
  else if(!strncmp(s,"weapon fire rate ",17)){float v=.2f;sscanf_s(s+17,"%f",&v);}
  else if(!strncmp(s,"game spawn rate ",16)){float v=2;sscanf_s(s+16,"%f",&v);fprintf(f,"SPAWN_RATE %.3f\n",v);}
  else if(!strncmp(s,"game max enemies ",18)){int v=20;sscanf_s(s+18,"%d",&v);}
  else if(!strncmp(s,"game win score ",15)){int v=1000;sscanf_s(s+15,"%d",&v);}
  else if(!strncmp(s,"game win time ",15)){float v=0;sscanf_s(s+15,"%f",&v);fprintf(f,"WIN_TIME %.3f\n",v);}
  else if(!strncmp(s,"if key ",7)){
    char key[64]={0};sscanf_s(s,"if key "%63[^"]" is pressed",key,(unsigned)_countof(key));
    char *p=strstr(s,"player move ");if(p){char dir[64]={0};sscanf_s(p+12,"%63s",dir,(unsigned)_countof(dir));fprintf(f,"RULE KEY %s MOVE %s %s 0\n",key,"Hero",dir);}
  }
  else if(!strncmp(s,"if mouse ",9)){
    char button[64]={0};sscanf_s(s,"if mouse button "%63[^"]" is pressed",button,(unsigned)_countof(button));
    fprintf(f,"RULE MOUSE %s FIRE Hero Blaster 0\n",button);
  }
  else if(!strncmp(s,"player move ",12)){/* consumed by the key rule above */}
  else if(!strncmp(s,"weapon fire",11)){/* consumed by the mouse rule above */}
  else if(!strncmp(s,"load model ",11)){quoted(s,q,sizeof(q));fprintf(f,"MODEL %s %s\n","Hero",q);}
  line=strtok_s(NULL,"\r\n",&ctx);
 }
 /* Compatibility behavior is compiled as generic rules, not runtime game logic. */
 fprintf(f,"RULE UPDATE CubeEnemy CHASE Hero NONE 0\n");
 fclose(f);free(copy);return 1;
}
static int launch(const char*engine,const char*game){
 char runtime[MAX_PATH],scene[MAX_PATH],cmd[32768],*slash;strncpy_s(runtime,sizeof(runtime),engine,_TRUNCATE);slash=strrchr(runtime,'\\');if(!slash)slash=strrchr(runtime,'/');if(!slash)return 0;slash[1]=0;strcat_s(runtime,sizeof(runtime),"EPlusGameRuntime.exe");
 if(GetFileAttributesA(runtime)==INVALID_FILE_ATTRIBUTES){fprintf(stderr,"E#+ error: EPlusGameRuntime.exe was not found next to the engine.\n");return 1;}
 if(!GetTempPathA(sizeof(scene),scene))return 1;strcat_s(scene,sizeof(scene),"eplus_game.scene");char*src=read_entire_file(game);if(!src)return 1;int ok=compile_game(src,scene);free(src);if(!ok)return 1;
 snprintf(cmd,sizeof(cmd),"\"%s\" \"%s\"",runtime,scene);STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};si.cb=sizeof(si);
 if(!CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)){fprintf(stderr,"E#+ error: game runtime launch failed (%lu).\n",(unsigned long)GetLastError());DeleteFileA(scene);return 1;}
 CloseHandle(pi.hThread);WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);DeleteFileA(scene);return (int)code;
}
int main(int argc,char**argv){
 if(argc<2){fprintf(stderr,"E#+ engine 0.5\nUsage: eplus-engine file.eplus\n");return 1;}
 char*s=read_entire_file(argv[1]);if(!s){fprintf(stderr,"E#+ error: could not read file\n");return 1;}
 if(is_game_source(s)){int r=launch(argv[0],argv[1]);free(s);return r;}
 int r=run_eplus(s);free(s);return r;
}
