#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Captures the screen using DXGI Desktop Duplication API (no OBS needed)
class Capture {
public:
    bool Init(ID3D11Device* device);

    // Returns a texture pointer valid until ReleaseFrame() is called.
    // Returns nullptr if no new frame is available yet.
    ID3D11Texture2D* AcquireFrame(ID3D11DeviceContext* ctx);
    void             ReleaseFrame();

    int Width()  const { return m_width; }
    int Height() const { return m_height; }

private:
    ComPtr<IDXGIOutputDuplication> m_duplication;
    ComPtr<ID3D11Texture2D>        m_stagingTex;
    int m_width  = 0;
    int m_height = 0;
    bool m_frameHeld = false;
};
