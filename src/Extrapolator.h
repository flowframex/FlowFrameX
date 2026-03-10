#pragma once
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class Extrapolator {
public:
    bool Init(ID3D11Device* device, int width, int height);

    // Gather-warp frameN forward by (MV * alpha) → returns SRV of predicted frame
    ID3D11ShaderResourceView* Extrapolate(
        ID3D11DeviceContext* ctx,
        ID3D11ShaderResourceView* frameN,
        ID3D11ShaderResourceView* mvField,
        float alpha
    );

    // Blit predicted frame to the render target (full-screen quad)
    void Blit(
        ID3D11DeviceContext* ctx,
        ID3D11RenderTargetView* rtv,
        ID3D11ShaderResourceView* src
    );

private:
    ComPtr<ID3D11ComputeShader>  m_csWarp;
    ComPtr<ID3D11VertexShader>   m_vsQuad;
    ComPtr<ID3D11PixelShader>    m_psBlit;
    ComPtr<ID3D11SamplerState>   m_sampler;
    ComPtr<ID3D11Buffer>         m_cbAlpha;  // constant buffer for alpha

    ComPtr<ID3D11Texture2D>           m_predicted;
    ComPtr<ID3D11ShaderResourceView>  m_predicted_SRV;
    ComPtr<ID3D11UnorderedAccessView> m_predicted_UAV;

    int m_width = 0, m_height = 0;
};
