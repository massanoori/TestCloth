#include "TestCloth.hlsli"

float4 main(GS_OUTPUT Input) : SV_Target
{
	float d = dot(normalize(Input.Normal.xyz),
		normalize(float3(1.0f, 0.2f, -1.0f))) + 0.3f;
	d = saturate(d);
	return float4(float3(1.0f, 0.8f, 0.5f) * d, 1.0f);
}
