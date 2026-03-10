#include "Capture.h"
#include <cstdio>

bool Capture::Init(ID3D11Device* device) {
    // Get the DXGI device from the D3D11 device
    ComPtr<IDXGIDevice> dxgiDevice;
    device->QueryInterface(IID_PPV_ARGS(dxgiDevice.ReleaseAndGetAddressOf()));

    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(adapter.ReleaseAndGetAddressOf());

    // Get primary output (monitor 0)
    ComPtr<IDXGIOutput> output;
    adapter->EnumOutputs(0, output.ReleaseAndGetAddressOf());

    ComPtr<IDXGIOutput1> output1;
    output->QueryInterface(IID_PPV_ARGS(output1.ReleaseAndGetAddressOf()));

    HRESULT hr = output1->DuplicateOutput(device, m_duplication.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        // Common cause: running on secondary GPU or without admin rights
        return false;
    }

    DXGI_OUTDUPL_DESC desc{};
    m_duplication->GetDesc(&desc);
    m_width  = (int)desc.ModeDesc.Width;
    m_height = (int)desc.ModeDesc.Height;

    // Allocate a GPU texture we copy each frame into (GPU→GPU, no CPU readback)
    D3D11_TEXTURE2D_DESC td{};
    td.Width            = m_width;
    td.Height           = m_height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    device->CreateTexture2D(&td, nullptr, m_stagingTex.ReleaseAndGetAddressOf());
    return true;
}

ID3D11Texture2D* Capture::AcquireFrame(ID3D11DeviceContext* ctx) {
    if (m_frameHeld) ReleaseFrame();

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;

    // Timeout 0 = non-blocking: returns immediately if no new frame
    HRESULT hr = m_duplication->AcquireNextFrame(0, &info, resource.ReleaseAndGetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return nullptr; // no new frame
    if (FAILED(hr)) {
        // Lost device or mode change — attempt reinit next call
        m_duplication.Reset();
        return nullptr;
    }

    ComPtr<ID3D11Texture2D> tex;
    resource->QueryInterface(IID_PPV_ARGS(tex.ReleaseAndGetAddressOf()));

    // GPU copy into our persistent texture (keeps binding flags correct)
    ctx->CopyResource(m_stagingTex.Get(), tex.Get());
    m_frameHeld = true;
    return m_stagingTex.Get();
}

void Capture::ReleaseFrame() {
    if (m_frameHeld && m_duplication) {
        m_duplication->ReleaseFrame();
        m_frameHeld = false;
    }
}
