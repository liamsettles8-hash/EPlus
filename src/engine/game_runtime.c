#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_ENTITIES 256
#define MAX_RULES 256
#define MAX_UI 256
#define MAX_SCRIPT_COMMANDS 512
#define MAX_VARIABLES 256

typedef struct { char name[128], type[16], text[256]; float x,y,w,h; } UIElement;

typedef struct {
    char name[128], model[128], texture[128];
    Vector3 pos, rotation;
    float scale, health, maxHealth, speed, damage, cooldown;
    Color color;
    int alive, controllable, solid, clickable;
} Entity;

typedef struct {
    char event[64], action[128], a[128], b[128];
    float value;
} Rule;

typedef struct { char type[32], target[128], ui[128], text[256]; float value; } ScriptCommand;
typedef struct { char name[128]; float value; } NumberVariable;
typedef struct { char name[128], path[512]; Texture2D texture; int loaded; } ImageAsset;
typedef struct {
    int width, height;
    float winTime;
    char title[256];
    char shader[64];
    Entity entities[MAX_ENTITIES];
    int entityCount;
    Rule rules[MAX_RULES];
    int ruleCount;
    int canvas;
    Color background;
    UIElement ui[MAX_UI];
    int uiCount;
    ScriptCommand scripts[MAX_SCRIPT_COMMANDS];
    int scriptCount;
    int scriptStart[MAX_UI];
    int scriptEnd[MAX_UI];
    NumberVariable variables[MAX_VARIABLES];
    int variableCount;
} Scene;

