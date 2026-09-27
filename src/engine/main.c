#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "lexer.h"

int run_eplus(const char *s);

static char *trim(char *s) {
    char *e;
    while (*s && isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) --e;
    *e = 0;
    return s;
}

static int is_game_source(const char *s) {
    const char *p = s;
    while (*p) {
        char line[2048];
        size_t n = 0;
        while (*p && *p != '\n' && n + 1 < sizeof(line)) line[n++] = *p++;
        line[n] = 0;
        char *q = trim(line);
        if (!strncmp(q, "game ", 5) || !strncmp(q, "window ", 7) ||
            !strncmp(q, "camera ", 7) || !strncmp(q, "player ", 7) ||
            !strncmp(q, "create ", 7) || !strncmp(q, "load model ", 11) ||
            !strncmp(q, "spawn model ", 12))
            return 1;
        if (*p == '\n') p++;
    }
    return 0;
}

static void write_scene_string(FILE *f, const char *key, const char *line) {
    const char *q = strchr(line, '"');
    if (!q) return;
    q++;
    {
        const char *e = strrchr(q, '"');
        if (e && e > q) fprintf(f, "%s %.*s\n", key, (int)(e-q), q);
    }
}

static int compile_game(const char *src, const char *scenePath) {
    char *copy = (char*)malloc(strlen(src) + 1);
    char *ctx = NULL, *line;
    FILE *f;
    int width = 1280, height = 720, cubes = 0;
    float px = 0, py = 2, pz = 12, speed = 5;

    if (!copy) return 0;
    strcpy(copy, src);
    f = fopen(scenePath, "w");
    if (!f) { free(copy); return 0; }

    fprintf(f, "EPLUS_SCENE 1\n");

    line = strtok_s(copy, "\r\n", &ctx);
    while (line) {
        char *s = trim(line);

        if (!strncmp(s, "game ", 5)) write_scene_string(f, "TITLE", s);
        else if (!strncmp(s, "window width ", 13)) sscanf_s(s + 13, "%d", &width);
        else if (!strncmp(s, "window height ", 14)) sscanf_s(s + 14, "%d", &height);
        else if (!strncmp(s, "window title ", 13)) write_scene_string(f, "TITLE", s);
        else if (!strncmp(s, "camera ", 7)) {
            if (strstr(s, "first person")) fprintf(f, "CAMERA first_person\n");
            else fprintf(f, "CAMERA free\n");
        }
        else if (!strncmp(s, "player position ", 16))
            sscanf_s(s + 16, "%f %f %f", &px, &py, &pz);
        else if (!strncmp(s, "player speed ", 13))
            sscanf_s(s + 13, "%f", &speed);
        else if (!strncmp(s, "create ", 7) && strstr(s, " cubes")) {
            sscanf_s(s + 7, "%d", &cubes);
        }

        line = strtok_s(NULL, "\r\n", &ctx);
    }

    fprintf(f, "WINDOW %d %d\n", width, height);
    fprintf(f, "PLAYER %.3f %.3f %.3f %.3f\n", px, py, pz, speed);
    fprintf(f, "CUBES %d\n", cubes);
    fprintf(f, "INPUT WASD_MOUSE_LOOK\n");
    fclose(f);
    free(copy);
    return 1;
}

static int launch_game_runtime(const char *enginePath, const char *gamePath) {
    char runtime[MAX_PATH], scene[MAX_PATH], command[32768];
    char *slash;
    DWORD attr;

    strncpy_s(runtime, sizeof(runtime), enginePath, _TRUNCATE);
    slash = strrchr(runtime, '\\');
    if (!slash) slash = strrchr(runtime, '/');
    if (!slash) return 0;
    slash[1] = 0;
    strcat_s(runtime, sizeof(runtime), "EPlusGameRuntime.exe");

    attr = GetFileAttributesA(runtime);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "E#+ error: EPlusGameRuntime.exe was not found next to the engine.\n");
        return 0;
    }

    GetTempPathA(sizeof(scene), scene);
    strcat_s(scene, sizeof(scene), "eplus_game.scene");
    {
        char *src = read_entire_file(gamePath);
        int ok;
        if (!src) return 0;
        ok = compile_game(src, scene);
        free(src);
        if (!ok) {
            fprintf(stderr, "E#+ error: could not compile game scene.\n");
            return 0;
        }
    }

    snprintf(command, sizeof(command), "\"%s\" \"%s\"", runtime, scene);

    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        DWORD exitCode = 1;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);

        if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            fprintf(stderr, "E#+ error: could not launch game runtime (Windows error %lu).\n",
                    (unsigned long)GetLastError());
            DeleteFileA(scene);
            return 0;
        }

        CloseHandle(pi.hThread);
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        DeleteFileA(scene);
        return (int)exitCode;
    }
}

int main(int argc, char **argv) {
    char *s;
    if (argc < 2) {
        fprintf(stderr, "E#+ engine 0.4\nUsage: eplus-engine file.eplus\n");
        return 1;
    }

    s = read_entire_file(argv[1]);
    if (!s) {
        fprintf(stderr, "E#+ error: could not read file\n");
        return 1;
    }

    if (is_game_source(s)) {
        int r = launch_game_runtime(argv[0], argv[1]);
        free(s);
        return r;
    }

    {
        int r = run_eplus(s);
        free(s);
        return r;
    }
}
