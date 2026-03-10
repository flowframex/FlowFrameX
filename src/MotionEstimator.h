#pragma once
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Pyramid SAD motion estimator.
// Builds 4 mip levels, matches at lowest res, refines upward.
// Output: MV texture (RG16F = float2 velocity in pixels/frame)
class MotionEstimator {
public:
    bool Init(ID3D11Device* device, int width, int height);

    // Returns SRV of the motion vector field (same resolution as input / 4)
    ID3D11ShaderResourceView* Estimate(
        ID3D11DeviceContext* ctx,
        ID3D11ShaderResourceView* frameN,
        ID3D11ShaderResourceView* frameNm1
    );

private:
    bool LoadShaders(ID3D11Device* device);
    void Downsample(ID3D11DeviceContext* ctx, int level);
    void MatchLevel(ID3D11DeviceContext* ctx, int level);
    void RefineLevel(ID3D11DeviceContext* ctx, int level);
    void MedianFilter(ID3D11DeviceContext* ctx);

    static const int LEVELS = 4;

    ComPtr<ID3D11ComputeShader> m_csDownsample;
    ComPtr<ID3D11ComputeShader> m_csMatch;
    ComPtr<ID3D11ComputeShader> m_csRefine;
    ComPtr<ID3D11ComputeShader> m_csMedian;

    // Pyramid textures for frame N and N-1
    ComPtr<ID3D11Texture2D>          m_pyrN[LEVELS];
    ComPtr<ID3D11ShaderResourceView> m_pyrN_SRV[LEVELS];
    ComPtr<ID3D11UnorderedAccessView>m_pyrN_UAV[LEVELS];

    ComPtr<ID3D11Texture2D>          m_pyrNm1[LEVELS];
    ComPtr<ID3D11ShaderResourceView> m_pyrNm1_SRV[LEVELS];
    ComPtr<ID3D11UnorderedAccessView>m_pyrNm1_UAV[LEVELS];

    // MV field at each level (RG16F)
    ComPtr<ID3D11Texture2D>          m_mv[LEVELS];
    ComPtr<ID3D11ShaderResourceView> m_mv_SRV[LEVELS];
    ComPtr<ID3D11UnorderedAccessView>m_mv_UAV[LEVELS];

    int m_width = 0, m_height = 0;
};
