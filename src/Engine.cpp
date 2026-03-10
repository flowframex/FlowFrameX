#include "Engine.h"
#include "Capture.h"
#include "MotionEstimator.h"
#include "Extrapolator.h"
#include "SceneCut.h"
#include "FramePacer.h"
#include <dxgi1_2.h>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY || (msg == WM_KEYDOWN && wp == VK_ESCAPE)) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

Engine::Engine()  = default;
Engine::~Engine() { Shutdown(); }

bool Engine::Init() {
    if (!InitWindow()) return false;
    if (!InitD3D())    return false;

    m_capture   = std::make_unique<Capture>();
    m_motion    = std::make_unique<MotionEstimator>();
    m_sceneCut  = std::make_unique<SceneCut>();
    m_extrap    = std::make_unique<Extrapolator>();
    m_pacer     = std::make_unique<FramePacer>();

    if (!m_capture->Init(m_device.Get())) {
        MessageBoxA(nullptr, "Desktop Duplication failed.\nMake sure you run as Administrator.", "FlowFrameX", MB_ICONERROR);
        return false;
    }

    m_width  = m_capture->Width();
    m_height = m_capture->Height();

    if (!m_motion->Init(m_device.Get(), m_width, m_height))   return false;
    if (!m_extrap->Init(m_device.Get(), m_width, m_height))   return false;
    if (!m_sceneCut->Init(m_device.Get(), m_width, m_height)) return false;

    // Allocate frame history textures
    D3D11_TEXTURE2D_DESC td{};
    td.Width            = m_width;
    td.Height           = m_height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    m_device->CreateTexture2D(&td, nullptr, m_frameN.ReleaseAndGetAddressOf());
    m_device->CreateTexture2D(&td, nullptr, m_frameNm1.ReleaseAndGetAddressOf());
    m_device->CreateShaderResourceView(m_frameN.Get(),   nullptr, m_frameN_SRV.ReleaseAndGetAddressOf());
    m_device->CreateShaderResourceView(m_frameNm1.Get(), nullptr, m_frameNm1_SRV.ReleaseAndGetAddressOf());

    m_pacer->Init(60);  // target 60fps pacing
    m_running = true;
    return true;
}

bool Engine::InitWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FlowFrameX";
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        L"FlowFrameX", L"FlowFrameX v2.0",
        WS_POPUP | WS_VISIBLE,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr
    );
    return m_hwnd != nullptr;
}

bool Engine::InitD3D() {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = w;
    sd.BufferDesc.Height                  = h;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = m_hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL gotLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, levels, 2, D3D11_SDK_VERSION,
        &sd, m_swapChain.ReleaseAndGetAddressOf(),
        m_device.ReleaseAndGetAddressOf(),
        &gotLevel,
        m_ctx.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr)) return false;

    ComPtr<ID3D11Texture2D> backbuf;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(backbuf.ReleaseAndGetAddressOf()));
    m_device->CreateRenderTargetView(backbuf.Get(), nullptr, m_rtv.ReleaseAndGetAddressOf());
    return true;
}

void Engine::Run() {
    MSG msg{};
    while (m_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { m_running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (m_running) ProcessFrame();
    }
}

void Engine::ProcessFrame() {
    // 1. Grab latest screen frame via Desktop Duplication
    ID3D11Texture2D* rawFrame = m_capture->AcquireFrame(m_ctx.Get());
    if (!rawFrame) return;  // no new frame yet, skip

    // 2. Shift history: N-1 = N, N = rawFrame
    m_ctx->CopyResource(m_frameNm1.Get(), m_frameN.Get());
    m_ctx->CopyResource(m_frameN.Get(), rawFrame);
    m_capture->ReleaseFrame();

    // 3. Scene cut detection
    float energy = m_sceneCut->Compute(m_ctx.Get(), m_frameN_SRV.Get(), m_frameNm1_SRV.Get());
    bool  isCut  = (energy > cfg.sceneCutThr);

    // 4. Adjust alpha: kill on cut, ramp back over 4 frames
    float alpha = isCut ? 0.0f : cfg.alpha;
    static float alphaRamp = 0.0f;
    alphaRamp = isCut ? 0.0f : fminf(cfg.alpha, alphaRamp + cfg.alpha * 0.25f);
    alpha = alphaRamp;

    // 5. Pyramid motion estimation (returns MV texture)
    ID3D11ShaderResourceView* mvSRV =
        m_motion->Estimate(m_ctx.Get(), m_frameN_SRV.Get(), m_frameNm1_SRV.Get());

    // 6. Gather warp → extrapolated frame
    ID3D11ShaderResourceView* predSRV =
        m_extrap->Extrapolate(m_ctx.Get(), m_frameN_SRV.Get(), mvSRV, alpha);

    // 7. Present extrapolated frame on VBlank
    m_pacer->BeginFrame();

    // Blit predSRV to backbuffer (full-screen quad — handled in Extrapolator)
    m_extrap->Blit(m_ctx.Get(), m_rtv.Get(), predSRV);

    // DWM flush → VBlank sync → present
    m_pacer->PresentVBlank(m_swapChain.Get());
}

void Engine::Shutdown() {
    m_running = false;
    if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
}
