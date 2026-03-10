// Blit.hlsl
// Fullscreen triangle — no vertex buffer needed.
// Blits predicted texture to backbuffer with RCAS sharpening pass.

Texture2D<float4> SrcTex    : register(t0);
SamplerState      Bilinear  : register(s0);

// ── Vertex Shader ────────────────────────────────────────────────────────────
// Generates a single fullscreen triangle from vertex ID (no VB required)

struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID) {
    // Triangle covers [-1,3] in NDC space — clips to fullscreen quad
    float2 ndc = float2(
        (vid == 1) ? 3.0f : -1.0f,
        (vid == 2) ? 3.0f : -1.0f
    );
    VSOut o;
    o.pos = float4(ndc, 0.0f, 1.0f);
    o.uv  = float2(ndc.x, -ndc.y) * 0.5f + 0.5f;
    return o;
}

// ── Pixel Shader ─────────────────────────────────────────────────────────────
// RCAS (Robust Contrast Adaptive Sharpening) — same math as AMD FSR 1.0 pass 2
// λ = 0.3 is safe for GT 730. Increase up to 0.5 for more sharpness.

static const float RCAS_LAMBDA = 0.3f;

float4 PSMain(VSOut i) : SV_Target {
    float2 texelSize;
    {
        uint w, h;
        SrcTex.GetDimensions(w, h);
        texelSize = 1.0f / float2(w, h);
    }

    // Sample center + 4 neighbors
    float4 c  = SrcTex.Sample(Bilinear, i.uv);
    float4 n  = SrcTex.Sample(Bilinear, i.uv + float2( 0, -1) * texelSize);
    float4 s  = SrcTex.Sample(Bilinear, i.uv + float2( 0,  1) * texelSize);
    float4 e  = SrcTex.Sample(Bilinear, i.uv + float2( 1,  0) * texelSize);
    float4 w2 = SrcTex.Sample(Bilinear, i.uv + float2(-1,  0) * texelSize);

    // Luma of neighbors
    float lumaC  = dot(c.rgb,  float3(0.299f, 0.587f, 0.114f));
    float lumaN  = dot(n.rgb,  float3(0.299f, 0.587f, 0.114f));
    float lumaS  = dot(s.rgb,  float3(0.299f, 0.587f, 0.114f));
    float lumaE  = dot(e.rgb,  float3(0.299f, 0.587f, 0.114f));
    float lumaW  = dot(w2.rgb, float3(0.299f, 0.587f, 0.114f));

    // Sharpening weight: negative laplacian scaled by lambda
    float minLuma = min(min(min(lumaC, lumaN), min(lumaS, lumaE)), lumaW);
    float maxLuma = max(max(max(lumaC, lumaN), max(lumaS, lumaE)), lumaW);

    float sharpWeight = -RCAS_LAMBDA / max(maxLuma - minLuma, 1e-4f);
    sharpWeight = clamp(sharpWeight, -0.5f, 0.0f);

    // Apply: sharpened = (C - w*(N+S+E+W)) / (1 - 4*w)
    float4 sharpened = (c - sharpWeight * (n + s + e + w2)) / (1.0f - 4.0f * sharpWeight);
    return saturate(sharpened);
}
