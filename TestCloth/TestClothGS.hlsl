#include "TestCloth.hlsli"

StructuredBuffer<float4> InputPositions : register(t0);
StructuredBuffer<float4> InputNormals : register(t1);

cbuffer cbTestClothMatrices : register(b0)
{
	matrix WorldView;
	matrix Projection;
	uint2 ClothResolution;
};

uint2 DecomposeID(in uint id)
{
	return uint2(id % ClothResolution.x,
		id / ClothResolution.x);
}

uint RecomposeID(in uint2 id)
{
	return id.x + id.y * ClothResolution.x;
}

[maxvertexcount(8)]
void main(point VS_OUTPUT Input[1], inout TriangleStream<GS_OUTPUT> triStream)
{
	uint id = Input[0].id;
	uint2 ID2 = DecomposeID(id);
	if (ID2.x > 0 && ID2.y > 0)
	{
		float4 pos;
		GS_OUTPUT Output;

		pos = InputPositions[id - 1];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = InputNormals[id - 1].xyz;
		triStream.Append(Output);

		pos = InputPositions[id];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = InputNormals[id].xyz;
		triStream.Append(Output);

		pos = InputPositions[id - ClothResolution.x - 1];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = InputNormals[id - ClothResolution.x - 1].xyz;
		triStream.Append(Output);
	
		pos = InputPositions[id - ClothResolution.x];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = InputNormals[id - ClothResolution.x].xyz;
		triStream.Append(Output);

		triStream.RestartStrip();

		pos = InputPositions[id];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = -InputNormals[id].xyz;
		triStream.Append(Output);

		pos = InputPositions[id - 1];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = -InputNormals[id - 1].xyz;
		triStream.Append(Output);

		pos = InputPositions[id - ClothResolution.x];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = -InputNormals[id - ClothResolution.x].xyz;
		triStream.Append(Output);

		pos = InputPositions[id - ClothResolution.x - 1];
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = -InputNormals[id - ClothResolution.x - 1].xyz;
		triStream.Append(Output);

		triStream.RestartStrip();
	}
}
