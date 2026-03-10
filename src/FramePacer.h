#pragma once
#include <Windows.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

// VBlank-synced frame pacing.
// Sleep until 1ms before VBlank, then spinlock for precision.
// Swaps on VBlank only → zero tearing.
class FramePacer {
public:
    void Init(int targetFPS);
    void BeginFrame();
    void PresentVBlank(IDXGISwapChain* swapChain);

private:
    int64_t  m_targetInterval = 0;  // ticks between frames
    int64_t  m_nextPresent    = 0;  // absolute tick of next present
    int64_t  m_freq           = 0;  // QPC frequency
};
