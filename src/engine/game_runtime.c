#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_ENEMIES 128

typedef struct {int width,height,cubes,maxEnemies,winScore,moveForward,moveBackward,moveLeft,moveRight,canFire;float arena,px,py,pz,speed,health,enemyHealth,enemySpeed,enemyDamage,weaponDamage,weaponRate,spawnRate,winTime;char title[256],playerName[128],weaponName[128],enemyName[128];} Scene;
typedef struct {Vector3 pos;float health;int alive;} Enemy;

static int load_scene(const char*p,Scene*s){
 FILE*f=fopen(p,"r");char line[1024];if(!f)return 0;memset(s,0,sizeof(*s));s->width=1280;s->height=720;s->py=2;s->pz=12;s->speed=5;s->health=100;s->enemyHealth=30;s->enemySpeed=2;s->enemyDamage=10;s->weaponDamage=15;s->weaponRate=.2f;s->spawnRate=2;s->maxEnemies=20;s->winScore=1000;s->arena=40;strcpy_s(s->title,sizeof(s->title),"E#+ Game");strcpy_s(s->playerName,sizeof(s->playerName),"Hero");strcpy_s(s->weaponName,sizeof(s->weaponName),"Blaster");strcpy_s(s->enemyName,sizeof(s->enemyName),"Enemy");s->moveForward=0;s->moveBackward=0;s->moveLeft=0;s->moveRight=0;s->canFire=0;
 while(fgets(line,sizeof(line),f)){
  if(sscanf_s(line,"WINDOW_WIDTH %d",&s->width)==1)continue;if(sscanf_s(line,"WINDOW_HEIGHT %d",&s->height)==1)continue;
  if(!strncmp(line,"TITLE ",6)){sscanf_s(line+6,"%255[^\r\n]",s->title,(unsigned)_countof(s->title));continue;}
  if(!strncmp(line,"PLAYER_NAME ",12)){sscanf_s(line+12,"%127[^\r\n]",s->playerName,(unsigned)_countof(s->playerName));continue;}
  if(sscanf_s(line,"PLAYER_POS %f %f %f",&s->px,&s->py,&s->pz)==3)continue;if(sscanf_s(line,"PLAYER_SPEED %f",&s->speed)==1)continue;if(sscanf_s(line,"PLAYER_HEALTH %f",&s->health)==1)continue;
  if(sscanf_s(line,"ARENA %f",&s->arena)==1)continue;if(sscanf_s(line,"ENEMY_NAME %127[^\r\n]",s->enemyName,(unsigned)_countof(s->enemyName))==1)continue;
  if(sscanf_s(line,"ENEMY_HEALTH %f",&s->enemyHealth)==1)continue;if(sscanf_s(line,"ENEMY_SPEED %f",&s->enemySpeed)==1)continue;if(sscanf_s(line,"ENEMY_DAMAGE %f",&s->enemyDamage)==1)continue;
  if(sscanf_s(line,"WEAPON_NAME %127[^\r\n]",s->weaponName,(unsigned)_countof(s->weaponName))==1)continue;if(sscanf_s(line,"WEAPON_DAMAGE %f",&s->weaponDamage)==1)continue;if(sscanf_s(line,"WEAPON_RATE %f",&s->weaponRate)==1)continue;
  if(sscanf_s(line,"CUBES %d",&s->cubes)==1)continue;if(sscanf_s(line,"WIN_SCORE %d",&s->winScore)==1)continue;if(sscanf_s(line,"WIN_TIME %f",&s->winTime)==1)continue;if(sscanf_s(line,"SPAWN_RATE %f",&s->spawnRate)==1)continue;if(sscanf_s(line,"MAX_ENEMIES %d",&s->maxEnemies)==1)continue;if(!strncmp(line,"ACTION ",7)){const char*a=line+7;if(strstr(a,"key \"W\""))s->moveForward=1;if(strstr(a,"key \"S\""))s->moveBackward=1;if(strstr(a,"key \"A\""))s->moveLeft=1;if(strstr(a,"key \"D\""))s->moveRight=1;if(strstr(a,"mouse button \"left\""))s->canFire=1;continue;}
 }fclose(f);return 1;
}

static void spawn_enemy(Enemy*e,const Scene*s,Vector3 player){
 float a=(float)GetRandomValue(0,359)*DEG2RAD,d=(float)GetRandomValue((int)(s->arena*.35f),(int)(s->arena*.48f));
 e->pos=(Vector3){player.x+cosf(a)*d,.8f,player.z+sinf(a)*d};e->health=s->enemyHealth;e->alive=1;
}
static void draw_arena(const Scene*s){
 DrawPlane((Vector3){0,0,0},(Vector2){s->arena,s->arena},(Color){62,68,76,255});
 DrawCube((Vector3){0,1,-s->arena/2},(float)s->arena,2,1,(Color){35,38,45,255});
 DrawCube((Vector3){0,1,s->arena/2},(float)s->arena,2,1,(Color){35,38,45,255});
 DrawCube((Vector3){-s->arena/2,1,0},1,2,(float)s->arena,(Color){35,38,45,255});
 DrawCube((Vector3){s->arena/2,1,0},1,2,(float)s->arena,(Color){35,38,45,255});
}
static int shoot(Enemy*es,int n,Vector3 origin,Vector3 dir,float damage){
 int hit=-1;float best=100000;
 for(int i=0;i<n;i++)if(es[i].alive){Vector3 to=Vector3Subtract(es[i].pos,origin);float dist=Vector3Length(to);if(dist>60)continue;Vector3 nd=Vector3Normalize(to);float dot=Vector3DotProduct(nd,dir);if(dot>.985f&&dist<best){best=dist;hit=i;}}
 if(hit>=0)es[hit].health-=damage;return hit;
}

