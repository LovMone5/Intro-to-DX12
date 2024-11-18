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
    
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
}

float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;
    return float4(
        invT * invT * invT,
        3.0f * t * invT * invT,
        3.0f * t * t * invT,
        t * t * t);
}

float4 dBernsteinBasis(float t)
{
    float invT = 1.0f - t;
    return float4(
        -3.0f * invT * invT,
        3.0f * invT * invT - 6.0f * t * invT,
        6.0f * t * invT - 3.0f * t * t,
        3.0f * t * t);
}

struct HullOut
{
    float3 PosL : POSITION;
};

float3 CubicBezierSum(const OutputPatch<HullOut, 16> bezpatch, float4 basisU, float4 basisV)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    sum = basisV.x * (basisU.x * bezpatch[0].PosL + basisU.y * bezpatch[1].PosL + basisU.z * bezpatch[2].PosL + basisU.w * bezpatch[3].PosL);
    sum += basisV.y * (basisU.x * bezpatch[4].PosL + basisU.y * bezpatch[5].PosL + basisU.z * bezpatch[6].PosL + basisU.w * bezpatch[7].PosL);
    sum += basisV.z * (basisU.x * bezpatch[8].PosL + basisU.y * bezpatch[9].PosL + basisU.z * bezpatch[10].PosL + basisU.w * bezpatch[11].PosL);
    sum += basisV.w * (basisU.x * bezpatch[12].PosL + basisU.y * bezpatch[13].PosL + basisU.z * bezpatch[14].PosL + basisU.w * bezpatch[15].PosL);

    return sum;
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
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

PathchTess ConstantHS(InputPatch<VertexOut, 16> patch, uint patchID : SV_PrimitiveID)
{
    const float tess = 64;
    PathchTess pt;
    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
    pt.EdgeTess[3] = tess;
    pt.InsideTess[0] = tess;
    pt.InsideTess[1] = tess;
    return pt;
}

[outputtopology("triangle_cw")]
[domain("quad")]
[partitioning("integer")]
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 16> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HullOut ho;
    ho.PosL = p[i].PosL;
    return ho;
}

struct DomainOut
{
    float4 PosH : SV_Position;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

[domain("quad")]
DomainOut DS(PathchTess pt, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 16> quad)
{
    float4 u = BernsteinBasis(uv.x);
    float4 v = BernsteinBasis(uv.y);
    float3 posL = CubicBezierSum(quad, u, v);
    float4 posW = mul(float4(posL, 1.0f), gWorld);
    
    float4 du = dBernsteinBasis(uv.x);
    float4 dv = dBernsteinBasis(uv.y);
    float3 dpdu = CubicBezierSum(quad, du, v);
    float3 dpdv = CubicBezierSum(quad, u, dv);
    float3 normalL = normalize(cross(dpdu, dpdv));

    DomainOut dout;
    dout.PosH = mul(posW, gViewProj);
    dout.NormalW = mul(normalL, (float3x3)gWorld);
    dout.PosW = posW.xyz;
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    
    Material mat;
    mat.DiffuseAlbedo = gDiffuseAlbedo;
    mat.FresnelR0 = gFresnelR0;
    mat.Shininess = 1.0f - gRoughness;
    
    float3 toEye = gEyePosW - pin.PosW;
    toEye = normalize(toEye);
    
    float4 ambient = gDiffuseAlbedo * gAmbientLight;
    float3 color = ambient.xyz + ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEye);

    return float4(color, gDiffuseAlbedo.a);
}
