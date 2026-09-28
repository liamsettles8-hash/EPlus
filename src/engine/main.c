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
 if(!strncmp(q,"game ",5)||!strncmp(q,"window ",7)||!strncmp(q,"camera ",7)||!strncmp(q,"player ",7)||!strncmp(q,"arena ",6)||!strncmp(q,"enemy ",6)||!strncmp(q,"weapon ",7)||!strncmp(q,"game spawn ",11)||!strncmp(q,"game max ",9)||!strncmp(q,"game win ",9)||!strncmp(q,"create ",7)||!strncmp(q,"load model ",11)||!strncmp(q,"spawn model ",12)||!strncmp(q,"import ",7)||!strncmp(q,"object ",7)||!strncmp(q,"when object ",12)||!strncmp(q,"every second",12)||!strncmp(q,"2d ",3)||!strncmp(q,"if button ",10))return 1;
 if(*p=='\n')p++;}return 0;
}
static int compile_game(const char*src,const char*path){
 char*copy=(char*)malloc(strlen(src)+1);if(!copy)return 0;strcpy(copy,src);
 FILE*f=fopen(path,"w");if(!f){free(copy);return 0;}fprintf(f,"EPLUS_SCENE 3\n");
 char player[128]="Player",enemy[128]="Enemy",weapon[128]="Weapon";char modelOwner[128]="Player";
 char*ctx=NULL,*line=strtok_s(copy,"\r\n",&ctx);
 while(line){
  char*s=trim(line),q[256];
  if(!strncmp(s,"game ",5)){quoted(s,q,sizeof(q));fprintf(f,"TITLE %s\n",q);}
  else if(!strncmp(s,"window width ",13)){int v=1280;sscanf_s(s+13,"%d",&v);fprintf(f,"WINDOW_WIDTH %d\n",v);}
  else if(!strncmp(s,"window height ",14)){int v=720;sscanf_s(s+14,"%d",&v);fprintf(f,"WINDOW_HEIGHT %d\n",v);}
  else if(!strncmp(s,"window title ",13)){quoted(s,q,sizeof(q));fprintf(f,"TITLE %s\n",q);}
  else if(!strncmp(s,"import ",7)){
   char file[512]={0},alias[128]={0};
   if(sscanf_s(s+7," "%c",file,(unsigned)_countof(file))){}
   const char *p=strchr(s+7,'"');
   if(p){p++;const char *e=strchr(p,'"');if(e){size_t n=(size_t)(e-p);if(n>=sizeof(file))n=sizeof(file)-1;memcpy(file,p,n);file[n]=0;p=e+1;}}
   if(file[0]){const char *a=strstr(p?p:s," as ");if(a){a+=4;while(*a==' ')a++;if(*a=='"'){a++;const char *e=strchr(a,'"');if(e){size_t n=(size_t)(e-a);if(n>=sizeof(alias))n=sizeof(alias)-1;memcpy(alias,a,n);alias[n]=0;}}}}
   if(file[0]&&alias[0])fprintf(f,"IMPORT %s "%s"\n",alias,file);
  }
  else if(!strncmp(s,"object create ",14)){
   char n[128]={0},type[64]={0};
   if(sscanf_s(s+14," "%127[^"]" "%63[^"]"",n,(unsigned)_countof(n),type,(unsigned)_countof(type))==2)fprintf(f,"ENTITY %s MODEL %s\n",n,type);
  }
  else if(!strncmp(s,"object position ",16)){char n[128]={0};float a=0,b=0,d=0;if(sscanf_s(s+16," "%127[^"]" %f %f %f",n,(unsigned)_countof(n),&a,&b,&d)==4)fprintf(f,"POS %s %.3f %.3f %.3f\n",n,a,b,d);}
  else if(!strncmp(s,"object scale ",13)){char n[128]={0};float v=1;if(sscanf_s(s+13," "%127[^"]" %f",n,(unsigned)_countof(n),&v)==2)fprintf(f,"SCALE %s %.3f\n",n,v);}
  else if(!strncmp(s,"object rotation ",16)){char n[128]={0};float x=0,y=0,z=0;if(sscanf_s(s+16," "%127[^"]" %f %f %f",n,(unsigned)_countof(n),&x,&y,&z)==4)fprintf(f,"ROT %s %.3f %.3f %.3f\n",n,x,y,z);}
  else if(!strncmp(s,"object color ",13)){char n[128]={0};int r=255,g=255,b=255;if(sscanf_s(s+13," "%127[^"]" %d %d %d",n,(unsigned)_countof(n),&r,&g,&b)==4)fprintf(f,"COLOR %s %d %d %d\n",n,r,g,b);}
  else if(!strncmp(s,"object texture ",15)){char n[128]={0},a[128]={0};if(sscanf_s(s+15," "%127[^"]" "%127[^"]"",n,(unsigned)_countof(n),a,(unsigned)_countof(a))==2)fprintf(f,"TEXTURE %s %s\n",n,a);}
  else if(!strncmp(s,"object clickable ",17)){char n[128]={0};if(sscanf_s(s+17," "%127[^"]"",n,(unsigned)_countof(n))==1)fprintf(f,"CLICKABLE %s\n",n);}
  else if(!strncmp(s,"when object ",12)){char n[128]={0};if(sscanf_s(s+12," "%127[^"]"",n,(unsigned)_countof(n))==1)fprintf(f,"SCRIPT_OBJECT %s\n",n);}
  else if(!strncmp(s,"every second",12)){fprintf(f,"TIMER_START\n");}
  else if(!strncmp(s,"2d canvas",9)){fprintf(f,"CANVAS 1\n");}
  else if(!strncmp(s,"2d background ",14)){quoted(s,q,sizeof(q));fprintf(f,"BACKGROUND %s\n",q);}
  else if(!strncmp(s,"2d text ",8)){char n[128]={0},t[256]={0};if(sscanf_s(s+8," \"%127[^\"]\" \"%255[^\"]\"",n,(unsigned)_countof(n),t,(unsigned)_countof(t))==2)fprintf(f,"TEXT %s \"%s\"\n",n,t);else{sscanf_s(s+8,"%127s",n,(unsigned)_countof(n));quoted(s,t,sizeof(t));fprintf(f,"TEXT %s \"%s\"\n",n,t);}}
  else if(!strncmp(s,"2d button ",10)){char n[128]={0},t[256]={0};if(sscanf_s(s+10," \"%127[^\"]\" \"%255[^\"]\"",n,(unsigned)_countof(n),t,(unsigned)_countof(t))==2)fprintf(f,"BUTTON %s \"%s\"\n",n,t);else{sscanf_s(s+10,"%127s",n,(unsigned)_countof(n));quoted(s,t,sizeof(t));fprintf(f,"BUTTON %s \"%s\"\n",n,t);}}
  else if(!strncmp(s,"2d position ",12)){char n[128]={0};float x=0,y=0;if(sscanf_s(s+12," \"%127[^\"]\" %f %f",n,(unsigned)_countof(n),&x,&y)==3)fprintf(f,"UI_POS %s %.1f %.1f\n",n,x,y);else{sscanf_s(s+12,"%127s %f %f",n,(unsigned)_countof(n),&x,&y);fprintf(f,"UI_POS %s %.1f %.1f\n",n,x,y);}}
  else if(!strncmp(s,"2d size ",8)){char n[128]={0};float x=120,y=40;if(sscanf_s(s+8," \"%127[^\"]\" %f %f",n,(unsigned)_countof(n),&x,&y)!=3)sscanf_s(s+8,"%127s %f %f",n,(unsigned)_countof(n),&x,&y);fprintf(f,"UI_SIZE %s %.1f %.1f\n",n,x,y);}
  else if(!strncmp(s,"if button ",10)){char n[128]={0};const char*p=strstr(s,"button ");p+=7;while(*p==' ')p++;if(*p=='"'){p++;const char*e=strchr(p,'"');if(e){size_t z=(size_t)(e-p);if(z>=sizeof(n))z=sizeof(n)-1;memcpy(n,p,z);n[z]=0;}}if(n[0])fprintf(f,"SCRIPT_BUTTON %s\n",n);}
  else if(!strncmp(s,"when button ",12)){char n[128]={0};const char*p=s+12;while(*p==' ')p++;if(*p=='"'){p++;const char*e=strchr(p,'"');if(e){size_t z=(size_t)(e-p);if(z>=sizeof(n))z=sizeof(n)-1;memcpy(n,p,z);n[z]=0;}}else{sscanf_s(p,"%127s",n,(unsigned)_countof(n));}if(n[0])fprintf(f,"SCRIPT_BUTTON %s\n",n);}
  else if(!strncmp(s,"add number ",11)){char operand[128]={0},var[128]={0};if(sscanf_s(s+11,"%127s to %127s",operand,(unsigned)_countof(operand),var,(unsigned)_countof(var))==2){char*e=NULL;float v=strtof(operand,&e);if(e&&*e==0)fprintf(f,"SCRIPT ADD %s %.3f\n",var,v);else fprintf(f,"SCRIPT ADDVAR %s %s\n",var,operand);}}
  else if(!strncmp(s,"set number ",11)){char var[128]={0};float v=0;sscanf_s(s+11,"%127s to %f",var,(unsigned)_countof(var),&v);fprintf(f,"SCRIPT SET %s %.3f\n",var,v);}
  else if(!strncmp(s,"change text ",12)){
   char ui[128]={0},t[256]={0}; const char*p=s+12; while(*p==' ')p++;
   if(*p=='"'){p++;const char*e=strchr(p,'"');if(e){size_t n=(size_t)(e-p);if(n>=sizeof(ui))n=sizeof(ui)-1;memcpy(ui,p,n);ui[n]=0;p=e+1;}}
   while(*p==' ')p++;
   if(*p=='"'){p++;const char*e=strrchr(p,'"');if(e){size_t n=(size_t)(e-p);if(n>=sizeof(t))n=sizeof(t)-1;memcpy(t,p,n);t[n]=0;}}
   if(ui[0])fprintf(f,"SCRIPT TEXT %s \"%s\"\n",ui,t);
  }
  else if(!strncmp(s,"subtract number ",15)){char var[128]={0};float v=0;sscanf_s(s+15,"%f from %127s",&v,var,(unsigned)_countof(var));fprintf(f,"SCRIPT SUB %s %.3f\n",var,v);}
  else if(!strncmp(s,"multiply number ",16)){char var[128]={0};float v=1;sscanf_s(s+16,"%f to %127s",&v,var,(unsigned)_countof(var));fprintf(f,"SCRIPT MUL %s %.3f\n",var,v);}
  else if(!strncmp(s,"divide number ",14)){char var[128]={0};float v=1;sscanf_s(s+14,"%f from %127s",&v,var,(unsigned)_countof(var));fprintf(f,"SCRIPT DIV %s %.3f\n",var,v);}
  else if(!strncmp(s,"if ",3) && strstr(s," then")){char left[128]={0},right[128]={0};if(sscanf_s(s+3,"%127s is greater than %127s then",left,(unsigned)_countof(left),right,(unsigned)_countof(right))==2)fprintf(f,"SCRIPT IFGT %s %s\n",left,right);else if(sscanf_s(s+3,"%127s is less than %127s then",left,(unsigned)_countof(left),right,(unsigned)_countof(right))==2)fprintf(f,"SCRIPT IFLT %s %s\n",left,right);else if(sscanf_s(s+3,"%127s is equal to %127s then",left,(unsigned)_countof(left),right,(unsigned)_countof(right))==2)fprintf(f,"SCRIPT IFEQ %s %s\n",left,right);else if(sscanf_s(s+3,"%127s is not equal to %127s then",left,(unsigned)_countof(left),right,(unsigned)_countof(right))==2)fprintf(f,"SCRIPT IFNE %s %s\n",left,right);}
  else if(!strcmp(s,"else"))fprintf(f,"SCRIPT ELSE\n");
  else if(!strcmp(s,"end"))fprintf(f,"SCRIPT END\n");
  else if(!strncmp(s,"set ",4)){char var[128]={0},val[128]={0};if(sscanf_s(s+4,"%127s to %127s",var,(unsigned)_countof(var),val,(unsigned)_countof(val))==2){char*e=NULL;float v=strtof(val,&e);if(e&&*e==0)fprintf(f,"SCRIPT SET %s %.3f\n",var,v);}}
  else if(!strncmp(s,"shader ",7)){char name[64]={0};sscanf_s(s+7,"%63s",name,(unsigned)_countof(name));fprintf(f,"SHADER %s\n",name);}
  else if(!strncmp(s,"camera ",7))fprintf(f,"CAMERA %s\n",strstr(s,"first person")?"first_person":"free");
  else if(!strncmp(s,"player create ",14)){quoted(s,q,sizeof(q));strncpy_s(player,sizeof(player),q,_TRUNCATE);fprintf(f,"ENTITY %s MODEL cube\nCONTROL %s\n",player,player);strncpy_s(modelOwner,sizeof(modelOwner),player,_TRUNCATE);}
  else if(!strncmp(s,"player position ",16)){float a=0,b=2,c=12;sscanf_s(s+16,"%f %f %f",&a,&b,&c);fprintf(f,"POS %s %.3f %.3f %.3f\n",player,a,b,c);}
  else if(!strncmp(s,"player speed ",13)){float v=5;sscanf_s(s+13,"%f",&v);fprintf(f,"SPEED %s %.3f\n",player,v);}
  else if(!strncmp(s,"player health ",14)){float v=100;sscanf_s(s+14,"%f",&v);fprintf(f,"HEALTH %s %.3f\n",player,v);}
  else if(!strncmp(s,"enemy create ",13)){quoted(s,q,sizeof(q));strncpy_s(enemy,sizeof(enemy),q,_TRUNCATE);fprintf(f,"ENTITY %s MODEL cube\n",enemy);}
  else if(!strncmp(s,"enemy health ",13)){float v=30;sscanf_s(s+13,"%f",&v);fprintf(f,"HEALTH %s %.3f\n",enemy,v);}
  else if(!strncmp(s,"enemy speed ",12)){float v=2;sscanf_s(s+12,"%f",&v);fprintf(f,"SPEED %s %.3f\n",enemy,v);}
  else if(!strncmp(s,"enemy damage ",13)){float v=10;sscanf_s(s+13,"%f",&v);fprintf(f,"DAMAGE %s %.3f\n",enemy,v);}
  else if(!strncmp(s,"weapon create ",14)){quoted(s,q,sizeof(q));strncpy_s(weapon,sizeof(weapon),q,_TRUNCATE);fprintf(f,"ENTITY %s MODEL cube\n",weapon);}
  else if(!strncmp(s,"weapon damage ",14)){float v=15;sscanf_s(s+14,"%f",&v);fprintf(f,"DAMAGE %s %.3f\n",weapon,v);}
  else if(!strncmp(s,"weapon fire rate ",17)){float v=.2f;sscanf_s(s+17,"%f",&v);}
  else if(!strncmp(s,"if key ",7)){
   char key[64]={0};sscanf_s(s,"if key \"%63[^\"]\" is pressed",key,(unsigned)_countof(key));
   char*p=strstr(s,"player move ");if(p){char dir[64]={0};sscanf_s(p+12,"%63s",dir,(unsigned)_countof(dir));fprintf(f,"RULE KEY %s %s MOVE %s 0\n",key,player,dir);}
  } else if(!strncmp(s,"if mouse ",9)){
   char button[64]={0};sscanf_s(s,"if mouse button \"%63[^\"]\" is pressed",button,(unsigned)_countof(button));
   fprintf(f,"RULE MOUSE %s %s FIRE %s 0\n",button,player,weapon);
  } else if(!strncmp(s,"load model ",11)){quoted(s,q,sizeof(q));fprintf(f,"MODEL %s %s\n",modelOwner,q);}
  line=strtok_s(NULL,"\r\n",&ctx);
 }
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
