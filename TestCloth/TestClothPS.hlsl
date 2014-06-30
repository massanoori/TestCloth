#include "TestCloth.hlsli"

float4 main(GS_OUTPUT Input) : SV_Target
{
	float d = dot(Input.Normal.xyz, float3(0.0f, 0.0f, -1.0f));
	return float4(float3(1.0f, 0.8f, 0.5f) * d, 1.0f);
}
