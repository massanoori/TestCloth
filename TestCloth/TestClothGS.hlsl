#include "TestCloth.hlsli"

cbuffer cbTestCloth : register(b0)
{
	matrix WorldView;
	matrix Projection;
};

[maxvertexcount(6)]
void main(point VS_OUTPUT Input[1], inout TriangleStream<GS_OUTPUT> triStream)
{
	float4 pos = Input[0].Position;
	pos.w = 1.0f;

	GS_OUTPUT Output;
	Output.Normal = float4(0.0f, 0.0f, -1.0f, 0.0f);

	float size = 0.013f;

	Output.Position = mul(mul(pos, WorldView)
		+ float4(-size, -size, 0.0f, 0.0f), Projection);
	triStream.Append(Output);

	Output.Position = mul(mul(pos, WorldView)
		+ float4( size, -size, 0.0f, 0.0f), Projection);
	triStream.Append(Output);

	Output.Position = mul(mul(pos, WorldView)
		+ float4(-size,  size, 0.0f, 0.0f), Projection);
	triStream.Append(Output);

	Output.Position = mul(mul(pos, WorldView)
		+ float4( size,  size, 0.0f, 0.0f), Projection);
	triStream.Append(Output);

	triStream.RestartStrip();
}
