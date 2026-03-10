// GatherWarp.hlsl
// THE CORE EXTRAPOLATION SHADER
//
// For every output pixel p, we ask:
//   "Where did this pixel come FROM in frame N, given MV and alpha?"
//
// Formula: source_pos = p - MV[p] * alpha
//          I_pred[p]  = BilinearSample(FrameN, source_pos)
//
// This is GATHER warp (read from random input location).
// GT 730 loves reads, hates random writes — so we use gather, not scatter.

Texture2D<float4>   FrameN  : register(t0);   // current frame
Texture2D<float2>   MVField : register(t1);    // motion vectors (pixels/frame)
RWTexture2D<float4> Pred    : register(u0);    // output: predicted frame N+1

SamplerState BilinearSampler : register(s0);

cbuffer AlphaParams : register(b0) {
    float Alpha;    // extrapolation strength (0.5 to 1.0)
    float Width;
    float Height;
    float _pad;
};

[numthreads(8, 8, 1)]
void CSWarp(uint3 id : SV_DispatchThreadID) {
    uint W = (uint)Width;
    uint H = (uint)Height;
    if (id.x >= W || id.y >= H) return;

    // Read motion vector at this pixel
    float2 mv = MVField[id.xy];

    // Extrapolate: where was this pixel in frame N?
    // p - mv*alpha = where to sample in FrameN
    float2 sourcePos = float2(id.xy) - mv * Alpha;

    // Convert to UV [0,1]
    float2 uv = (sourcePos + 0.5f) / float2(W, H);

    // Check bounds — if out of range, fall back to copying frame N directly
    bool valid = (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f);

    float4 color;
    if (valid) {
        // Hardware bilinear — free on GT 730, no manual interpolation needed
        color = FrameN.SampleLevel(BilinearSampler, uv, 0);
    } else {
        // Fallback: just copy from frame N (no warp in occluded areas)
        color = FrameN[id.xy];
    }

    Pred[id.xy] = color;
}
