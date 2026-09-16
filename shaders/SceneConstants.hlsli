cbuffer SceneConstants : register(b0)
{
    float4x4 modelViewProjection;
    float4x4 model;
    float3 cameraPosition;
    float lightingEnabled;
    float3 lightDirection;
    float lightIntensity;
    float ambientIntensity;
    float specularIntensity;
    float shininess;
    float tessellationFactor;
    float specularEnabled;
    float colorVisualization;
    float2 constantsPadding;
};

