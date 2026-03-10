#include <Windows.h>
#include "Engine.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Engine engine;

    // Config — tweak these for your setup
    engine.cfg.alpha        = 1.0f;   // full extrapolation (1 frame ahead)
    engine.cfg.sceneCutThr  = 0.12f;  // raise if too many false cut detections
    engine.cfg.sharpLambda  = 0.3f;   // RCAS sharpening (0 = off, 0.5 = aggressive)
    engine.cfg.renderScale  = 75;     // render at 75% res → upscale to native

    if (!engine.Init()) return 1;
    engine.Run();
    return 0;
}
