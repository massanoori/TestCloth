RWStructuredBuffer<float4> Positions : register(u0);
RWStructuredBuffer<float4> Velocities : register(u1);

cbuffer cbTestCloth
{
	float4 FourPositions[4];
	uint2 ClothResolution;
};

uint ComposeID(in uint2 id)
{
	return id.x + id.y * ClothResolution.x;
}

[numthreads(128, 2, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	uint2 id2D = threadID.xy;
	uint id = ComposeID(id2D);

	float2 factors = id2D / float2(ClothResolution.x - 1, ClothResolution.y - 1);

	Positions[id] = lerp(
		lerp(FourPositions[0], FourPositions[1], factors.x),
		lerp(FourPositions[2], FourPositions[3], factors.x),
		factors.y);
	Velocities[id] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}
