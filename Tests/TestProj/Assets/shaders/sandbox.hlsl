[[vk::binding(0, 0)]] cbuffer FrameData : register(b0, space0) { float4x4 u_ViewProjection; };
[[vk::binding(0, 1)]] cbuffer MaterialData : register(b0, space1) { float4 u_Color; };
[[vk::binding(0, 2)]] cbuffer ObjectData : register(b0, space2) { float4x4 u_Transform; };
[[vk::binding(1, 1)]] Texture2D u_Texture : register(t1, space1);
[[vk::binding(2, 1)]] SamplerState u_TextureSampler : register(s2, space1);

struct VertexInput { float3 Position : POSITION; float2 Uv : TEXCOORD0; };
struct VertexOutput { float4 Position : SV_Position; float2 Uv : TEXCOORD0; };

VertexOutput VSMain(VertexInput input) {
    VertexOutput output;
    output.Position = mul(u_ViewProjection, mul(u_Transform, float4(input.Position, 1.0)));
    output.Uv = input.Uv;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0 {
    return u_Color * u_Texture.Sample(u_TextureSampler, input.Uv);
}
