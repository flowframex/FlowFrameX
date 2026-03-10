#include "ShaderCompiler.h"
#include <d3dcompiler.h>
#include <cstdio>
#pragma comment(lib, "d3dcompiler.lib")

static ComPtr<ID3DBlob> CompileFile(const wchar_t* path, const char* entry, const char* target) {
    ComPtr<ID3DBlob> code, errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompileFromFile(path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entry, target, flags, 0,
                                    code.ReleaseAndGetAddressOf(),
                                    errors.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        if (errors) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Shader compile error [%ls::%s]:\n%s",
                     path, entry, (char*)errors->GetBufferPointer());
            OutputDebugStringA(msg);
            MessageBoxA(nullptr, msg, "FlowFrameX Shader Error", MB_ICONERROR);
        }
        return nullptr;
    }
    return code;
}

ComPtr<ID3D11ComputeShader> ShaderCompiler::LoadCS(ID3D11Device* device, const wchar_t* path, const char* entry) {
    auto blob = CompileFile(path, entry, "cs_5_0");
    if (!blob) return nullptr;
    ComPtr<ID3D11ComputeShader> cs;
    device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, cs.ReleaseAndGetAddressOf());
    return cs;
}

ComPtr<ID3D11VertexShader> ShaderCompiler::LoadVS(ID3D11Device* device, const wchar_t* path, const char* entry) {
    auto blob = CompileFile(path, entry, "vs_5_0");
    if (!blob) return nullptr;
    ComPtr<ID3D11VertexShader> vs;
    device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, vs.ReleaseAndGetAddressOf());
    return vs;
}

ComPtr<ID3D11PixelShader> ShaderCompiler::LoadPS(ID3D11Device* device, const wchar_t* path, const char* entry) {
    auto blob = CompileFile(path, entry, "ps_5_0");
    if (!blob) return nullptr;
    ComPtr<ID3D11PixelShader> ps;
    device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps.ReleaseAndGetAddressOf());
    return ps;
}
