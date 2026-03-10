#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

class Capture;
class MotionEstimator;
class Extrapolator;
class SceneCut;
class FramePacer;

struct EngineConfig {
    float   alpha       = 1.0f;   // extrapolation strength (0.5–1.0)
    float   sceneCutThr = 0.12f;  // scene-cut energy threshold
    float   sharpLambda = 0.3f;   // RCAS sharpening strength
    int     renderScale = 75;     // % of screen to render at (e.g. 75 = 75%)
    bool    showOverlay = true;
};

class Engine {
public:
    Engine();
    ~Engine();

    bool Init();
    void Run();
    void Shutdown();

    EngineConfig cfg;

private:
    bool InitD3D();
    bool InitWindow();
    void MainLoop();
    void ProcessFrame();

    HWND                        m_hwnd      = nullptr;
    ComPtr<ID3D11Device>        m_device;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<IDXGISwapChain>      m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_rtv;

    std::unique_ptr<Capture>         m_capture;
    std::unique_ptr<MotionEstimator> m_motion;
    std::unique_ptr<Extrapolator>    m_extrap;
    std::unique_ptr<SceneCut>        m_sceneCut;
    std::unique_ptr<FramePacer>      m_pacer;

    // Two-frame history for extrapolation
    ComPtr<ID3D11Texture2D>          m_frameN;    // current frame
    ComPtr<ID3D11Texture2D>          m_frameNm1;  // previous frame
    ComPtr<ID3D11ShaderResourceView> m_frameN_SRV;
    ComPtr<ID3D11ShaderResourceView> m_frameNm1_SRV;

    int  m_width  = 0;
    int  m_height = 0;
    bool m_running = false;
};
