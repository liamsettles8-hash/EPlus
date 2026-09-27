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

typedef struct {
    char name[128], model[128];
    Vector3 pos;
    float scale, health, maxHealth, speed, damage, cooldown;
    int alive, controllable, solid;
} Entity;

typedef struct {
    char event[64], action[128], a[128], b[128];
    float value;
} Rule;

typedef struct {
    int width, height;
    float winTime;
    char title[256];
    char shader[64];
    Entity entities[MAX_ENTITIES];
    int entityCount;
    Rule rules[MAX_RULES];
    int ruleCount;
} Scene;

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
    e->scale=1; e->health=100; e->maxHealth=100; e->speed=5; e->alive=1; e->solid=1;
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
    memset(s,0,sizeof(*s)); s->width=1280;s->height=720;
    strcpy_s(s->title,sizeof(s->title),"E#+ Game");
    strcpy_s(s->shader,sizeof(s->shader),"none");
    while(fgets(line,sizeof(line),f)) {
        char a[256]={0},b[256]={0},c[256]={0}; float x,y,z;
        if(sscanf_s(line,"WINDOW_WIDTH %d",&s->width)==1) continue;
        if(sscanf_s(line,"WINDOW_HEIGHT %d",&s->height)==1) continue;
        if(sscanf_s(line,"TITLE %255[^\r\n]",s->title,(unsigned)_countof(s->title))==1) continue;
        if(sscanf_s(line,"SHADER %63s",s->shader,(unsigned)_countof(s->shader))==1) continue;
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
        if(sscanf_s(line,"CONTROL %127s",a,(unsigned)_countof(a))==1){int i=find_entity(s,a);if(i>=0)s->entities[i].controllable=1;continue;}
        if(!strncmp(line,"RULE ",5)) {
            char event[64],who[128],action[128],target[128]; float value=0;
            int n=sscanf_s(line,"RULE %63s %127s %127s %127s %f",event,(unsigned)_countof(event),who,(unsigned)_countof(who),action,(unsigned)_countof(action),target,(unsigned)_countof(target),&value);
            if(n>=4)add_rule(s,event,who,action,target,n==5?value:0);
        }
    }
    fclose(f);
    return 1;
}
static Vector3 forward_from(float yaw,float pitch){
    return (Vector3){cosf(yaw*DEG2RAD)*cosf(pitch*DEG2RAD),sinf(pitch*DEG2RAD),sinf(yaw*DEG2RAD)*cosf(pitch*DEG2RAD)};
}
static int key_code(const char *key) {
    if(strlen(key)==1) {
        char c=(char)toupper((unsigned char)key[0]);
        if(c>='A'&&c<='Z') return KEY_A+(c-'A');
    }
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
static void render_entity(const Entity *e) {
    if(!e->alive)return;
    float q=e->scale;
    if(!strcmp(e->model,"cube")) {
        DrawCube(e->pos,q,q,q,(Color){210,70,75,255});
        DrawCubeWires(e->pos,q,q,q,BLACK);
    } else if(!strcmp(e->model,"sphere")) {
        DrawSphere(e->pos,q*.5f,(Color){80,150,230,255});
    } else {
        DrawCube(e->pos,q,q,q,(Color){150,150,160,255});
    }
}
int main(int argc,char **argv) {
    Scene s;
    if(argc<2 || !load_scene(argv[1],&s)){fprintf(stderr,"E#+ Game Runtime: invalid scene.\n");return 1;}
    InitWindow(s.width,s.height,s.title);\n    if(!IsWindowReady()){fprintf(stderr,"E#+ Game Runtime: raylib could not create the window.\\n");return 1;}\n    SetTargetFPS(120);DisableCursor();
    /* Resolve bundled shaders relative to the runtime executable without
       depending on windows.h (which conflicts with raylib's Win32 names). */
    char exeDir[1024]={0}, shaderVs[1024]={0}, shaderFs[1024]={0};
    strncpy_s(exeDir,sizeof(exeDir),argv[0],_TRUNCATE);
    char *slash=strrchr(exeDir,'\\\\'); if(!slash) slash=strrchr(exeDir,'/');
    if(slash) slash[1]=0; else exeDir[0]=0;
    snprintf(shaderVs,sizeof(shaderVs),"%seplus_realistic.vs",exeDir);
    snprintf(shaderFs,sizeof(shaderFs),"%seplus_realistic.fs",exeDir);
    Shader realistic={0};
    if(!strcmp(s.shader,"realistic")) realistic=LoadShader(FileExists(shaderVs)?shaderVs:NULL,FileExists(shaderFs)?shaderFs:NULL);
    int locTime=GetShaderLocation(realistic,"uTime");
    int locCamera=GetShaderLocation(realistic,"uCameraPos");
    int locSunDir=GetShaderLocation(realistic,"uSunDir");
    float shaderTime=0;
    int player=-1;
    for(int i=0;i<s.entityCount;i++)if(s.entities[i].controllable){player=i;break;}
    Camera3D cam={0};
    if(player>=0)cam.position=s.entities[player].pos;
    cam.up=(Vector3){0,1,0};cam.fovy=70;cam.projection=CAMERA_PERSPECTIVE;
    float yaw=-90,pitch=0,elapsed=0;
    while(!WindowShouldClose()){
        float dt=GetFrameTime();elapsed+=dt;shaderTime+=dt;
        if(player>=0 && s.entities[player].alive){
            Entity *p=&s.entities[player];
            Vector2 md=GetMouseDelta();yaw+=md.x*.10f;pitch-=md.y*.10f;
            if(pitch>89)pitch=89;if(pitch<-89)pitch=-89;
            Vector3 fwd=forward_from(yaw,pitch);
            Vector3 flat=Vector3Normalize((Vector3){fwd.x,0,fwd.z});
            Vector3 right=Vector3Normalize(Vector3CrossProduct(flat,(Vector3){0,1,0}));
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
            cam.position=p->pos;cam.target=Vector3Add(cam.position,fwd);
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
        SetShaderValue(realistic,locTime,&shaderTime,SHADER_UNIFORM_FLOAT);
        SetShaderValue(realistic,locCamera,camPos,SHADER_UNIFORM_VEC3);
        SetShaderValue(realistic,locSunDir,sunDir,SHADER_UNIFORM_VEC3);
        BeginDrawing();ClearBackground((Color){18,22,30,255});
        BeginMode3D(cam);
        if(realistic.id>0)BeginShaderMode(realistic);
        for(int i=0;i<s.entityCount;i++)render_entity(&s.entities[i]);
        if(realistic.id>0)EndShaderMode();
        EndMode3D();
        EndDrawing();
    }
    if(realistic.id>0)UnloadShader(realistic); EnableCursor();CloseWindow();return 0;
}
