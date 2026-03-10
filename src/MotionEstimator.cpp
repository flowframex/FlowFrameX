#include "MotionEstimator.h"
#include "ShaderCompiler.h"
#include <cmath>

bool MotionEstimator::Init(ID3D11Device* device, int width, int height) {
    m_width  = width;
    m_height = height;

    if (!LoadShaders(device)) return false;

    // Create pyramid textures for each level
    for (int i = 0; i < LEVELS; i++) {
        int w = max(1, width  >> i);
        int h = max(1, height >> i);

        auto makeLevel = [&](ComPtr<ID3D11Texture2D>& tex,
                             ComPtr<ID3D11ShaderResourceView>& srv,
                             ComPtr<ID3D11UnorderedAccessView>& uav,
                             DXGI_FORMAT fmt)
        {
            D3D11_TEXTURE2D_DESC td{};
            td.Width            = w;
            td.Height           = h;
            td.MipLevels        = 1;
            td.ArraySize        = 1;
            td.Format           = fmt;
            td.SampleDesc.Count = 1;
            td.Usage            = D3D11_USAGE_DEFAULT;
            td.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
            device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());
            device->CreateUnorderedAccessView(tex.Get(), nullptr, uav.ReleaseAndGetAddressOf());
        };

        makeLevel(m_pyrN[i],   m_pyrN_SRV[i],   m_pyrN_UAV[i],   DXGI_FORMAT_R8G8B8A8_UNORM);
        makeLevel(m_pyrNm1[i], m_pyrNm1_SRV[i], m_pyrNm1_UAV[i], DXGI_FORMAT_R8G8B8A8_UNORM);
        makeLevel(m_mv[i],     m_mv_SRV[i],     m_mv_UAV[i],     DXGI_FORMAT_R16G16_FLOAT);
    }
    return true;
}

bool MotionEstimator::LoadShaders(ID3D11Device* device) {
    m_csDownsample = ShaderCompiler::LoadCS(device, L"shaders/Downsample.hlsl",  "CSMain");
    m_csMatch      = ShaderCompiler::LoadCS(device, L"shaders/MotionEstimate.hlsl", "CSMatch");
    m_csRefine     = ShaderCompiler::LoadCS(device, L"shaders/MotionEstimate.hlsl", "CSRefine");
    m_csMedian     = ShaderCompiler::LoadCS(device, L"shaders/MotionEstimate.hlsl", "CSMedian");
    return m_csDownsample && m_csMatch && m_csRefine && m_csMedian;
}

ID3D11ShaderResourceView* MotionEstimator::Estimate(
    ID3D11DeviceContext* ctx,
    ID3D11ShaderResourceView* frameN,
    ID3D11ShaderResourceView* frameNm1)
{
    // Level 0 = full res: copy SRVs into level 0 by dispatching downsample with scale=1
    // Then build pyramid levels 1..3 by downsampling
    // (We bind the input SRVs directly for level 0)

    // Downsample both frames into pyramid
    for (int i = 1; i < LEVELS; i++) {
        int w = max(1, m_width  >> i);
        int h = max(1, m_height >> i);

        ctx->CSSetShader(m_csDownsample.Get(), nullptr, 0);

        // Frame N pyramid
        ID3D11ShaderResourceView* src = (i == 1) ? frameN : m_pyrN_SRV[i-1].Get();
        ctx->CSSetShaderResources(0, 1, &src);
        ctx->CSSetUnorderedAccessViews(0, 1, m_pyrN_UAV[i].GetAddressOf(), nullptr);
        ctx->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

        // Frame N-1 pyramid
        ID3D11ShaderResourceView* srcNm1 = (i == 1) ? frameNm1 : m_pyrNm1_SRV[i-1].Get();
        ctx->CSSetShaderResources(0, 1, &srcNm1);
        ctx->CSSetUnorderedAccessViews(0, 1, m_pyrNm1_UAV[i].GetAddressOf(), nullptr);
        ctx->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
    }

    // Motion match at coarsest level (LEVELS-1), search radius ±8 blocks
    {
        int lvl = LEVELS - 1;
        int w = max(1, m_width  >> lvl);
        int h = max(1, m_height >> lvl);

        ctx->CSSetShader(m_csMatch.Get(), nullptr, 0);
        ID3D11ShaderResourceView* srvs[2] = {
            m_pyrN_SRV[lvl].Get(),
            m_pyrNm1_SRV[lvl].Get()
        };
        ctx->CSSetShaderResources(0, 2, srvs);
        ctx->CSSetUnorderedAccessViews(0, 1, m_mv_UAV[lvl].GetAddressOf(), nullptr);
        ctx->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
    }

    // Refine upward: each level uses MV from coarser level × 2 + local search ±2
    for (int i = LEVELS - 2; i >= 0; i--) {
        int w = max(1, m_width  >> i);
        int h = max(1, m_height >> i);

        ctx->CSSetShader(m_csRefine.Get(), nullptr, 0);
        ID3D11ShaderResourceView* srcFrame = (i == 0) ? frameN : m_pyrN_SRV[i].Get();
        ID3D11ShaderResourceView* srcPrev  = (i == 0) ? frameNm1 : m_pyrNm1_SRV[i].Get();
        ID3D11ShaderResourceView* srvs[3] = { srcFrame, srcPrev, m_mv_SRV[i+1].Get() };
        ctx->CSSetShaderResources(0, 3, srvs);
        ctx->CSSetUnorderedAccessViews(0, 1, m_mv_UAV[i].GetAddressOf(), nullptr);
        ctx->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
    }

    // Median filter on final MV field (level 0) to kill outlier vectors
    ctx->CSSetShader(m_csMedian.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 1, m_mv_SRV[0].GetAddressOf());
    ctx->CSSetUnorderedAccessViews(0, 1, m_mv_UAV[0].GetAddressOf(), nullptr);
    ctx->Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);

    // Unbind
    ID3D11ShaderResourceView* nullSRV[3] = {};
    ID3D11UnorderedAccessView* nullUAV[1] = {};
    ctx->CSSetShaderResources(0, 3, nullSRV);
    ctx->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return m_mv_SRV[0].Get();  // full-res MV field
}
