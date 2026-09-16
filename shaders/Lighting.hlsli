#include "SceneConstants.hlsli"

float3 ApplyLighting(float3 baseColor, float3 worldPosition, float3 worldNormal)
{
    float3 finalColor = baseColor;
    if (lightingEnabled >= 0.5f)
    {
        const float3 normal = normalize(worldNormal);
        const float3 toLight = normalize(-lightDirection);
        const float diffuseFactor = max(dot(normal, toLight), 0.0f);
        const float3 ambient = baseColor * ambientIntensity;
        const float3 diffuse = baseColor * diffuseFactor * lightIntensity;
        float specular = 0.0f;
        if (specularEnabled > 0.5f && diffuseFactor > 0.0f)
        {
            const float3 toCamera = normalize(cameraPosition - worldPosition);
            const float3 halfVector = normalize(toLight + toCamera);
            specular = pow(max(dot(normal, halfVector), 0.0f), shininess)
                     * specularIntensity * lightIntensity;
        }
        finalColor = saturate(ambient + diffuse + specular.xxx);
    }
    return finalColor;
}
