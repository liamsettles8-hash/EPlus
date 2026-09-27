#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

int main(int argc,char**argv){
    if(argc<2){fprintf(stderr,"E#+ Game Runtime\nUsage: EPlusGameRuntime.exe game.eplus\n");return 1;}
    char*src=read_file(argv[1]); if(!src){fprintf(stderr,"E#+ error: could not read game file\n");return 1;}

    int width=1280,height=720;
    char title[256]="E#+ Game";
    char modelPath[512]={0};
    Vector3 player={0,2,6}, spawn={0,0,0};

    char*c=_strdup(src),*ctx=NULL,*line=strtok_s(c,"\r\n",&ctx);
    while(line){
        char s[2048]; strncpy_s(s,sizeof(s),line,_TRUNCATE); trim(s);
        if(!strncmp(s,"window width ",13)) sscanf_s(s+13,"%d",&width);
        else if(!strncmp(s,"window height ",14)) sscanf_s(s+14,"%d",&height);
        else if(!strncmp(s,"window title ",13)) quoted(s+13,title,sizeof(title));
        else if(!strncmp(s,"player position ",16)) sscanf_s(s+16,"%f %f %f",&player.x,&player.y,&player.z);
        else if(!strncmp(s,"load model ",11)) quoted(s+11,modelPath,sizeof(modelPath));
        line=strtok_s(NULL,"\r\n",&ctx);
    }
    free(c);

    InitWindow(width,height,title);
    SetTargetFPS(120);
    DisableCursor();

    Camera3D camera={0};
    camera.position=player;
    camera.target=(Vector3){player.x,player.y,player.z-1};
    camera.up=(Vector3){0,1,0};
    camera.fovy=70;
    camera.projection=CAMERA_PERSPECTIVE;

    Model model={0};
    int haveModel=0;
    if(modelPath[0]){
        char full[1024];
        const char*slash=strrchr(argv[1],'\\');
        if(!slash)slash=strrchr(argv[1],'/');
        if(slash){
            size_t n=(size_t)(slash-argv[1]+1);
            memcpy(full,argv[1],n); full[n]=0; strcat_s(full,sizeof(full),modelPath);
        }else strncpy_s(full,sizeof(full),modelPath,_TRUNCATE);
        model=LoadModel(full);
        haveModel=(model.meshCount>0);
    }

    float yaw=0,pitch=0;
    while(!WindowShouldClose()){
        float dt=GetFrameTime(),speed=5.0f;
        Vector3 forward={sinf(yaw),0,cosf(yaw)};
        if(IsKeyDown(KEY_W)){player.x-=forward.x*speed*dt;player.z-=forward.z*speed*dt;}
        if(IsKeyDown(KEY_S)){player.x+=forward.x*speed*dt;player.z+=forward.z*speed*dt;}
        if(IsKeyDown(KEY_A)){player.x-=forward.z*speed*dt;player.z+=forward.x*speed*dt;}
        if(IsKeyDown(KEY_D)){player.x+=forward.z*speed*dt;player.z-=forward.x*speed*dt;}

        Vector2 mouse=GetMouseDelta();
        yaw-=mouse.x*0.0025f; pitch-=mouse.y*0.0025f;
        if(pitch>1.4f)pitch=1.4f; if(pitch<-1.4f)pitch=-1.4f;

        camera.position=player;
        camera.target=(Vector3){
            player.x+sinf(yaw)*-10.0f,
            player.y+sinf(pitch)*-10.0f,
            player.z+cosf(yaw)*-10.0f
        };

        BeginDrawing();
        ClearBackground((Color){18,22,30,255});
        BeginMode3D(camera);
            DrawPlane((Vector3){0,0,0},(Vector2){100,100},(Color){70,90,70,255});
            DrawGrid(100,1.0f);
            DrawCube((Vector3){0,0.5f,0},1,1,1,(Color){90,120,180,255});
            if(haveModel)DrawModel(model,spawn,1.0f,WHITE);
        EndMode3D();
        DrawRectangle(12,12,360,62,(Color){0,0,0,160});
        DrawText("E#+ 3D GAME RUNTIME",24,22,20,RAYWHITE);
        DrawText("WASD = move   Mouse = look   ESC = quit",24,48,14,RAYWHITE);
        EndDrawing();
    }

    if(haveModel)UnloadModel(model);
    CloseWindow();
    free(src);
    return 0;
}