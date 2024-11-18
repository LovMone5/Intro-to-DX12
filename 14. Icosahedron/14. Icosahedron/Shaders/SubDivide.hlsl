#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 0
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

struct VertexInOut
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexInOut VS(VertexInOut vin)
{
    return vin;
}

struct PatchTess
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexInOut, 3> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    float3 centerL = (patch[0].PosL + patch[1].PosL + patch[2].PosL) / 3.0f;
    float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;
    float d = distance(centerW, gEyePosW);
    
    const float d0 = 5.0f, d1 = 30.0f;
    float tess = 4.0f * saturate((d1 - d) / (d1 - d0)) + 1.0f;
    
    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
    pt.InsideTess = tess;
    
    return pt;
}

struct HullOut
{
    float3 PosL : POSITION;
};

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[patchconstantfunc("ConstantHS")]
[outputcontrolpoints(3)]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexInOut, 3> patch, uint i : SV_OutputControlPointID)
{
    HullOut ho;
    ho.PosL = patch[i].PosL;
    return ho;
}

struct DomainOut
{
    float4 PosH : SV_Position;
};

[domain("tri")]
DomainOut DS(PatchTess pt, float3 uvw : SV_DomainLocation, const OutputPatch<HullOut, 3> tri)
{
    DomainOut dout;
    
    float3 posL = uvw.x * tri[0].PosL + uvw.y * tri[1].PosL + uvw.z * tri[2].PosL;
    posL = normalize(posL);
    float4 posW = mul(float4(posL, 1.0f), gWorld);
    dout.PosH = mul(posW, gViewProj);
    
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