int main(int argc,char**argv){
 Scene s;if(argc<2||!load_scene(argv[1],&s)){fprintf(stderr,"E#+ Game Runtime: invalid scene.\n");return 1;}
 InitWindow(s.width,s.height,s.title);SetTargetFPS(120);DisableCursor();
 Camera3D cam={0};cam.position=(Vector3){s.px,s.py,s.pz};cam.up=(Vector3){0,1,0};cam.fovy=70;cam.projection=CAMERA_PERSPECTIVE;
 float yaw=-90,pitch=0,playerHP=s.health,score=0,elapsed=0,spawnClock=0,fireClock=0;int kills=0,started=0,gameOver=0,won=0,n=0;Enemy enemies[MAX_ENEMIES]={0};
 while(!WindowShouldClose()){
  float dt=GetFrameTime();if(!started){started=1;elapsed=0;}
  if(!gameOver&&!won){elapsed+=dt;spawnClock+=dt;fireClock-=dt;
   Vector2 m=GetMouseDelta();yaw+=m.x*.10f;pitch-=m.y*.10f;if(pitch>89)pitch=89;if(pitch<-89)pitch=-89;
   Vector3 forward={cosf(yaw*DEG2RAD)*cosf(pitch*DEG2RAD),sinf(pitch*DEG2RAD),sinf(yaw*DEG2RAD)*cosf(pitch*DEG2RAD)};
   Vector3 flat=Vector3Normalize((Vector3){forward.x,0,forward.z});Vector3 right=Vector3Normalize(Vector3CrossProduct(flat,(Vector3){0,1,0}));
   float a=s.speed*dt;if(IsKeyDown(KEY_W))cam.position=Vector3Add(cam.position,Vector3Scale(flat,a));if(IsKeyDown(KEY_S))cam.position=Vector3Subtract(cam.position,Vector3Scale(flat,a));if(IsKeyDown(KEY_A))cam.position=Vector3Subtract(cam.position,Vector3Scale(right,a));if(IsKeyDown(KEY_D))cam.position=Vector3Add(cam.position,Vector3Scale(right,a));
   if(cam.position.x<-s.arena/2+1)cam.position.x=-s.arena/2+1;if(cam.position.x>s.arena/2-1)cam.position.x=s.arena/2-1;if(cam.position.z<-s.arena/2+1)cam.position.z=-s.arena/2+1;if(cam.position.z>s.arena/2-1)cam.position.z=s.arena/2-1;
   cam.target=Vector3Add(cam.position,forward);
   if(spawnClock>=s.spawnRate&&n<s.maxEnemies&&n<MAX_ENEMIES){spawnClock=0;spawn_enemy(&enemies[n++],&s,cam.position);}
   if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&fireClock<=0){fireClock=s.weaponRate;int h=shoot(enemies,n,cam.position,forward,s.weaponDamage);if(h>=0&&enemies[h].health<=0){enemies[h].alive=0;score+=100;kills++;}}
   for(int i=0;i<n;i++)if(enemies[i].alive){Vector3 d=Vector3Subtract(cam.position,enemies[i].pos);float len=Vector3Length(d);if(len>1.3f){enemies[i].pos=Vector3Add(enemies[i].pos,Vector3Scale(Vector3Normalize(d),s.enemySpeed*dt));}else{playerHP-=s.enemyDamage*dt;}}
   if(playerHP<=0){playerHP=0;gameOver=1;}if(score>=s.winScore||(s.winTime>0&&elapsed>=s.winTime))won=1;
  } else if(IsKeyPressed(KEY_R)){playerHP=s.health;score=0;elapsed=0;kills=0;n=0;memset(enemies,0,sizeof(enemies));gameOver=0;won=0;cam.position=(Vector3){s.px,s.py,s.pz};}
  BeginDrawing();ClearBackground((Color){18,22,30,255});BeginMode3D(cam);draw_arena(&s);
  for(int i=0;i<n;i++)if(enemies[i].alive){float scale=.9f+0.15f*sinf((float)GetTime()*4+i);DrawCube(enemies[i].pos,scale,scale,scale,(Color){210,70,75,255});DrawCubeWires(enemies[i].pos,scale,scale,scale,BLACK);}
  EndMode3D();
  DrawText(TextFormat("HEALTH %.0f",playerHP),20,20,22,RAYWHITE);DrawText(TextFormat("SCORE %d",(int)score),20,48,22,RAYWHITE);DrawText(TextFormat("KILLS %d",kills),20,76,22,RAYWHITE);DrawText(TextFormat("TIME %.1f",elapsed),20,104,22,RAYWHITE);
  DrawText(TextFormat("WASD move   Mouse look   LMB fire   R restart"),20,s.height-35,18,RAYWHITE);
  if(gameOver){DrawRectangle(0,0,s.width,s.height,(Color){0,0,0,150});DrawText("GAME OVER",s.width/2-110,s.height/2-35,40,RED);DrawText("Press R to restart",s.width/2-105,s.height/2+20,20,RAYWHITE);}
  if(won){DrawRectangle(0,0,s.width,s.height,(Color){0,0,0,150});DrawText("YOU WIN!",s.width/2-90,s.height/2-35,40,GREEN);DrawText("Press R to play again",s.width/2-120,s.height/2+20,20,RAYWHITE);}
  EndDrawing();
 }
 EnableCursor();CloseWindow();return 0;
}
