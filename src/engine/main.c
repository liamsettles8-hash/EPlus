#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "lexer.h"

int run_eplus(const char *s);

static int is_game_source(const char *s) {
    const char *p = s;
    while (*p) {
        char line[2048];
        size_t n = 0;
        while (*p && *p != '\n' && n + 1 < sizeof(line)) line[n++] = *p++;
        line[n] = 0;
        while (n && (line[n-1] == '\r' || isspace((unsigned char)line[n-1]))) line[--n] = 0;
        char *q = line;
        while (*q && isspace((unsigned char)*q)) q++;
        if (!strncmp(q, "game ", 5) || !strncmp(q, "window ", 7) ||
            !strncmp(q, "camera ", 7) || !strncmp(q, "load model ", 11) ||
            !strncmp(q, "spawn model ", 12) || !strncmp(q, "player create ", 14))
            return 1;
        if (*p == '\n') p++;
    }
    return 0;
}

static int launch_game_runtime(const char *enginePath, const char *gamePath) {
    char runtime[MAX_PATH];
    char command[32768];
    char *slash;

    strncpy_s(runtime, sizeof(runtime), enginePath, _TRUNCATE);
    slash = strrchr(runtime, '\\');
    if (!slash) slash = strrchr(runtime, '/');
    if (!slash) return 0;
    slash[1] = 0;
    strcat_s(runtime, sizeof(runtime), "EPlusGameRuntime.exe");

    if (GetFileAttributesA(runtime) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "E#+ error: EPlusGameRuntime.exe was not found next to the engine.\\n");
        return 0;
    }

    snprintf(command, sizeof(command), ""%s" "%s"", runtime, gamePath);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "E#+ error: could not launch the 3D game runtime (Windows error %lu).\\n",
                (unsigned long)GetLastError());
        return 0;
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    return (int)exitCode;
}

int main(int argc, char **argv) {
    char *s;

    if (argc < 2) {
        fprintf(stderr, "E#+ engine 0.3\\nUsage: eplus-engine file.eplus\\n");
        return 1;
    }

    s = read_entire_file(argv[1]);
    if (!s) {
        fprintf(stderr, "E#+ error: could not read file\\n");
        return 1;
    }

    /*
       Game files are handed to the native E#+ 3D runtime.
       This keeps the language source as the single game description while
       allowing the runtime to own the real-time window/input/render loop.
    */
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
