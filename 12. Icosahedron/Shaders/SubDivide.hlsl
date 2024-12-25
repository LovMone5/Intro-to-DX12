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

struct VertexInOut
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexInOut VS(VertexInOut vin)
{
    return vin;
}

VertexInOut MidPoint(VertexInOut a, VertexInOut b)
{
    VertexInOut ret;
    ret.PosL = normalize(0.5f * (a.PosL + b.PosL));
    ret.NormalL = ret.PosL;
    ret.TexC = 0.5f * (a.TexC + b.TexC);
    return ret;
}

GeoOut Solve(VertexInOut x, float3 normal)
{
    x.PosL = x.PosL + normal * gTotalTime * 0.3f;
    
    GeoOut ret = (GeoOut) 0;
    ret.PosW = mul(float4(x.PosL, 1.0f), gWorld).xyz;
    ret.PosH = mul(float4(ret.PosW, 1.0f), gViewProj);
    ret.NormalW = mul(float4(x.NormalL, 1.0f), gWorld).xyz;
        
    float4 texC = mul(float4(x.TexC, 0.0f, 1.0f), gTexTransform);
    ret.TexC = mul(texC, gMatTransform).xy;
    return ret;
}

struct StackEntry
{
    VertexInOut p0, p1, p2;
    int currentLevel;
};
void SubDivide(VertexInOut a, VertexInOut b, VertexInOut c, int level, inout TriangleStream<GeoOut> stream)
{
    StackEntry stack[64] = (StackEntry[64])0;
    int stackIndex = 0;

    StackEntry t = (StackEntry) 0;
    t.currentLevel = level;
    t.p0 = a;
    t.p1 = b;
    t.p2 = c;
    stack[stackIndex++] = t;

    while (stackIndex > 0) 
    {
        StackEntry entry = stack[--stackIndex];

        if (entry.currentLevel == 0) 
        {
            float3 normal = normalize(entry.p0.NormalL + entry.p1.NormalL + entry.p2.NormalL);
            
            GeoOut fa = Solve(entry.p0, normal);
            GeoOut fb = Solve(entry.p1, normal);
            GeoOut fc = Solve(entry.p2, normal);

            stream.Append(fa);
            stream.Append(fb);
            stream.Append(fc);
            stream.RestartStrip();
        } 
        else
        {
            VertexInOut ma = MidPoint(entry.p0, entry.p1);
            VertexInOut mb = MidPoint(entry.p1, entry.p2);
            VertexInOut mc = MidPoint(entry.p0, entry.p2);

            StackEntry t = (StackEntry) 0;
            t.currentLevel = entry.currentLevel - 1;
            
            t.p0 = entry.p0, t.p1 = ma, t.p2 = mc;
            stack[stackIndex++] = t;
            t.p0 = ma, t.p1 = mb, t.p2 = mc;
            stack[stackIndex++] = t;
            t.p0 = ma, t.p1 = entry.p1, t.p2 = mb;
            stack[stackIndex++] = t;
            t.p0 = mc, t.p1 = mb, t.p2 = entry.p2;
            stack[stackIndex++] = t;
        }
    }
}


[maxvertexcount(48)]
void GS(triangle VertexInOut gin[3], inout TriangleStream<GeoOut> triStream)
{
    float3 posW = mul(float4(gin[0].PosL, 1.0f), gWorld).xyz;
    float dist = length(gEyePosW - posW);
    int k;
    if (dist < 15)
        k = 2;
    else if (dist <= 30)
        k = 1;
    else
        k = 0;
    SubDivide(gin[0], gin[1], gin[2], k, triStream);
}

float4 PS(GeoOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    
    float4 diffuseAlbedo = gDiffuseAlbedo;
    
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


[maxvertexcount(2)]
void GSPoint(point VertexInOut gin[1], inout LineStream<GeoOut> pstream)
{
    const float len = 0.5f;
    
    VertexInOut t[2];
    t[0] = gin[0];
    t[1] = gin[0];
    
    t[1].PosL = gin[0].PosL + gin[0].NormalL * len;
    
    GeoOut gout[2];
    
    for (int i = 0; i < 2; i++)
    {
        gout[i].PosW = mul(float4(t[i].PosL, 1.0f), gWorld).xyz;
        gout[i].PosH = mul(float4(gout[i].PosW, 1.0f), gViewProj);
        gout[i].NormalW = mul(float4(t[i].NormalL, 1.0f), gWorld).xyz;
        
        float4 texC = mul(float4(t[i].TexC, 0.0f, 1.0f), gTexTransform);
        gout[i].TexC = mul(texC, gMatTransform).xy;
        pstream.Append(gout[i]);
    }
}

[maxvertexcount(2)]
void GSPlane(triangle VertexInOut gin[3], inout LineStream<GeoOut> pstream)
{
    const float len = 0.5f;
    
    VertexInOut t[2];
    t[0] = gin[0];
    t[0].PosL = (gin[0].PosL + gin[1].PosL + gin[2].PosL) / 3.0f;
    t[0].NormalL = normalize(gin[0].NormalL + gin[1].NormalL + gin[2].NormalL);
    t[1] = t[0];
    t[1].PosL = t[0].PosL + t[0].NormalL * len;
    
    GeoOut gout[2];
    
    for (int i = 0; i < 2; i++)
    {
        gout[i].PosW = mul(float4(t[i].PosL, 1.0f), gWorld).xyz;
        gout[i].PosH = mul(float4(gout[i].PosW, 1.0f), gViewProj);
        gout[i].NormalW = mul(float4(t[i].NormalL, 1.0f), gWorld).xyz;
        
        float4 texC = mul(float4(t[i].TexC, 0.0f, 1.0f), gTexTransform);
        gout[i].TexC = mul(texC, gMatTransform).xy;
        
        pstream.Append(gout[i]);
    }
}
