#include <Windows.h>
#include <cstdio>
#include "Engine.h"

// Force a console window so we can see what's going wrong
static void OpenConsole() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    SetConsoleTitleW(L"FlowFrameX Debug Console");
}

static bool CheckRequirements() {
    bool ok = true;

    // Check 1: Are we running as Administrator?
    BOOL isAdmin = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev{};
        DWORD size = sizeof(elev);
        if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size))
            isAdmin = elev.TokenIsElevated;
        CloseHandle(token);
    }
    if (!isAdmin) {
        printf("[ERROR] Not running as Administrator!\n");
        printf("        Right-click FlowFrameX.exe -> 'Run as administrator'\n\n");
        ok = false;
    } else {
        printf("[OK] Running as Administrator\n");
    }

    // Check 2: Do the shader files exist?
    const char* shaders[] = {
        "shaders/Downsample.hlsl",
        "shaders/MotionEstimate.hlsl",
        "shaders/GatherWarp.hlsl",
        "shaders/SceneCut.hlsl",
        "shaders/Blit.hlsl"
    };
    for (auto& s : shaders) {
        DWORD attr = GetFileAttributesA(s);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            printf("[ERROR] Missing shader: %s\n", s);
            printf("        Make sure the 'shaders' folder is in the SAME folder as the exe!\n\n");
            ok = false;
        } else {
            printf("[OK] Found shader: %s\n", s);
        }
    }

    // Check 3: Is Windows 10 or higher?
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = 10;
    DWORDLONG mask = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    if (!VerifyVersionInfoW(&osvi, VER_MAJORVERSION, mask)) {
        printf("[ERROR] Windows 10 or higher required for Desktop Duplication API\n\n");
        ok = false;
    } else {
        printf("[OK] Windows version OK\n");
    }

    printf("\n[INFO] If a game is running in FULLSCREEN EXCLUSIVE mode,\n");
    printf("       Desktop Duplication will fail. Switch the game to\n");
    printf("       BORDERLESS WINDOWED mode first!\n\n");

    return ok;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    OpenConsole();

    printf("========================================\n");
    printf("  FlowFrameX v2.0 -- Starting up...\n");
    printf("========================================\n\n");

    if (!CheckRequirements()) {
        printf("\n[FATAL] Fix the errors above, then restart.\n");
        printf("Press ENTER to exit...\n");
        getchar();
        return 1;
    }

    printf("\n[INFO] All checks passed. Initializing engine...\n\n");

    Engine engine;
    engine.cfg.alpha       = 1.0f;
    engine.cfg.sceneCutThr = 0.12f;
    engine.cfg.sharpLambda = 0.3f;
    engine.cfg.renderScale = 75;

    printf("[INFO] Config:\n");
    printf("       alpha       = %.2f\n", engine.cfg.alpha);
    printf("       sceneCutThr = %.2f\n", engine.cfg.sceneCutThr);
    printf("       sharpLambda = %.2f\n", engine.cfg.sharpLambda);
    printf("       renderScale = %d%%\n\n", engine.cfg.renderScale);

    if (!engine.Init()) {
        printf("\n[FATAL] Engine init failed!\n");
        printf("Common causes:\n");
        printf("  1. Game is in FULLSCREEN EXCLUSIVE mode (switch to borderless windowed)\n");
        printf("  2. You have 2 GPUs (iGPU + GT730) - disable iGPU in Device Manager\n");
        printf("  3. A shader failed to compile (check errors above)\n");
        printf("  4. DirectX 11 not supported on this GPU\n");
        printf("\nPress ENTER to exit...\n");
        getchar();
        return 1;
    }

    printf("[OK] Engine started! Press ESC to exit.\n\n");
    engine.Run();
    return 0;
}
