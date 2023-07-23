cbuffer ConstantBuffer : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4 WorldLightPosition;
    float4 WorldCameraPosition;
}

struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float3 LightDirection : TEXCOORD1;
    float3 ViewDirection : TEXCOORD2;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = mul(input.Position, WorldMatrix);
    
    output.LightDirection = normalize(output.Position.xyz - WorldLightPosition.xyz);
    
    output.ViewDirection = normalize(output.Position.xyz - WorldCameraPosition.xyz);
    
    output.Position = mul(output.Position, ViewMatrix);
    output.Position = mul(output.Position, ProjectionMatrix);
    
    output.Normal = mul(input.Normal, (float3x3) WorldMatrix);
    output.Normal = normalize(output.Normal);
    
    return output;
}

float4 PS(VS_OUTPUT input) : SV_Target
{
    float3 diffuse = saturate(dot(-input.LightDirection, input.Normal));
    
    float3 reflection = reflect(input.LightDirection, input.Normal);
    float3 viewDirection = normalize(input.ViewDirection);
    float3 specular = 0.0f;
    
    if (diffuse.x > 0.0f)
    {
        specular = saturate(dot(reflection, -viewDirection));
        specular = pow(specular, 20.0f);
    }
    
    float3 ambient = float3(0.03f, 0.03f, 0.03f);
    
    return float4(diffuse + specular + ambient, 1.0f);
}
