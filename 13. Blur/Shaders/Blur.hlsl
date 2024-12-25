//=============================================================================
// Performs a separable Guassian blur with a blur radius up to 5 pixels.
//=============================================================================

cbuffer cbSettings : register(b0)
{ 
    int gBlurRadius;

    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
};

static const int gMaxBlurRadius = 5;
static const float LIMIT = 1.2f;

Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

#define N 256
#define CacheSize (N + 2*gMaxBlurRadius)
groupshared float4 gCache[CacheSize];

[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
    
    if (groupThreadID.x < gBlurRadius)
    {
        int x = max(dispatchThreadID.x - gBlurRadius, 0);
        gCache[groupThreadID.x] = gInput[int2(x, dispatchThreadID.y)];
    }
    
    if (groupThreadID.x >= N - gBlurRadius)
    {
        int x = min(dispatchThreadID.x + gBlurRadius, gInput.Length.x - 1);
        gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
    }
    
    int x = min(dispatchThreadID.x, gInput.Length.x - 1);
    gCache[groupThreadID.x + gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
    
    GroupMemoryBarrierWithGroupSync();
    
    // 中心点颜色值
    float4 centerColor = gCache[groupThreadID.x + gBlurRadius];
    
    float4 blurColor = centerColor * weights[gBlurRadius];
    float totalWeight = weights[gBlurRadius];
    
    for (int i = -gBlurRadius; i <= gBlurRadius; i++)
    {
        if (i == 0)
            continue;
        
        int j = groupThreadID.x + i + gBlurRadius;
        float4 neighborColor = gCache[j];
        
        if (dot(neighborColor, centerColor) >= LIMIT)
        {
            blurColor += weights[i + gBlurRadius] * gCache[j];
            totalWeight += weights[i + gBlurRadius];
        }
    }
    
    gOutput[dispatchThreadID.xy] = blurColor / totalWeight;
}

[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
    
    if (groupThreadID.y < gBlurRadius)
    {
        int y = max(dispatchThreadID.y - gBlurRadius, 0);
        gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
    }
    
    if (groupThreadID.y >= N - gBlurRadius)
    {
        int y = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y - 1);
        gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
    }
    
    int y = min(dispatchThreadID.y, gInput.Length.y - 1);
    gCache[groupThreadID.y + gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
    
    GroupMemoryBarrierWithGroupSync();
    
    // 中心点颜色值
    float4 centerColor = gCache[groupThreadID.y + gBlurRadius];
    
    float4 blurColor = centerColor * weights[gBlurRadius];
    float totalWeight = weights[gBlurRadius];
    
    for (int i = -gBlurRadius; i <= gBlurRadius; i++)
    {
        if (i == 0)
            continue;
        
        int j = groupThreadID.y + i + gBlurRadius;
        float4 neighborColor = gCache[j];
        
        if (dot(neighborColor, centerColor) >= LIMIT)
        {
            blurColor += weights[i + gBlurRadius] * gCache[j];
            totalWeight += weights[i + gBlurRadius];
        }
    }
    
    gOutput[dispatchThreadID.xy] = blurColor / totalWeight;
}