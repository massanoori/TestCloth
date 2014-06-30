
struct VS_INPUT
{
	uint ID : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 Position : POSITION;
};

struct GS_OUTPUT
{
	float4 Position : SV_Position;
	float3 Normal : NORMAL;
};
