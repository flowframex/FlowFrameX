#include "Extrapolator.h"
#include "ShaderCompiler.h"

bool Extrapolator::Init(ID3D11Device* device, int width, int height) {
    m_width  = width;
    m_height = height;

    m_csWarp  = ShaderCompiler::LoadCS(device, L"shaders/GatherWarp.hlsl", "CSWarp");
    m_vsQuad  = ShaderCompiler::LoadVS(device, L"shaders/Blit.hlsl",       "VSMain");
    m_psBlit  = ShaderCompiler::LoadPS(device, L"shaders/Blit.hlsl",       "PSMain");
    if (!m_csWarp || !m_vsQuad || !m_psBlit) return false;

    // Output texture for predicted frame
    D3D11_TEXTURE2D_DESC td{};
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    device->CreateTexture2D(&td, nullptr, m_predicted.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(m_predicted.Get(),  nullptr, m_predicted_SRV.ReleaseAndGetAddressOf());
    device->CreateUnorderedAccessView(m_predicted.Get(), nullptr, m_predicted_UAV.ReleaseAndGetAddressOf());

    // Constant buffer for alpha value
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth      = 16;  // float4 aligned
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, m_cbAlpha.ReleaseAndGetAddressOf());

    // Bilinear sampler (hardware-accelerated on any DX11 GPU)
    D3D11_SAMPLER_DESC sd{};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sd, m_sampler.ReleaseAndGetAddressOf());

    return true;
}

ID3D11ShaderResourceView* Extrapolator::Extrapolate(
    ID3D11DeviceContext* ctx,
    ID3D11ShaderResourceView* frameN,
    ID3D11ShaderResourceView* mvField,
    float alpha)
{
    // Update alpha constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ctx->Map(m_cbAlpha.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    float data[4] = { alpha, (float)m_width, (float)m_height, 0.f };
    memcpy(mapped.pData, data, sizeof(data));
    ctx->Unmap(m_cbAlpha.Get(), 0);

    // Gather warp compute shader:
    // For each output pixel p: sample frameN at (p - MV[p] * alpha)
    ctx->CSSetShader(m_csWarp.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[2] = { frameN, mvField };
    ctx->CSSetShaderResources(0, 2, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, m_predicted_UAV.GetAddressOf(), nullptr);
    ctx->CSSetConstantBuffers(0, 1, m_cbAlpha.GetAddressOf());
    ctx->CSSetSamplers(0, 1, m_sampler.GetAddressOf());

    ctx->Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);

    // Unbind UAV before using as SRV
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    ID3D11ShaderResourceView*  nullSRV[2] = {};
    ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    ctx->CSSetShaderResources(0, 2, nullSRV);

    return m_predicted_SRV.Get();
}

void Extrapolator::Blit(
    ID3D11DeviceContext* ctx,
    ID3D11RenderTargetView* rtv,
    ID3D11ShaderResourceView* src)
{
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->VSSetShader(m_vsQuad.Get(), nullptr, 0);
    ctx->PSSetShader(m_psBlit.Get(), nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &src);
    ctx->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);  // fullscreen triangle (no vertex buffer needed)

    // Unbind
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
}
