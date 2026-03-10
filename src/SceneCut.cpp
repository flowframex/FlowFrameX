#include "SceneCut.h"
#include "ShaderCompiler.h"

bool SceneCut::Init(ID3D11Device* device, int width, int height) {
    m_width  = width;
    m_height = height;

    m_csReduce = ShaderCompiler::LoadCS(device, L"shaders/SceneCut.hlsl", "CSReduce");
    if (!m_csReduce) return false;

    // Structured buffer (1 float) for GPU to write energy sum into
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth           = sizeof(float);
    bd.Usage               = D3D11_USAGE_DEFAULT;
    bd.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
    bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    device->CreateBuffer(&bd, nullptr, m_resultBuf.ReleaseAndGetAddressOf());

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
    uavd.Format              = DXGI_FORMAT_R32_TYPELESS;
    uavd.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
    uavd.Buffer.NumElements  = 1;
    uavd.Buffer.Flags        = D3D11_BUFFER_UAV_FLAG_RAW;
    device->CreateUnorderedAccessView(m_resultBuf.Get(), &uavd, m_resultUAV.ReleaseAndGetAddressOf());

    // CPU-readable staging buffer
    bd.Usage          = D3D11_USAGE_STAGING;
    bd.BindFlags      = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bd.MiscFlags      = 0;
    device->CreateBuffer(&bd, nullptr, m_resultBufCPU.ReleaseAndGetAddressOf());

    return true;
}

float SceneCut::Compute(
    ID3D11DeviceContext* ctx,
    ID3D11ShaderResourceView* frameN,
    ID3D11ShaderResourceView* frameNm1)
{
    // Clear result to 0
    UINT clearVal[4] = { 0, 0, 0, 0 };
    ctx->ClearUnorderedAccessViewUint(m_resultUAV.Get(), clearVal);

    // Dispatch reduction shader: accumulates |F_N - F_{N-1}|^2 per pixel into result
    ctx->CSSetShader(m_csReduce.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvs[2] = { frameN, frameNm1 };
    ctx->CSSetShaderResources(0, 2, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, m_resultUAV.GetAddressOf(), nullptr);
    ctx->Dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

    // Unbind
    ID3D11ShaderResourceView* nullSRV[2] = {};
    ID3D11UnorderedAccessView* nullUAV   = nullptr;
    ctx->CSSetShaderResources(0, 2, nullSRV);
    ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    // GPU→CPU readback (1 float — tiny, not a perf issue)
    ctx->CopyResource(m_resultBufCPU.Get(), m_resultBuf.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    float energy = 0.f;
    if (SUCCEEDED(ctx->Map(m_resultBufCPU.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        float sum = *reinterpret_cast<float*>(mapped.pData);
        energy = sum / float(m_width * m_height);  // normalize to mean
        ctx->Unmap(m_resultBufCPU.Get(), 0);
    }
    return energy;
}
