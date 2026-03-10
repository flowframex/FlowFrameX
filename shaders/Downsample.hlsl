// Downsample.hlsl
// Bilinear 2x downsample — builds pyramid levels for motion estimation
// GT 730 friendly: uses hardware bilinear sampler (free on any DX11 GPU)

Texture2D<float4>   SrcTex : register(t0);
RWTexture2D<float4> DstTex : register(u0);

SamplerState BilinearSampler : register(s0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    uint dstW, dstH;
    DstTex.GetDimensions(dstW, dstH);

    if (id.x >= dstW || id.y >= dstH) return;

    // Sample source at 2x the destination coordinate (center of 2x2 block)
    float2 uv = (float2(id.xy) + 0.5f) / float2(dstW, dstH);

    // 4-tap box filter (better than single bilinear for proper downsampling)
    float2 texelSrc;
    uint srcW, srcH;
    SrcTex.GetDimensions(srcW, srcH);
    texelSrc = float2(1.0f / srcW, 1.0f / srcH);

    float4 c =
        SrcTex.SampleLevel(BilinearSampler, uv + float2(-0.25f, -0.25f) * texelSrc * 2.0f, 0) +
        SrcTex.SampleLevel(BilinearSampler, uv + float2( 0.25f, -0.25f) * texelSrc * 2.0f, 0) +
        SrcTex.SampleLevel(BilinearSampler, uv + float2(-0.25f,  0.25f) * texelSrc * 2.0f, 0) +
        SrcTex.SampleLevel(BilinearSampler, uv + float2( 0.25f,  0.25f) * texelSrc * 2.0f, 0);

    DstTex[id.xy] = c * 0.25f;
}
