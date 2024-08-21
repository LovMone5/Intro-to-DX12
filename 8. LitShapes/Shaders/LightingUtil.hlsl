#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float CalcAttenuation(float start, float end, float d)
{
    return saturate((end - d) / (end - start));
}

float3 SchlickFresnel(float3 light, float3 normal, float3 R0)
{
    float cos = saturate(dot(light, normal));

    return R0 + (1.0f - R0) * pow(1.0f - cos, 5.0f);
}

float3 BlinnPhong(float3 strength, float3 light, float3 normal, float3 toEye, Material mat)
{
    float3 h = normalize(toEye + light);

    float3 diffuse = mat.DiffuseAlbedo.rgb;
    const float m = mat.Shininess * 256.0f;
    float3 specular = SchlickFresnel(light, h, mat.FresnelR0) * (m + 8.0f) * pow(max(dot(normal, h), 0.0f), m) / 8.0f;

    // specular = specular / (specular + 1.0f);
    
    return strength * (diffuse + specular);
}

float3 ComputeDirectionalLight(Light ligth, float3 normal, float3 toEye, Material mat)
{
    float3 L = -ligth.Direction;
    float ndotl = max(dot(L, normal), 0.0f);
    float3 strength = ligth.Strength * ndotl;
    
    return BlinnPhong(strength, L, normal, toEye, mat);
}


float3 ComputePointLight(Light light, float3 normal, float3 toEye, float3 pos, Material mat)
{
    float3 L = light.Position - pos;
    const float d = length(L);
    L = normalize(L);
    
    float ndotl = max(dot(L, normal), 0.0f);
    float3 att = CalcAttenuation(light.FalloffStart, light.FalloffEnd, d);
    float3 strength = att * ndotl * light.Strength;
    
    return BlinnPhong(strength, L, normal, toEye, mat);
}

float3 ComputeSpotLight(Light light, float3 normal, float3 toEye, float3 pos, Material mat)
{
    float3 L = light.Position - pos;
    const float d = length(L);
    L = normalize(L);
    
    float kspot = pow(max(dot(-L, light.Direction), 0.0f), light.SpotPower);
    float ndotl = max(dot(L, normal), 0.0f);
    float3 att = CalcAttenuation(light.FalloffStart, light.FalloffEnd, d);
    
    float3 strength = att* kspot * ndotl * light.Strength;
    
    return BlinnPhong(strength, L, normal, toEye, mat);
}

float3 ComputeLighting(Light gLights[MaxLights], Material mat,
                       float3 pos, float3 normal, float3 toEye)
{
    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += ComputeDirectionalLight(gLights[i], normal, toEye, mat);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], normal, toEye, pos, mat);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], normal, toEye, pos, mat);
    }
#endif 

    return result;
}
