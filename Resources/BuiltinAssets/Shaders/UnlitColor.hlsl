[[vk::binding(0, 0)]]
cbuffer FrameData : register(b0, space0)
{
    float4x4 u_ViewProjection;
};

[[vk::binding(0, 1)]]
cbuffer MaterialData : register(b0, space1)
{
    float4 u_Color;
};

[[vk::binding(0, 2)]]
cbuffer ObjectData : register(b0, space2)
{
    float4x4 u_Transform;
};

struct VertexInput
{
    float3 Position : POSITION;
};

struct VertexOutput
{
    float4 Position : SV_Position;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.Position = mul(u_ViewProjection, mul(u_Transform, float4(input.Position, 1.0)));
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    return u_Color;
}
