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

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : Position;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
    return vout;
}

struct PathchTess
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

PathchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
    float3 centerL = (patch[0].PosL + patch[1].PosL + patch[2].PosL) / 3.0f;
    float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;
    float d = distance(gEyePosW, centerW);
    
    const float d1 = 100.0f;
    const float d0 = 20.0f;
    float tess = 63.0f * saturate((d1 - d) / (d1 - d0)) + 1.0f;
    
    PathchTess pt;
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

[outputtopology("triangle_cw")]
[domain("tri")]
[partitioning("fractional_even")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 3> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HullOut ho;
    ho.PosL = p[i].PosL;
    return ho;
}

struct DomainOut
{
    float4 PosH : SV_Position;
};

[domain("tri")]
DomainOut DS(PathchTess pt, float3 uvw : SV_DomainLocation, const OutputPatch<HullOut, 3> quad)
{
    float3 p = uvw.x * quad[0].PosL + uvw.y * quad[1].PosL + uvw.z * quad[2].PosL;
    
    p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));
    
    float4 posW = mul(float4(p, 1.0f), gWorld);
    DomainOut dout;
    dout.PosH = mul(posW, gViewProj);
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
