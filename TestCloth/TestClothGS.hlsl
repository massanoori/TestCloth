#include "TestCloth.hlsli"

ByteAddressBuffer InputPositions : register(t0);
ByteAddressBuffer InputNormals : register(t1);
#define FETCH_POSITION(id) asfloat(InputPositions.Load4((id) * 16))
#define FETCH_NORMAL(id) asfloat(InputNormals.Load4((id) * 16))

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

[maxvertexcount(6)]
void main(point VS_OUTPUT Input[1], inout TriangleStream<GS_OUTPUT> triStream)
{
	uint id = Input[0].id;
	uint2 ID2 = DecomposeID(id);
	if (ID2.x > 0 && ID2.y > 0)
	{
		float4 pos;
		GS_OUTPUT Output;

		pos = FETCH_POSITION(id);
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = FETCH_NORMAL(id);
		triStream.Append(Output);

		pos = FETCH_POSITION(id - 1);
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = FETCH_NORMAL(id - 1);
		triStream.Append(Output);

		pos = FETCH_POSITION(id - ClothResolution.x);
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = FETCH_NORMAL(id - ClothResolution.x);
		triStream.Append(Output);

		pos = FETCH_POSITION(id - ClothResolution.x - 1);
		pos.w = 1.0f;
		Output.Position = mul(mul(pos, WorldView), Projection);
		Output.Normal = FETCH_NORMAL(id - ClothResolution.x - 1);
		triStream.Append(Output);

		triStream.RestartStrip();
	}
}
