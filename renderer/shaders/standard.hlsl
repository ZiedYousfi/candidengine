/**
 * @file standard.hlsl
 * @brief Standard PBR shader for Candid Engine
 *
 * This shader implements physically-based rendering with metallic-roughness workflow.
 * It can be compiled to:
 *   - SPIR-V for Vulkan (using DXC)
 *   - Metal Shading Language for Apple platforms (via SPIRV-Cross)
 *   - DXIL for Direct3D 12 (using DXC)
 *
 * Compilation examples:
 *   dxc -T vs_6_0 -E VSMain -Fo standard_vs.spv -spirv standard.hlsl
 *   dxc -T ps_6_0 -E PSMain -Fo standard_ps.spv -spirv standard.hlsl
 *   spirv-cross standard_vs.spv --msl --output standard_vs.metal
 */

//=============================================================================
// Constant Buffers
//=============================================================================

cbuffer PerFrame : register(b0) {
    float4x4 ViewProjection;
    float3 CameraPosition;
    float Time;
    float3 LightDirection;
    float LightIntensity;
    float4 LightColor;
    float4 AmbientColor;
};

cbuffer PerObject : register(b1) {
    float4x4 Model;
    float4x4 NormalMatrix;  // Inverse transpose of Model
};

cbuffer PerMaterial : register(b2) {
    float4 BaseColorFactor;
    float MetallicFactor;
    float RoughnessFactor;
    float NormalScale;
    float OcclusionStrength;
    float3 EmissiveFactor;
    float AlphaCutoff;
};

//=============================================================================
// Textures and Samplers
//=============================================================================

Texture2D<float4> BaseColorTexture : register(t0);
Texture2D<float3> NormalTexture : register(t1);
Texture2D<float2> MetallicRoughnessTexture : register(t2);  // G = Roughness, B = Metallic
Texture2D<float> OcclusionTexture : register(t3);
Texture2D<float3> EmissiveTexture : register(t4);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearClampSampler : register(s1);

//=============================================================================
// Vertex Shader
//=============================================================================

struct VSInput {
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Tangent  : TANGENT;      // w = handedness
    float2 TexCoord0 : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
    float4 Color    : COLOR0;
};

struct VSOutput {
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldTangent : TEXCOORD2;
    float3 WorldBitangent : TEXCOORD3;
    float2 TexCoord0 : TEXCOORD4;
    float2 TexCoord1 : TEXCOORD5;
    float4 Color : COLOR0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;

    // Transform position
    float4 worldPosition = mul(Model, float4(input.Position, 1.0));
    output.Position = mul(ViewProjection, worldPosition);
    output.WorldPosition = worldPosition.xyz;

    // Transform normal (using normal matrix for non-uniform scaling)
    output.WorldNormal = normalize(mul((float3x3)NormalMatrix, input.Normal));

    // Transform tangent and compute bitangent
    output.WorldTangent = normalize(mul((float3x3)Model, input.Tangent.xyz));
    output.WorldBitangent = cross(output.WorldNormal, output.WorldTangent) * input.Tangent.w;

    // Pass through texture coordinates and color
    output.TexCoord0 = input.TexCoord0;
    output.TexCoord1 = input.TexCoord1;
    output.Color = input.Color;

    return output;
}

//=============================================================================
// PBR Functions
//=============================================================================

static const float PI = 3.14159265359;
static const float EPSILON = 0.00001;

// Schlick's Fresnel approximation
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, EPSILON);
}

// Schlick-GGX geometry function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, EPSILON);
}

// Smith's geometry function
float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

//=============================================================================
// Pixel Shader
//=============================================================================

float4 PSMain(VSOutput input) : SV_Target {
    // Sample textures
    float4 baseColor = BaseColorTexture.Sample(LinearWrapSampler, input.TexCoord0) * BaseColorFactor * input.Color;
    float2 metallicRoughness = MetallicRoughnessTexture.Sample(LinearWrapSampler, input.TexCoord0);
    float metallic = metallicRoughness.b * MetallicFactor;
    float roughness = metallicRoughness.g * RoughnessFactor;
    float ao = OcclusionTexture.Sample(LinearWrapSampler, input.TexCoord0) * OcclusionStrength;
    float3 emissive = EmissiveTexture.Sample(LinearWrapSampler, input.TexCoord0) * EmissiveFactor;

    // Normal mapping
    float3 normalSample = NormalTexture.Sample(LinearWrapSampler, input.TexCoord0);
    normalSample = normalSample * 2.0 - 1.0;
    normalSample.xy *= NormalScale;

    float3x3 TBN = float3x3(
        normalize(input.WorldTangent),
        normalize(input.WorldBitangent),
        normalize(input.WorldNormal)
    );
    float3 N = normalize(mul(normalSample, TBN));

    // View direction
    float3 V = normalize(CameraPosition - input.WorldPosition);

    // Light direction (assuming directional light)
    float3 L = normalize(-LightDirection);
    float3 H = normalize(V + L);

    // Dot products
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // F0 - reflectance at normal incidence
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(HdotV, F0);

    // Specular term
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    float3 specular = numerator / denominator;

    // Energy conservation
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    // Diffuse term (Lambertian)
    float3 diffuse = kD * baseColor.rgb / PI;

    // Final radiance
    float3 Lo = (diffuse + specular) * LightColor.rgb * LightIntensity * NdotL;

    // Ambient term (simplified IBL approximation)
    float3 ambient = AmbientColor.rgb * baseColor.rgb * ao;

    // Final color
    float3 color = ambient + Lo + emissive;

    // Tone mapping (simple Reinhard)
    color = color / (color + 1.0);

    // Gamma correction
    color = pow(color, 1.0 / 2.2);

    return float4(color, baseColor.a);
}

//=============================================================================
// Unlit Shader Variants
//=============================================================================

float4 PSUnlit(VSOutput input) : SV_Target {
    float4 color = BaseColorTexture.Sample(LinearWrapSampler, input.TexCoord0) * BaseColorFactor * input.Color;
    return color;
}

//=============================================================================
// Shadow Pass
//=============================================================================

struct ShadowVSOutput {
    float4 Position : SV_Position;
};

ShadowVSOutput VSShadow(VSInput input) {
    ShadowVSOutput output;
    float4 worldPosition = mul(Model, float4(input.Position, 1.0));
    output.Position = mul(ViewProjection, worldPosition);
    return output;
}

void PSShadow(ShadowVSOutput input) {
    // Shadow map generation - no color output needed
}
