#pragma once
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Computes mean frame-diff energy E = mean(|F_N - F_{N-1}|^2)
// If E > threshold → scene cut detected → suppress extrapolation
class SceneCut {
public:
    bool  Init(ID3D11Device* device, int width, int height);
    float Compute(ID3D11DeviceContext* ctx,
                  ID3D11ShaderResourceView* frameN,
                  ID3D11ShaderResourceView* frameNm1);
private:
    ComPtr<ID3D11ComputeShader>   m_csReduce;
    ComPtr<ID3D11Buffer>          m_resultBuf;       // GPU UAV buffer (1 float)
    ComPtr<ID3D11Buffer>          m_resultBufCPU;    // staging for CPU readback
    ComPtr<ID3D11UnorderedAccessView> m_resultUAV;

    int m_width = 0, m_height = 0;
};
