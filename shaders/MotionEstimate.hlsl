// MotionEstimate.hlsl
// Three entry points:
//   CSMatch  — coarse SAD block match at lowest pyramid level
//   CSRefine — refine MV upward from coarser level
//   CSMedian — 3x3 median filter to kill outlier vectors

Texture2D<float4>   FrameN   : register(t0);
Texture2D<float4>   FrameNm1 : register(t1);
Texture2D<float2>   MVCoarse : register(t2);  // only used in CSRefine

RWTexture2D<float2> MVOut    : register(u0);

// ── Utility ──────────────────────────────────────────────────────────────────

float SAD8x8(Texture2D<float4> A, Texture2D<float4> B,
             int2 posA, int2 posB, int2 sizeA)
{
    float s = 0.0f;
    [unroll]
    for (int dy = 0; dy < 8; dy++) {
        [unroll]
        for (int dx = 0; dx < 8; dx++) {
            int2 pa = clamp(posA + int2(dx, dy), int2(0,0), sizeA - 1);
            int2 pb = clamp(posB + int2(dx, dy), int2(0,0), sizeA - 1);
            float3 diff = A[pa].rgb - B[pb].rgb;
            s += dot(abs(diff), float3(0.299f, 0.587f, 0.114f)); // luma-weighted SAD
        }
    }
    return s;
}

// ── Entry 1: Coarse block match ───────────────────────────────────────────────
// Search radius ±8 blocks at lowest pyramid level

[numthreads(8, 8, 1)]
void CSMatch(uint3 id : SV_DispatchThreadID) {
    int2 size;
    {
        uint w, h;
        FrameN.GetDimensions(w, h);
        size = int2(w, h);
    }
    if (any(id.xy >= (uint2)size)) return;

    int2 pos = id.xy * 8;  // block origin in pixels

    float bestSAD = 1e9f;
    int2  bestMV  = int2(0, 0);

    // Search ±8 blocks (= ±64 pixels at full res, scaled down here)
    for (int dy = -8; dy <= 8; dy++) {
        for (int dx = -8; dx <= 8; dx++) {
            int2 candidate = pos + int2(dx, dy) * 8;
            float s = SAD8x8(FrameN, FrameNm1, pos, candidate, size);
            if (s < bestSAD) {
                bestSAD = s;
                bestMV  = int2(dx, dy) * 8;
            }
        }
    }

    MVOut[id.xy] = float2(bestMV);
}

// ── Entry 2: Refine from coarser level ───────────────────────────────────────
// MVCoarse is 2x smaller; we upsample it and do ±2 pixel local search

[numthreads(8, 8, 1)]
void CSRefine(uint3 id : SV_DispatchThreadID) {
    int2 size;
    {
        uint w, h;
        FrameN.GetDimensions(w, h);
        size = int2(w, h);
    }
    if (any(id.xy >= (uint2)size)) return;

    // Upsample MV from coarser level (factor 2)
    int2  coarseCoord = id.xy / 2;
    float2 baseMV     = MVCoarse[coarseCoord] * 2.0f;

    float bestSAD = 1e9f;
    float2 bestMV = baseMV;

    // Local ±2 pixel refinement around the coarse MV
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            float2 mv = baseMV + float2(dx, dy);
            int2 candidatePos = id.xy + int2(mv);
            float s = SAD8x8(FrameN, FrameNm1, id.xy, candidatePos, size);
            if (s < bestSAD) {
                bestSAD = s;
                bestMV  = mv;
            }
        }
    }

    MVOut[id.xy] = bestMV;
}

// ── Entry 3: 3x3 Median filter on MV field ───────────────────────────────────
// Sorts 9 MV magnitudes, picks median — kills outlier vectors cleanly

[numthreads(8, 8, 1)]
void CSMedian(uint3 id : SV_DispatchThreadID) {
    int2 size;
    {
        uint w, h;
        FrameN.GetDimensions(w, h);
        size = int2(w, h);
    }
    if (any(id.xy >= (uint2)size)) return;

    float2 samples[9];
    float  mags[9];
    int k = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int2 coord = clamp(int2(id.xy) + int2(dx, dy), int2(0,0), size - 1);
            samples[k] = FrameN[coord].rg;  // reading from MV texture bound as t0
            mags[k]    = dot(samples[k], samples[k]);
            k++;
        }
    }

    // Bubble sort mags to find median (only need partial sort for median at index 4)
    for (int i = 0; i < 5; i++) {
        for (int j = i+1; j < 9; j++) {
            if (mags[j] < mags[i]) {
                float  tmp  = mags[i];    mags[i]    = mags[j];    mags[j]    = tmp;
                float2 tmp2 = samples[i]; samples[i] = samples[j]; samples[j] = tmp2;
            }
        }
    }

    MVOut[id.xy] = samples[4];  // median at index 4
}
