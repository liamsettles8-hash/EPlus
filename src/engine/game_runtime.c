#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_OBJECTS 10000

typedef struct {
    Vector3 position;
    Vector3 size;
    Color color;
    int active;
} Cube;

static char *read_file(const char *p){
    FILE*f=fopen(p,"rb"); long n; char*b;
    if(!f)return NULL; fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
    b=(char*)malloc((size_t)n+1); if(!b){fclose(f);return NULL;}
    fread(b,1,(size_t)n,f); b[n]=0; fclose(f); return b;
}
static void trim(char*s){
    char*a=s,*e; while(*a&&isspace((unsigned char)*a))a++;
    if(a!=s)memmove(s,a,strlen(a)+1);
    e=s+strlen(s); while(e>s&&isspace((unsigned char)e[-1]))--e; *e=0;
}
static void quoted(const char*s,char*out,size_t cap){
    const char*a=strchr(s,'"'); if(!a){out[0]=0;return;} a++;
    const char*b=strchr(a,'"'); if(!b){out[0]=0;return;}
    size_t n=(size_t)(b-a); if(n>=cap)n=cap-1; memcpy(out,a,n); out[n]=0;
}
static int key_from_name(const char*s){
    if(!_stricmp(s,"W"))return KEY_W;
    if(!_stricmp(s,"A"))return KEY_A;
    if(!_stricmp(s,"S"))return KEY_S;
    if(!_stricmp(s,"D"))return KEY_D;
    if(!_stricmp(s,"SPACE"))return KEY_SPACE;
    if(!_stricmp(s,"E"))return KEY_E;
    return 0;
}

int main(int argc,char**argv){
    if(argc<2){fprintf(stderr,"E#+ Game Runtime\nUsage: EPlusGameRuntime.exe game.eplus\n");return 1;}
    char*src=read_file(argv[1]); if(!src){fprintf(stderr,"E#+ error: could not read game file\n");return 1;}

    int width=1280,height=720;
    char title[256]="E#+ Game";
    Vector3 player={0,2,6};
    int cubeCount=0;
    Cube cubes[MAX_OBJECTS];
    memset(cubes,0,sizeof(cubes));

    char*c=_strdup(src),*ctx=NULL,*line=strtok_s(c,"\r\n",&ctx);
    while(line){
        char s[2048]; strncpy_s(s,sizeof(s),line,_TRUNCATE); trim(s);
        if(!strncmp(s,"window width ",13)) sscanf_s(s+13,"%d",&width);
        else if(!strncmp(s,"window height ",14)) sscanf_s(s+14,"%d",&height);
        else if(!strncmp(s,"window title ",13)) quoted(s+13,title,sizeof(title));
        else if(!strncmp(s,"player position ",16)) sscanf_s(s+16,"%f %f %f",&player.x,&player.y,&player.z);
        else if(!strcmp(s,"create cube")){
            if(cubeCount<MAX_OBJECTS){
                int i=cubeCount++;
                cubes[i].position=(Vector3){0,0.5f,0};
                cubes[i].size=(Vector3){1,1,1};
                cubes[i].color=WHITE;
                cubes[i].active=1;
            }
        }
        else if(!strncmp(s,"create ",7) && strstr(s+7," cubes")){
            int n=0;
            if(sscanf_s(s+7,"%d cubes",&n)==1){
                if(n<0)n=0;
                if(n>MAX_OBJECTS)n=MAX_OBJECTS;
                for(int k=0;k<n;k++){
                    int i=cubeCount++;
                    int x=k%10,z=k/10;
                    cubes[i].position=(Vector3){(float)(x-4.5f)*2.0f,0.5f,(float)(z-4.5f)*2.0f};
                    cubes[i].size=(Vector3){1,1,1};
                    cubes[i].color=WHITE;
                    cubes[i].active=1;
                }
            }
        }
        line=strtok_s(NULL,"\r\n",&ctx);
    }
    free(c);

    InitWindow(width,height,title);
    SetTargetFPS(120);
    DisableCursor();

    Camera3D camera={0};
    camera.up=(Vector3){0,1,0};
    camera.fovy=70;
    camera.projection=CAMERA_PERSPECTIVE;

    float yaw=0,pitch=0;
    while(!WindowShouldClose()){
        float dt=GetFrameTime(),speed=5.0f;
        Vector3 forward={-sinf(yaw),0,-cosf(yaw)};
        Vector3 right={cosf(yaw),0,-sinf(yaw)};

        if(IsKeyDown(KEY_W)){player.x+=forward.x*speed*dt;player.z+=forward.z*speed*dt;}
        if(IsKeyDown(KEY_S)){player.x-=forward.x*speed*dt;player.z-=forward.z*speed*dt;}
        if(IsKeyDown(KEY_A)){player.x-=right.x*speed*dt;player.z-=right.z*speed*dt;}
        if(IsKeyDown(KEY_D)){player.x+=right.x*speed*dt;player.z+=right.z*speed*dt;}

        Vector2 mouse=GetMouseDelta();
        yaw-=mouse.x*0.0025f;
        pitch+=mouse.y*0.0025f;
        if(pitch>1.4f)pitch=1.4f;
        if(pitch<-1.4f)pitch=-1.4f;

        camera.position=player;
        camera.target=(Vector3){
            player.x-sinf(yaw)*10.0f,
            player.y-sinf(pitch)*10.0f,
            player.z-cosf(yaw)*10.0f
        };

        BeginDrawing();
        ClearBackground((Color){18,22,30,255});
        BeginMode3D(camera);
            DrawPlane((Vector3){0,0,0},(Vector2){100,100},(Color){70,90,70,255});
            DrawGrid(100,1.0f);
            for(int i=0;i<cubeCount;i++){
                if(!cubes[i].active)continue;
                DrawCube(cubes[i].position,cubes[i].size.x,cubes[i].size.y,cubes[i].size.z,cubes[i].color);
                DrawCubeWires(cubes[i].position,cubes[i].size.x,cubes[i].size.y,cubes[i].size.z,BLACK);
            }
        EndMode3D();
        DrawRectangle(12,12,390,62,(Color){0,0,0,160});
        DrawText("E#+ 3D GAME RUNTIME",24,22,20,RAYWHITE);
        DrawText("WASD = move   Mouse = look   ESC = quit",24,48,14,RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    free(src);
    return 0;
}
