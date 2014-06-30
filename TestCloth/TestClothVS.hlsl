#include "TestCloth.hlsli"

VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;
	Output.id = Input.id;

	return Output;
}
