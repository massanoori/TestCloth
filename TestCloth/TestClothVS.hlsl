#include "TestCloth.hlsli"

ByteAddressBuffer InputPositions : register(t0);

#define FETCH_POSITION(id) asfloat(InputPositions.Load4(id * 16))

VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;
	Output.Position = FETCH_POSITION(Input.ID);

	return Output;
}
