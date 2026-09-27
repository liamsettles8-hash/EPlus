#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *p){
    FILE*f=fopen(p,"rb"); long n; char*b;
    if(!f)return NULL;
    fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
    b=(char*)malloc((size_t)n+1);
    if(!b){fclose(f);return NULL;}
    fread(b,1,(size_t)n,f); b[n]=0; fclose(f); return b;
}

int main(int argc,char**argv){
    if(argc<2){
        fprintf(stderr,"E#+ Game Runtime\nUsage: EPlusGameRuntime.exe game.eplus\n");
        return 1;
    }

    char*src=read_file(argv[1]);
    if(!src){
        fprintf(stderr,"E#+ error: could not read game file\n");
        return 1;
    }

    /*
       The runtime is intentionally generic.
       E#+ itself owns the game language and scene creation.
       This executable only provides the native window/rendering host.
       No game objects, controls, cubes, models, or movement are hardcoded here.
    */

    InitWindow(1280,720,"E#+ Game");
    SetTargetFPS(120);

    while(!WindowShouldClose()){
        BeginDrawing();
        ClearBackground((Color){18,22,30,255});
        EndDrawing();
    }

    CloseWindow();
    free(src);
    return 0;
}