static int find_ui(Scene *s,const char *name){for(int i=0;i<s->uiCount;i++)if(!strcmp(s->ui[i].name,name))return i;return -1;}
static UIElement *add_ui(Scene *s,const char *name,const char *type){if(s->uiCount>=MAX_UI)return NULL;UIElement*u=&s->ui[s->uiCount++];memset(u,0,sizeof(*u));strncpy_s(u->name,sizeof(u->name),name,_TRUNCATE);strncpy_s(u->type,sizeof(u->type),type,_TRUNCATE);u->w=120;u->h=40;return u;}
static int find_variable(Scene*s,const char*n){for(int i=0;i<s->variableCount;i++)if(!strcmp(s->variables[i].name,n))return i;return -1;}
static NumberVariable*get_variable(Scene*s,const char*n){int i=find_variable(s,n);if(i>=0)return &s->variables[i];if(s->variableCount>=MAX_VARIABLES)return NULL;NumberVariable*v=&s->variables[s->variableCount++];memset(v,0,sizeof(*v));strncpy_s(v->name,sizeof(v->name),n,_TRUNCATE);return v;}
static void add_script(Scene*s,const char*type,const char*target,const char*ui,const char*text,float value){if(s->scriptCount>=MAX_SCRIPT_COMMANDS)return;ScriptCommand*c=&s->scripts[s->scriptCount++];memset(c,0,sizeof(*c));strncpy_s(c->type,sizeof(c->type),type,_TRUNCATE);strncpy_s(c->target,sizeof(c->target),target?target:"",_TRUNCATE);strncpy_s(c->ui,sizeof(c->ui),ui?ui:"",_TRUNCATE);strncpy_s(c->text,sizeof(c->text),text?text:"",_TRUNCATE);c->value=value;}
static void set_ui_text_value(Scene*s,const char*ui,const char*fmt){
    int idx=find_ui(s,ui); if(idx<0||!fmt)return;
    UIElement*u=&s->ui[idx]; char out[256]={0}; size_t n=0; const char*p=fmt;
    while(*p&&n+1<sizeof(out)){
        if(*p=='{'){const char*e=strchr(p+1,'}');
            if(e){size_t len=(size_t)(e-(p+1));
                if(len>0&&len<128){char name[128]={0}; memcpy(name,p+1,len); name[len]=0;
                    int vi=find_variable(s,name);
                    if(vi>=0){int w=_snprintf_s(out+n,sizeof(out)-n,_TRUNCATE,"%.0f",s->variables[vi].value);
                        if(w>0)n+=(size_t)w; p=e+1; continue; }
                }
            }
        }
        out[n++]=*p++;
    }
    out[n]=0; strncpy_s(u->text,sizeof(u->text),out,_TRUNCATE);
}
static float script_value(Scene*s,const char*n){int idx=find_variable(s,n);if(idx>=0&&idx<s->variableCount)return s->variables[idx].value;char*e=NULL;float x=strtof(n,&e);return(e&&e!=n)?x:0.0f;}
static int script_condition(Scene*s,const char*type,const char*a,const char*b){float x=script_value(s,a),y=script_value(s,b);if(!strcmp(type,"ifgt"))return x>y;if(!strcmp(type,"iflt"))return x<y;if(!strcmp(type,"ifeq"))return x==y;if(!strcmp(type,"ifne"))return x!=y;return 1;}
static void run_script_range(Scene*s,int start,int end){
    if(start<0||end<start||end>s->scriptCount)return;
    int execute=1,parent[64]={0},depth=0;
    for(int i=start;i<end;i++){
        ScriptCommand*c=&s->scripts[i];
        if(!strcmp(c->type,"ifgt")||!strcmp(c->type,"iflt")||!strcmp(c->type,"ifeq")||!strcmp(c->type,"ifne")){
            if(depth<64){parent[depth]=execute;execute=execute&&script_condition(s,c->type,c->target,c->ui);depth++;}continue;
        }
        if(!strcmp(c->type,"else")){if(depth>0)execute=parent[depth-1]&&!execute;continue;}
        if(!strcmp(c->type,"end")){if(depth>0){execute=parent[depth-1];depth--;}continue;}
        if(!execute)continue;
        if(!strcmp(c->type,"text")){set_ui_text_value(s,c->ui,c->text);continue;}
        NumberVariable*v=get_variable(s,c->target);
        if(!strcmp(c->type,"addvar")&&v)v->value+=script_value(s,c->ui);
        else if(!strcmp(c->type,"add")&&v)v->value+=c->value;
        else if(!strcmp(c->type,"sub")&&v)v->value-=c->value;
        else if(!strcmp(c->type,"mul")&&v)v->value*=c->value;
        else if(!strcmp(c->type,"div")&&v&&c->value!=0)v->value/=c->value;
        else if(!strcmp(c->type,"set")&&v)v->value=c->value;
    }
}
static void run_button_script(Scene*s,int uiIndex){if(uiIndex>=0&&uiIndex<s->uiCount)run_script_range(s,s->scriptStart[uiIndex],s->scriptEnd[uiIndex]);}
static void run_object_script(Scene*s,int entityIndex){if(entityIndex>=0&&entityIndex<s->entityCount)run_script_range(s,s->scriptObjectStart[entityIndex],s->scriptObjectEnd[entityIndex]);}
static Color parse_hex(const char *v){unsigned r=18,g=22,b=30;if(v&&v[0]=='#')sscanf_s(v+1,"%02x%02x%02x",&r,&g,&b);return(Color){(unsigned char)r,(unsigned char)g,(unsigned char)b,255};}
static int find_asset(Scene *s,const char *name){for(int i=0;i<s->assetCount;i++)if(!strcmp(s->assets[i].name,name))return i;return -1;}
static Entity *get_entity(Scene*s,const char*n){int i=find_entity(s,n);return i>=0?&s->entities[i]:NULL;}
static int find_entity(Scene *s, const char *name) {
    for (int i=0;i<s->entityCount;i++) if (!strcmp(s->entities[i].name,name)) return i;
    return -1;
}
static Entity *add_entity(Scene *s, const char *name) {
    if (s->entityCount >= MAX_ENTITIES) return NULL;
    Entity *e=&s->entities[s->entityCount++];
    memset(e,0,sizeof(*e));
    strncpy_s(e->name,sizeof(e->name),name,_TRUNCATE);
    strcpy_s(e->model,sizeof(e->model),"cube");
    e->scale=1; e->health=100; e->maxHealth=100; e->speed=5; e->alive=1; e->solid=1; e->color=(Color){210,70,75,255};
    return e;
}
static void add_rule(Scene *s,const char *event,const char *a,const char *action,const char *b,float value) {
    if(s->ruleCount>=MAX_RULES)return;
    Rule *r=&s->rules[s->ruleCount++];
    memset(r,0,sizeof(*r));
    strncpy_s(r->event,sizeof(r->event),event,_TRUNCATE);
    strncpy_s(r->a,sizeof(r->a),a?a:"",_TRUNCATE);
    strncpy_s(r->action,sizeof(r->action),action?action:"",_TRUNCATE);
    strncpy_s(r->b,sizeof(r->b),b?b:"",_TRUNCATE);
    r->value=value;
}
static int load_scene(const char *path,Scene *s) {
    FILE *f=fopen(path,"r"); char line[2048];
    if(!f)return 0;
    memset(s,0,sizeof(*s)); for(int i=0;i<MAX_UI;i++){s->scriptStart[i]=-1;s->scriptEnd[i]=-1;} for(int i=0;i<MAX_ENTITIES;i++){s->scriptObjectStart[i]=-1;s->scriptObjectEnd[i]=-1;} s->timerStart=-1;s->timerEnd=-1;s->width=1280;s->height=720;
    strcpy_s(s->title,sizeof(s->title),"E#+ Game");
    strcpy_s(s->shader,sizeof(s->shader),"none"); s->background=(Color){18,22,30,255};
    int lastScriptButton=-1,lastScriptObject=-1;
    while(fgets(line,sizeof(line),f)) {
        char a[256]={0},b[256]={0},c[256]={0}; float x,y,z;
        if(sscanf_s(line,"WINDOW_WIDTH %d",&s->width)==1) continue;
        if(sscanf_s(line,"WINDOW_HEIGHT %d",&s->height)==1) continue;
        if(sscanf_s(line,"TITLE %255[^\r\n]",s->title,(unsigned)_countof(s->title))==1) continue;
        if(!strncmp(line,"IMPORT ",7)){
            char alias[128]={0},path[512]={0};
            if(sscanf_s(line+7,"%127s %511[^\r\n]",alias,(unsigned)_countof(alias),path,(unsigned)_countof(path))>=2){
                while(*path==' ')memmove(path,path+1,strlen(path));
                if(path[0]=='"'&&path[strlen(path)-1]=='"'){path[strlen(path)-1]=0;memmove(path,path+1,strlen(path));}
                if(s->assetCount<128){ImageAsset*a=&s->assets[s->assetCount++];memset(a,0,sizeof(*a));strncpy_s(a->name,sizeof(a->name),alias,_TRUNCATE);strncpy_s(a->path,sizeof(a->path),path,_TRUNCATE);}
            } continue;
        }
        if(sscanf_s(line,"SHADER %63s",s->shader,(unsigned)_countof(s->shader))==1) continue;
        if(!strncmp(line,"CANVAS ",7)){s->canvas=1;continue;}
        if(!strncmp(line,"BACKGROUND ",11)){char v[32]={0};if(sscanf_s(line+11,"%31s",v,(unsigned)_countof(v))==1)s->background=parse_hex(v);continue;}
        if(!strncmp(line,"TEXT ",5)){char n[128]={0},t[256]={0};if(sscanf_s(line,"TEXT %127s %255[^\r\n]",n,(unsigned)_countof(n),t,(unsigned)_countof(t))==2){UIElement*u=add_ui(s,n,"text");if(u){while(*t==' ')memmove(t,t+1,strlen(t));if(t[0]=='"'&&t[strlen(t)-1]=='"'){t[strlen(t)-1]=0;memmove(t,t+1,strlen(t));}strncpy_s(u->text,sizeof(u->text),t,_TRUNCATE);}}continue;}
        if(!strncmp(line,"BUTTON ",7)){char n[128]={0},t[256]={0};if(sscanf_s(line,"BUTTON %127s %255[^\r\n]",n,(unsigned)_countof(n),t,(unsigned)_countof(t))==2){UIElement*u=add_ui(s,n,"button");if(u)strncpy_s(u->text,sizeof(u->text),t,_TRUNCATE);}continue;}
        if(!strncmp(line,"UI_POS ",7)){char n[128]={0};if(sscanf_s(line,"UI_POS %127s %f %f",n,(unsigned)_countof(n),&x,&y)==3){int i=find_ui(s,n);if(i>=0){s->ui[i].x=x;s->ui[i].y=y;}}continue;}
        if(!strncmp(line,"UI_SIZE ",8)){char n[128]={0};if(sscanf_s(line,"UI_SIZE %127s %f %f",n,(unsigned)_countof(n),&x,&y)==3){int i=find_ui(s,n);if(i>=0){s->ui[i].w=x;s->ui[i].h=y;}}continue;}
        if(sscanf_s(line,"ENTITY %127s",a,(unsigned)_countof(a))==1) {
            Entity *e=add_entity(s,a); if(!e)continue;
            if(sscanf_s(line,"ENTITY %127s MODEL %127s",a,(unsigned)_countof(a),b,(unsigned)_countof(b))==2)strncpy_s(e->model,sizeof(e->model),b,_TRUNCATE);
            continue;
        }
        if(sscanf_s(line,"POS %127s %f %f %f",a,(unsigned)_countof(a),&x,&y,&z)==4){int i=find_entity(s,a);if(i>=0)s->entities[i].pos=(Vector3){x,y,z};continue;}
        if(sscanf_s(line,"HEALTH %127s %f",a,(unsigned)_countof(a),&x)==2){int i=find_entity(s,a);if(i>=0){s->entities[i].health=x;s->entities[i].maxHealth=x;}continue;}
        if(sscanf_s(line,"SPEED %127s %f",a,(unsigned)_countof(a),&x)==2){int i=find_entity(s,a);if(i>=0)s->entities[i].speed=x;continue;}
        if(sscanf_s(line,"DAMAGE %127s %f",a,(unsigned)_countof(a),&x)==2){int i=find_entity(s,a);if(i>=0)s->entities[i].damage=x;continue;}
        if(sscanf_s(line,"MODEL %127s %127s",a,(unsigned)_countof(a),b,(unsigned)_countof(b))==2){int i=find_entity(s,a);if(i>=0)strncpy_s(s->entities[i].model,sizeof(s->entities[i].model),b,_TRUNCATE);continue;}
        if(sscanf_s(line,"SCALE %127s %f",a,(unsigned)_countof(a),&x)==2){int i=find_entity(s,a);if(i>=0)s->entities[i].scale=x;continue;}
        if(sscanf_s(line,"ROT %127s %f %f %f",a,(unsigned)_countof(a),&x,&y,&z)==4){int i=find_entity(s,a);if(i>=0)s->entities[i].rotation=(Vector3){x,y,z};continue;}
        if(!strncmp(line,"COLOR ",6)){int cr=255,cg=255,cb=255;if(sscanf_s(line+6,"%127s %d %d %d",a,(unsigned)_countof(a),&cr,&cg,&cb)==4){int i=find_entity(s,a);if(i>=0)s->entities[i].color=(Color){(unsigned char)cr,(unsigned char)cg,(unsigned char)cb,255};}continue;}
        if(sscanf_s(line,"TEXTURE %127s %127s",a,(unsigned)_countof(a),b,(unsigned)_countof(b))==2){int i=find_entity(s,a);if(i>=0)strncpy_s(s->entities[i].texture,sizeof(s->entities[i].texture),b,_TRUNCATE);continue;}
        if(sscanf_s(line,"CLICKABLE %127s",a,(unsigned)_countof(a))==1){int i=find_entity(s,a);if(i>=0)s->entities[i].clickable=1;continue;}

        if(sscanf_s(line,"CONTROL %127s",a,(unsigned)_countof(a))==1){int i=find_entity(s,a);if(i>=0)s->entities[i].controllable=1;continue;}
        if(!strncmp(line,"SCRIPT_BUTTON ",14)){
            if(lastScriptObject>=0)s->scriptObjectEnd[lastScriptObject]=s->scriptCount;
            if(lastScriptButton>=0)s->scriptEnd[lastScriptButton]=s->scriptCount;
            char n[128]={0};if(sscanf_s(line+14,"%127s",n,(unsigned)_countof(n))==1){int ui=find_ui(s,n);if(ui>=0){s->scriptStart[ui]=s->scriptCount;lastScriptButton=ui;lastScriptObject=-1;}}continue;}
        if(!strncmp(line,"SCRIPT_OBJECT ",14)){
            if(lastScriptButton>=0)s->scriptEnd[lastScriptButton]=s->scriptCount;
            if(lastScriptObject>=0)s->scriptObjectEnd[lastScriptObject]=s->scriptCount;
            char n[128]={0};if(sscanf_s(line+14,"%127s",n,(unsigned)_countof(n))==1){int ei=find_entity(s,n);if(ei>=0){s->scriptObjectStart[ei]=s->scriptCount;lastScriptObject=ei;lastScriptButton=-1;}}continue;}
        if(!strncmp(line,"TIMER_START",11)){s->timerStart=s->scriptCount;lastScriptButton=-1;lastScriptObject=-1;continue;}
        if(!strncmp(line,"SCRIPT ADD ",11)){char n[128]={0};float v=0;if(sscanf_s(line+11,"%127s %f",n,(unsigned)_countof(n),&v)==2)add_script(s,"add",n,NULL,NULL,v);continue;}
        if(!strncmp(line,"SCRIPT SET ",11)){char n[128]={0};float v=0;if(sscanf_s(line+11,"%127s %f",n,(unsigned)_countof(n),&v)==2){if(lastScriptButton<0){NumberVariable*v0=get_variable(s,n);if(v0)v0->value=v;}else add_script(s,"set",n,NULL,NULL,v);}continue;}
        if(!strncmp(line,"SCRIPT ADDVAR ",14)){char a[128]={0},b[128]={0};if(sscanf_s(line+14,"%127s %127s",a,(unsigned)_countof(a),b,(unsigned)_countof(b))==2)add_script(s,"addvar",a,b,NULL,0);continue;}
        if(!strncmp(line,"SCRIPT SUB ",11)){char a[128]={0};float v=0;if(sscanf_s(line+11,"%127s %f",a,(unsigned)_countof(a),&v)==2)add_script(s,"sub",a,NULL,NULL,v);continue;}
        if(!strncmp(line,"SCRIPT MUL ",11)){char a[128]={0};float v=1;if(sscanf_s(line+11,"%127s %f",a,(unsigned)_countof(a),&v)==2)add_script(s,"mul",a,NULL,NULL,v);continue;}
        if(!strncmp(line,"SCRIPT DIV ",11)){char a[128]={0};float v=1;if(sscanf_s(line+11,"%127s %f",a,(unsigned)_countof(a),&v)==2)add_script(s,"div",a,NULL,NULL,v);continue;}
        if(!strncmp(line,"SCRIPT TEXT ",12)){
            char ui[128]={0},t[256]={0}; const char*p=line+12; while(*p==' ')p++;
            if(*p=='"'){p++; const char*e=strchr(p,'"');
                if(e){size_t n=(size_t)(e-p); if(n>=sizeof(ui))n=sizeof(ui)-1; memcpy(ui,p,n); ui[n]=0;
                    p=e+1; while(*p==' ')p++;
                    if(*p=='"'){p++; e=strrchr(p,'"'); if(e){size_t n2=(size_t)(e-p); if(n2>=sizeof(t))n2=sizeof(t)-1; memcpy(t,p,n2); t[n2]=0; add_script(s,"text",NULL,ui,t,0);}}
                }
            }else{
                sscanf_s(p,"%127s %255[^\r\n]",ui,(unsigned)_countof(ui),t,(unsigned)_countof(t));
                while(*t==' ')memmove(t,t+1,strlen(t));
                if(t[0]=='"'&&t[strlen(t)-1]=='"'){t[strlen(t)-1]=0;memmove(t,t+1,strlen(t));}
                add_script(s,"text",NULL,ui,t,0);
            }
            continue;
        }
        if(!strncmp(line,"SCRIPT IFGT ",12)||!strncmp(line,"SCRIPT IFLT ",12)||!strncmp(line,"SCRIPT IFEQ ",12)||!strncmp(line,"SCRIPT IFNE ",12)){
            char a[128]={0},b[128]={0},type[16]={0};
            if(!strncmp(line,"SCRIPT IFGT ",12))strcpy_s(type,sizeof(type),"ifgt");
            else if(!strncmp(line,"SCRIPT IFLT ",12))strcpy_s(type,sizeof(type),"iflt");
            else if(!strncmp(line,"SCRIPT IFEQ ",12))strcpy_s(type,sizeof(type),"ifeq");
            else strcpy_s(type,sizeof(type),"ifne");
            if(sscanf_s(line+12,"%127s %127s",a,(unsigned)_countof(a),b,(unsigned)_countof(b))==2)add_script(s,type,a,b,NULL,0);
            continue;
        }
        if(!strncmp(line,"SCRIPT ELSE",11)){add_script(s,"else",NULL,NULL,NULL,0);continue;}
        if(!strncmp(line,"SCRIPT END",10)){add_script(s,"end",NULL,NULL,NULL,0);continue;}
        if(!strncmp(line,"RULE ",5)){
            char event[64],who[128],action[128],target[128]; float value=0;
            int n=sscanf_s(line,"RULE %63s %127s %127s %127s %f",event,(unsigned)_countof(event),who,(unsigned)_countof(who),action,(unsigned)_countof(action),target,(unsigned)_countof(target),&value);
            if(n>=4)add_rule(s,event,who,action,target,n==5?value:0);
        }
    }
    if(lastScriptButton>=0)s->scriptEnd[lastScriptButton]=s->scriptCount;
    if(lastScriptObject>=0)s->scriptObjectEnd[lastScriptObject]=s->scriptCount;
    if(s->timerStart>=0)s->timerEnd=s->scriptCount;
    fclose(f);
    return 1;
}
static Vector3 forward_from(float yaw,float pitch){
    return (Vector3){cosf(yaw*DEG2RAD)*cosf(pitch*DEG2RAD),sinf(pitch*DEG2RAD),sinf(yaw*DEG2RAD)*cosf(pitch*DEG2RAD)};
}
static int key_code(const char *key) {
    if(!strcmp(key,"W"))return KEY_W;
    if(!strcmp(key,"A"))return KEY_A;
    if(!strcmp(key,"S"))return KEY_S;
    if(!strcmp(key,"D"))return KEY_D;
    if(!strcmp(key,"SPACE"))return KEY_SPACE;
    if(!strcmp(key,"ENTER"))return KEY_ENTER;
    if(!strcmp(key,"ESC"))return KEY_ESCAPE;
    if(!strcmp(key,"UP"))return KEY_UP;
    if(!strcmp(key,"DOWN"))return KEY_DOWN;
    if(!strcmp(key,"LEFT"))return KEY_LEFT;
    if(!strcmp(key,"RIGHT"))return KEY_RIGHT;
    return KEY_NULL;
}
static int is_pressed(const char *key) {
    int k=key_code(key);
    return k!=KEY_NULL && IsKeyDown(k);
}
static void render_entity(const Entity *e,Scene *s,Camera cam,float time) {
    if(!e->alive)return;
    if(e->texture[0]){
        int ai=find_asset(s,e->texture);
        if(ai>=0&&s->assets[ai].loaded){DrawBillboard(cam,s->assets[ai].texture,e->pos,e->scale,e->color);return;}
    }
    float q=e->scale;
    if(!strcmp(e->model,"cube")) {
        DrawCube(e->pos,q,q,q,e->color);
        DrawCubeWires(e->pos,q*1.01f,q*1.01f,q*1.01f,(Color){40,40,45,255});
    } else if(!strcmp(e->model,"sphere")) {
        DrawSphereEx(e->pos,q,24,32,e->color);
    } else if(!strcmp(e->model,"cylinder")||!strcmp(e->model,"cookie")) {
        DrawCylinder(e->pos,q,q,q*0.38f,48,e->color);
        DrawCylinderWires(e->pos,q*1.01f,q*1.01f,q*0.39f,48,(Color){80,35,15,255});
    } else if(!strcmp(e->model,"torus")) {
        DrawTorus(e->pos,q*0.65f,q*0.22f,32,16,e->color);
    } else {
        DrawCube(e->pos,q,q,q,e->color);
    }
}
static void render_2d(Scene *s) {
    /*
       The 2D layer is a real screen-space canvas. Draw its background first,
       then every UI element on top. This also makes a canvas visible even
       when the source only contains "2d canvas" and no UI elements yet.
    */
    DrawRectangle(0,0,s->width,s->height,s->background);

    Vector2 mp=GetMousePosition();
    for(int i=0;i<s->uiCount;i++) {
        UIElement *u=&s->ui[i];
        if(!strcmp(u->type,"text")) {
            DrawText(u->text,(int)u->x,(int)u->y,24,RAYWHITE);
        } else if(!strcmp(u->type,"button")) {
            Rectangle r={(float)u->x,(float)u->y,u->w,u->h};
            int hovered=CheckCollisionPointRec(mp,r);
            if(hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) run_button_script(s,i);
            DrawRectangleRec(r,hovered?(Color){70,110,190,255}:(Color){50,65,90,255});
            DrawRectangleLinesEx(r,1,RAYWHITE);
            DrawText(u->text,(int)u->x+10,(int)u->y+10,20,RAYWHITE);
        }
    }
}
int main(int argc,char **argv) {
    Scene s;
    if(argc<2 || !load_scene(argv[1],&s)){fprintf(stderr,"E#+ Game Runtime: invalid scene.\n");return 1;}
    InitWindow(s.width,s.height,s.title);
    for(int ai=0;ai<s.assetCount;ai++){
        s.assets[ai].texture=LoadTexture(s.assets[ai].path);
        s.assets[ai].loaded=IsTextureValid(s.assets[ai].texture)?1:0;
        if(!s.assets[ai].loaded)fprintf(stderr,"E#+: could not load image import %s (%s)\n",s.assets[ai].name,s.assets[ai].path);
    }
    if(!IsWindowReady()){fprintf(stderr,"E#+ Game Runtime: raylib could not create the window.\n");return 1;}
    /* Keep the game window visible and centered on the current monitor. */
    int monitor=GetCurrentMonitor();
    int mw=GetMonitorWidth(monitor), mh=GetMonitorHeight(monitor);
    int wx=(mw-s.width)/2, wy=(mh-s.height)/2;
    if(wx<0)wx=0; if(wy<0)wy=0;
    SetWindowPosition(wx,wy);
    SetWindowFocused();
    RestoreWindow();
    SetTargetFPS(120);
    /* 2D elements are an overlay in every scene. A canvas scene is still 2D-only. */
    if(s.canvas) EnableCursor(); else DisableCursor();
    /* Resolve bundled shaders relative to the runtime executable without
       depending on windows.h (which conflicts with raylib's Win32 names). */
    char exeDir[1024]={0}, shaderVs[1024]={0}, shaderFs[1024]={0};
    strncpy_s(exeDir,sizeof(exeDir),argv[0],_TRUNCATE);
    char *slash=strrchr(exeDir,'\\'); if(!slash) slash=strrchr(exeDir,'/');
    if(slash) slash[1]=0; else exeDir[0]=0;
    snprintf(shaderVs,sizeof(shaderVs),"%seplus_realistic.vs",exeDir);
    snprintf(shaderFs,sizeof(shaderFs),"%seplus_realistic.fs",exeDir);
    Shader realistic={0};
    if(!strcmp(s.shader,"realistic")){
        realistic=LoadShader(FileExists(shaderVs)?shaderVs:NULL,FileExists(shaderFs)?shaderFs:NULL);
        if(realistic.id<=0) fprintf(stderr,"E#+ Game Runtime: realistic shader could not be loaded.\n");
    }
    int locTime=-1,locCamera=-1,locSunDir=-1;
    if(realistic.id>0){
        locTime=GetShaderLocation(realistic,"uTime");
        locCamera=GetShaderLocation(realistic,"uCameraPos");
        locSunDir=GetShaderLocation(realistic,"uSunDir");
    }
    float shaderTime=0;
    int player=-1;
    for(int i=0;i<s.entityCount;i++)if(s.entities[i].controllable){player=i;break;}
    Camera3D cam={0};
    if(player>=0)cam.position=s.entities[player].pos;
    cam.up=(Vector3){0,1,0};cam.fovy=70;cam.projection=CAMERA_PERSPECTIVE;
    float yaw=-90,pitch=0,elapsed=0;
    float verticalVelocity=0.0f;
    int grounded=0;
    while(!WindowShouldClose()){
        float dt=GetFrameTime();elapsed+=dt;shaderTime+=dt;
        if(player>=0 && s.entities[player].alive){
            Entity *p=&s.entities[player];
            Vector2 md=GetMouseDelta();yaw+=md.x*.10f;pitch-=md.y*.10f;
            if(pitch>89)pitch=89;if(pitch<-89)pitch=-89;
            Vector3 fwd=forward_from(yaw,pitch);
            Vector3 horizontalForward=Vector3Normalize((Vector3){fwd.x,0,fwd.z});
            Vector3 flat=horizontalForward;
            Vector3 right=Vector3Normalize(Vector3CrossProduct(flat,(Vector3){0,1,0}));
            /* Built-in generic first-person controls for any E#+ controllable entity.
               These are runtime mechanics, not game-specific behavior. */
            if(IsKeyDown(KEY_W)) p->pos=Vector3Add(p->pos,Vector3Scale(flat,p->speed*dt));
            if(IsKeyDown(KEY_S)) p->pos=Vector3Add(p->pos,Vector3Scale(Vector3Negate(flat),p->speed*dt));
            if(IsKeyDown(KEY_A)) p->pos=Vector3Add(p->pos,Vector3Scale(Vector3Negate(right),p->speed*dt));
            if(IsKeyDown(KEY_D)) p->pos=Vector3Add(p->pos,Vector3Scale(right,p->speed*dt));

            grounded = (p->pos.y <= 0.001f);
            if(grounded) {
                p->pos.y = 0.0f;
                verticalVelocity = 0.0f;
            }
            if(IsKeyPressed(KEY_SPACE) && grounded) {
                verticalVelocity = 5.5f;
                grounded = 0;
            }
            verticalVelocity -= 18.0f * dt;
            p->pos.y += verticalVelocity * dt;
            if(p->pos.y < 0.0f) {
                p->pos.y = 0.0f;
                verticalVelocity = 0.0f;
                grounded = 1;
            }

            if(s.timerStart>=0&&s.timerEnd>s.timerStart){
            static float timerAccumulator=0; timerAccumulator+=dt;
            if(timerAccumulator>=1.0f){timerAccumulator-=1.0f;run_script_range(&s,s.timerStart,s.timerEnd);}
        }
        if(player>=0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            Vector2 clickPos=(Vector2){s.width*0.5f,s.height*0.5f};
            Ray ray=GetScreenToWorldRay(clickPos,cam);
            float best=1e30f;int hit=-1;
            for(int i=0;i<s.entityCount;i++)if(s.entities[i].alive&&s.entities[i].clickable){
                RayCollision rc={0};
                if(!strcmp(s.entities[i].model,"cube")){float q=s.entities[i].scale;BoundingBox box={{s.entities[i].pos.x-q/2,s.entities[i].pos.y-q/2,s.entities[i].pos.z-q/2},{s.entities[i].pos.x+q/2,s.entities[i].pos.y+q/2,s.entities[i].pos.z+q/2}};rc=GetRayCollisionBox(ray,box);}
                else rc=GetRayCollisionSphere(ray,s.entities[i].pos,s.entities[i].scale);
                if(rc.hit&&rc.distance<best){best=rc.distance;hit=i;}
            }
            if(hit>=0)run_object_script(&s,hit);
        }
        for(int r=0;r<s.ruleCount;r++){
                Rule *rule=&s.rules[r];
                if(strcmp(rule->event,"KEY"))continue;
                if(strcmp(rule->a,"W")&&strcmp(rule->a,"S")&&strcmp(rule->a,"A")&&strcmp(rule->a,"D")&&strcmp(rule->a,"SPACE"))continue;
                if(!is_pressed(rule->a))continue;
                if(strcmp(rule->action,"MOVE")==0){
                    Vector3 dir=flat;
                    if(!strcmp(rule->b,"backward"))dir=Vector3Negate(flat);
                    else if(!strcmp(rule->b,"left"))dir=Vector3Negate(right);
                    else if(!strcmp(rule->b,"right"))dir=right;
                    p->pos=Vector3Add(p->pos,Vector3Scale(dir,p->speed*dt));
                }
            }
            cam.position=Vector3Add(p->pos,(Vector3){0,1.6f,0});
            cam.target=Vector3Add(cam.position,Vector3Scale(fwd,10.0f));
            for(int r=0;r<s.ruleCount;r++){
                Rule *rule=&s.rules[r];
                if(strcmp(rule->event,"MOUSE"))continue;
                if(strcmp(rule->a,"left"))continue;
                if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))continue;
                if(strcmp(rule->action,"FIRE"))continue;
                int weapon=find_entity(&s,rule->b); if(weapon<0)weapon=player;
                float best=999999;int hit=-1;
                for(int i=0;i<s.entityCount;i++)if(i!=player&&s.entities[i].alive){
                    Vector3 to=Vector3Subtract(s.entities[i].pos,cam.position);float d=Vector3Length(to);if(d>100)continue;
                    if(Vector3DotProduct(Vector3Normalize(to),fwd)>.985f&&d<best){best=d;hit=i;}
                }
                if(hit>=0){s.entities[hit].health-=s.entities[weapon].damage;if(s.entities[hit].health<=0)s.entities[hit].alive=0;}
            }
        }
        for(int r=0;r<s.ruleCount;r++){
            Rule *rule=&s.rules[r];
            if(strcmp(rule->event,"UPDATE"))continue;
            int who=find_entity(&s,rule->a);if(who<0||!s.entities[who].alive)continue;
            if(!strcmp(rule->action,"CHASE")){
                int target=find_entity(&s,rule->b);if(target<0||!s.entities[target].alive)continue;
                Vector3 d=Vector3Subtract(s.entities[target].pos,s.entities[who].pos);float len=Vector3Length(d);
                if(len>1.3f)s.entities[who].pos=Vector3Add(s.entities[who].pos,Vector3Scale(Vector3Normalize(d),s.entities[who].speed*dt));
            }
        }
        float sunDir[3]={-0.45f,-0.85f,-0.25f};
        float camPos[3]={cam.position.x,cam.position.y,cam.position.z};
        if(realistic.id>0){
            if(locTime>=0)SetShaderValue(realistic,locTime,&shaderTime,SHADER_UNIFORM_FLOAT);
            if(locCamera>=0)SetShaderValue(realistic,locCamera,camPos,SHADER_UNIFORM_VEC3);
            if(locSunDir>=0)SetShaderValue(realistic,locSunDir,sunDir,SHADER_UNIFORM_VEC3);
        }
        BeginDrawing();
        ClearBackground(s.background);
        if(!s.canvas){
            BeginMode3D(cam);
            DrawPlane((Vector3){0,-0.51f,0},(Vector2){100,100},(Color){38,43,52,255}); DrawGrid(40,1.0f);
            if(realistic.id>0)BeginShaderMode(realistic);
            for(int i=0;i<s.entityCount;i++)render_entity(&s.entities[i],&s,cam,elapsed);
            if(realistic.id>0)EndShaderMode();
            EndMode3D();
        }
        /* Always draw the 2D layer last so HUD/UI stays above the 3D world. */
        if(s.canvas || s.uiCount>0) render_2d(&s);
        EndDrawing();
    }
    if(realistic.id>0)UnloadShader(realistic);
    EnableCursor();
    CloseWindow();
    return 0;
}
