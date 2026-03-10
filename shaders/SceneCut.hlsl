// SceneCut.hlsl
// Computes mean(|F_N - F_{N-1}|^2) over the frame.
// Uses InterlockedAdd with a global accumulator (1 float on GPU).
// If result > threshold (e.g. 0.12) → scene cut → suppress extrapolation.

Texture2D<float4>     FrameN   : register(t0);
Texture2D<float4>     FrameNm1 : register(t1);
RWByteAddressBuffer   Result   : register(u0);  // 1 float, accumulated via atomic

[numthreads(16, 16, 1)]
void CSReduce(uint3 id : SV_DispatchThreadID) {
    uint W, H;
    FrameN.GetDimensions(W, H);
    if (id.x >= W || id.y >= H) return;

    float3 diff = FrameN[id.xy].rgb - FrameNm1[id.xy].rgb;

    // Luma-weighted squared error (more perceptually accurate than RGB mean)
    float lumaErr = dot(diff * diff, float3(0.299f, 0.587f, 0.114f));

    // Scale to int for atomic add (multiply by 1000 to preserve 3 decimal places)
    uint encoded = (uint)(lumaErr * 1000.0f);

    // Atomic accumulate into result buffer
    Result.InterlockedAdd(0, encoded);
}

// NOTE: CPU divides readback value by (pixelCount * 1000) to get mean energy float
