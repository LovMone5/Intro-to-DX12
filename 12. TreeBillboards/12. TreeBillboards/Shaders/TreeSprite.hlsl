#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;
    
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
}

Texture2DArray gDiffuseMap0 : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct VertexIn
{
    float3 PosL : POSITION;
    float2 Size : SIZE;
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float2 Size : SIZE;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    uint PrimID : SV_PrimitiveID;
};

VertexOut VS(VertexIn vin, uint vertID : SV_VertexID)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.CenterW = posW.xyz;
    
    vout.Size = float2(2 * (vertID + 1), 2 * (vertID + 1));
   
    return vout;
}

[maxvertexcount(4)]
void GS(point VertexOut gin[1],
        inout TriangleStream<GeoOut> triStream,
        uint primID : SV_PrimitiveID)
{
    float3 center = gin[0].CenterW;
    float2 size = gin[0].Size;
    float halfWidth = size.x / 2.0f, halfHeight = size.y / 2.0f;
    
    float3 up = float3(0.f, 1.f, 0.f);
    float3 look = gEyePosW - center;
    // look.y = 0.0f;
    float3 right = normalize(cross(up, look));
    
    float3 tv[4];
    tv[0] = center + halfWidth * right - halfHeight * up;
    tv[1] = center + halfWidth * right + halfHeight * up;
    tv[2] = center - halfWidth * right - halfHeight * up;
    tv[3] = center - halfWidth * right + halfHeight * up;
    
    float2 ttc[4] =
    {
        float2(0.f, 1.f),
        float2(0.f, 0.f),
        float2(1.f, 1.f),
        float2(1.f, 0.f)
    };

    GeoOut gout;
    for (int i = 0; i < 4; i++)
    {
        gout.PosW = tv[i];
        gout.PosH = mul(float4(tv[i], 1.0f), gViewProj);
        gout.NormalW = up;
        gout.TexC = ttc[i];
        gout.PrimID = primID;
        
        triStream.Append(gout);
    }
}

float4 PS(GeoOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    
    float3 uvw = float3(pin.TexC, pin.PrimID);
    float4 mixTex = gDiffuseMap0.Sample(gsamAnisotropicWrap, uvw);
    float4 diffuseAlbedo = gDiffuseAlbedo * mixTex;
    
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.01f);
#endif    
    
    Material mat;
    mat.DiffuseAlbedo = diffuseAlbedo;
    mat.FresnelR0 = gFresnelR0;
    mat.Shininess = 1.0f - gRoughness;
    
    float3 toEye = gEyePosW - pin.PosW;
    float distToEye = length(toEye);
    toEye = toEye / distToEye;
    
    float4 ambient = diffuseAlbedo * gAmbientLight;
    float3 destColor = ambient.xyz + ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEye);
    
#ifdef FOG
    float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
    destColor = lerp(destColor, gFogColor, fogAmount);
#endif
    
    return float4(destColor, diffuseAlbedo.a);
}
