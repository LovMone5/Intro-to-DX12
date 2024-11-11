Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(16, 16, 1)]
void SobelCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float4 c[3][3];
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) 
            c[i + 1][j + 1] = gInput[int2(dispatchThreadID.x + i, dispatchThreadID.y + j)];
    
    float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] +
        1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
    float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][2] +
        1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];
    
    float4 mag = sqrt(Gx * Gx + Gy * Gy);
    
    mag = 1.0 - saturate(CalcLuminance(mag.rgb));
    
    gOutput[dispatchThreadID.xy] = mag;
}
