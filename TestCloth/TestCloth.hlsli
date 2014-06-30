
struct VS_INPUT
{
	uint id : SV_VertexID;
};

struct VS_OUTPUT
{
	uint id : POSITION;
};

struct GS_OUTPUT
{
	float4 Position : SV_Position;
	float3 Normal : NORMAL;
};
