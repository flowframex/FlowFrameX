#pragma once
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Helper to load and compile HLSL shaders from files at runtime
class ShaderCompiler {
public:
    static ComPtr<ID3D11ComputeShader> LoadCS(ID3D11Device* device, const wchar_t* path, const char* entry);
    static ComPtr<ID3D11VertexShader>  LoadVS(ID3D11Device* device, const wchar_t* path, const char* entry);
    static ComPtr<ID3D11PixelShader>   LoadPS(ID3D11Device* device, const wchar_t* path, const char* entry);
};
