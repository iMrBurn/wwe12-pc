#include "copy_common.hlsli"

Texture2D<float4> g_Texture2DDescriptorHeap[] : register(t0, space0);
SamplerState     g_SamplerDescriptorHeap[]   : register(s0, space3);

// Sharpening of an enlarged picture (1.3, svr_upscale_sharpness; idea of the WWE 2K14 recomp's 720p
// mode, contrast-adaptive sharpening as AMD's FidelityFX CAS describes it, MIT: written here, not
// copied). ResourceDescriptorIndex2 carries the strength 0..100 (0 = the plain filtered picture): the
// present pass sets it only when the picture is drawn larger than the scene was rendered.
float3 Fetch(uint index, float2 uv)
{
    return max(g_Texture2DDescriptorHeap[index].Sample(g_SamplerDescriptorHeap[0], uv).rgb, 0.0);
}

float3 Sharpened(uint index, float2 uv, float strength)
{
    float width, height;
    g_Texture2DDescriptorHeap[index].GetDimensions(width, height);
    const float2 texel = float2(1.0 / width, 1.0 / height);
    const float3 c = Fetch(index, uv);
    const float3 n = Fetch(index, uv + float2(0.0, -texel.y));
    const float3 s = Fetch(index, uv + float2(0.0, texel.y));
    const float3 w = Fetch(index, uv + float2(-texel.x, 0.0));
    const float3 e = Fetch(index, uv + float2(texel.x, 0.0));
    const float3 mn = min(c, min(min(n, s), min(w, e)));
    const float3 mx = max(c, max(max(n, s), max(w, e)));
    // Less sharpening where the neighbourhood already has strong contrast (no halos), more on soft
    // detail; the negative lobe goes from -1/8 (weak) to -1/5 (strong).
    const float3 amount = sqrt(saturate(min(mn, 1.0 - mx) / max(mx, 1e-4)));
    const float3 weight = amount * (-1.0 / lerp(8.0, 5.0, strength));
    return saturate((c + (n + s + w + e) * weight) / (1.0 + 4.0 * weight));
}

float4 main(in float4 position : SV_Position, in float2 texCoord : TEXCOORD) : SV_Target
{
    float4 sampled = g_Texture2DDescriptorHeap[g_PushConstants.ResourceDescriptorIndex]
                       .Sample(g_SamplerDescriptorHeap[0], texCoord);
    const uint sharpness = g_PushConstants.ResourceDescriptorIndex2;
    if (sharpness != 0)
        sampled.rgb = Sharpened(g_PushConstants.ResourceDescriptorIndex, texCoord,
                                min(sharpness, 100u) / 100.0);
    // max() guards pow() against NaN on negative HDR values (source RT may be R16F).
    float3 ramp = pow(max(sampled.rgb, 0.0), g_PushConstants.Param0);
    // X360 scanout: the system reshapes the app ramp for the display before upload
    // (D3D::GammaRampCorrected 0x8246F538): for display-gamma type 2 (TV) the
    // hardware LUT is Rec709_encode(sRGB_decode(app_ramp)). Xenia measurably
    // displays the UNCORRECTED front buffer (its DC LUT stays linear), so
    // Param1 (display correction strength) blends between the two references.
    float3 lin = lerp(ramp / 12.92, pow((ramp + 0.055) / 1.055, 2.4),
                      step(0.04045, ramp));
    float3 corrected = lerp(lin * 4.5, 1.099 * pow(lin, 0.45) - 0.099,
                            step(0.018, lin));
    sampled.rgb = lerp(ramp, corrected, g_PushConstants.Param1);
    return float4(sampled.rgb, 1.0);
}
