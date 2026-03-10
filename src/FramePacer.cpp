#include "FramePacer.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

void FramePacer::Init(int targetFPS) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_freq = freq.QuadPart;
    m_targetInterval = m_freq / targetFPS;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m_nextPresent = now.QuadPart + m_targetInterval;

    // Tell Windows scheduler to use 1ms timer resolution
    timeBeginPeriod(1);
}

void FramePacer::BeginFrame() {
    // Nothing needed here — timing is enforced at PresentVBlank
}

void FramePacer::PresentVBlank(IDXGISwapChain* swapChain) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    // Sleep until 1ms before the target present time
    int64_t sleepTicks = m_nextPresent - now.QuadPart - (m_freq / 1000);
    if (sleepTicks > 0) {
        DWORD sleepMs = (DWORD)(sleepTicks * 1000 / m_freq);
        if (sleepMs > 0) Sleep(sleepMs);
    }

    // Spinlock for the final ~1ms to hit VBlank precisely
    do {
        QueryPerformanceCounter(&now);
    } while (now.QuadPart < m_nextPresent);

    // DWM flush: sync to Windows compositor VBlank
    DwmFlush();

    // Present with DXGI_PRESENT_DO_NOT_WAIT so we don't block if VBlank was missed
    HRESULT hr = swapChain->Present(1, DXGI_PRESENT_DO_NOT_WAIT);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
        // Missed VBlank — skip this frame, don't stall
        swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    }

    // Advance next present target (clamp if we're behind)
    m_nextPresent += m_targetInterval;
    QueryPerformanceCounter(&now);
    if (m_nextPresent < now.QuadPart) {
        // We're running behind — snap back to now to stop spiral
        m_nextPresent = now.QuadPart + m_targetInterval;
    }
}
