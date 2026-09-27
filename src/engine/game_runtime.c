#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int width, height, cubes;
    char title[256];
    float px, py, pz, speed;
    int firstPerson;
} Scene;

static int load_scene(const char *path, Scene *s) {
    FILE *f = fopen(path, "r");
    char line[1024];
    if (!f) return 0;
    memset(s, 0, sizeof(*s));
    s->width = 1280; s->height = 720;
    strcpy_s(s->title, sizeof(s->title), "E#+ Game");
    s->py = 2; s->pz = 12; s->speed = 5;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf_s(line, "WINDOW %d %d", &s->width, &s->height) == 2) continue;
        if (sscanf_s(line, "PLAYER %f %f %f %f", &s->px, &s->py, &s->pz, &s->speed) == 4) continue;
        if (sscanf_s(line, "CUBES %d", &s->cubes) == 1) continue;
        if (!strncmp(line, "TITLE ", 6)) {
            char *p = line + 6;
            size_t n = strlen(p);
            while (n && (p[n-1] == '\n' || p[n-1] == '\r')) p[--n] = 0;
            strncpy_s(s->title, sizeof(s->title), p, _TRUNCATE);
            continue;
        }
        if (!strncmp(line, "CAMERA first_person", 20)) s->firstPerson = 1;
    }
    fclose(f);
    return 1;
}

static void draw_cube_field(int count) {
    int side = 1;
    while (side * side < count) side++;
    for (int i = 0; i < count; i++) {
        int x = i % side;
        int z = i / side;
        Vector3 pos = { (float)(x - side/2) * 2.0f, 0.5f, (float)(z - side/2) * 2.0f };
        DrawCube(pos, 1.0f, 1.0f, 1.0f, (Color){120, 170, 220, 255});
        DrawCubeWires(pos, 1.0f, 1.0f, 1.0f, BLACK);
    }
}

int main(int argc, char **argv) {
    Scene scene;
    Camera3D camera = {0};
    if (argc < 2 || !load_scene(argv[1], &scene)) {
        fprintf(stderr, "E#+ Game Runtime: invalid scene.\n");
        return 1;
    }

    InitWindow(scene.width, scene.height, scene.title);
    SetTargetFPS(120);
    DisableCursor();

    camera.position = (Vector3){scene.px, scene.py, scene.pz};
    camera.target = (Vector3){scene.px, scene.py, scene.pz - 1.0f};
    camera.up = (Vector3){0,1,0};
    camera.fovy = 70;
    camera.projection = CAMERA_PERSPECTIVE;

    float yaw = -90.0f, pitch = 0.0f;

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMouseDelta();
        yaw += mouse.x * 0.10f;
        pitch -= mouse.y * 0.10f;
        if (pitch > 89) pitch = 89;
        if (pitch < -89) pitch = -89;

        Vector3 forward = {
            cosf(DEG2RAD * yaw) * cosf(DEG2RAD * pitch),
            sinf(DEG2RAD * pitch),
            sinf(DEG2RAD * yaw) * cosf(DEG2RAD * pitch)
        };
        Vector3 flat = Vector3Normalize((Vector3){forward.x, 0, forward.z});
        Vector3 right = Vector3Normalize(Vector3CrossProduct(flat, (Vector3){0,1,0}));
        float dt = GetFrameTime();
        float amount = scene.speed * dt;

        if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(flat, amount));
        if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(flat, amount));
        if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, amount));
        if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, amount));

        camera.target = Vector3Add(camera.position, forward);

        BeginDrawing();
        ClearBackground((Color){20, 24, 32, 255});
        BeginMode3D(camera);
        DrawPlane((Vector3){0,0,0}, (Vector2){200,200}, (Color){70,75,82,255});
        draw_cube_field(scene.cubes);
        EndMode3D();
        DrawText("E#+ native 3D runtime", 16, 16, 20, RAYWHITE);
        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
    return 0;
}
