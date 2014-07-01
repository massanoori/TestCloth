StructuredBuffer<float4> PositionsFrom : register(t0);
StructuredBuffer<float4> VelocitiesFrom : register(t1);
RWStructuredBuffer<float4> PositionsTo : register(u0);
RWStructuredBuffer<float4> VelocitiesTo : register(u1);
RWStructuredBuffer<float4> Normals : register(u2);

struct Spring
{
	float stiffness;
	float damping;
	float restLength;
	float dummy;
};

cbuffer cbTestCloth
{
	Spring Neighbour;
	Spring Diagonal;
	Spring Bending;
	uint2 ClothResolution;
	float TimeStep;
};

uint ComposeID(in uint2 id)
{
	return id.x + id.y * ClothResolution.x;
}

float4 CalcAccel(in uint id0, in uint id1, in Spring spring)
{
	float4 dp = PositionsFrom[id0] - PositionsFrom[id1];
	float4 dv = VelocitiesFrom[id0] - VelocitiesFrom[id1];

	float lenSq = dot(dp.xyz, dp.xyz);
	float len = length(dp.xyz);

	return (spring.stiffness * (spring.restLength / len - 1.0f) -
		spring.damping * dot(dp.xyz, dv.xyz) / lenSq) * dp;
}

[numthreads(128, 2, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	uint id = ComposeID(threadID.xy);

	PositionsTo[id] = PositionsFrom[id];
	VelocitiesTo[id] = VelocitiesFrom[id];

	const uint X_NOT_MIN = threadID.x > 0;
	const uint Y_NOT_MIN = threadID.y > 0;
	const uint X_NOT_MAX = threadID.x < ClothResolution.x - 1;
	const uint Y_NOT_MAX = threadID.y < ClothResolution.y - 1;
	const uint X_NOT_MIN2 = threadID.x > 1;
	const uint Y_NOT_MIN2 = threadID.y > 1;
	const uint X_NOT_MAX2 = threadID.x < ClothResolution.x - 2;
	const uint Y_NOT_MAX2 = threadID.y < ClothResolution.y - 2;

	float4 accel = float4(0.0f, 0.0f, 0.0f, 0.0f);
		if (Y_NOT_MIN)
		{
		if (X_NOT_MIN)
		{
			accel += CalcAccel(id, id - 1, Neighbour);
		}

		if (X_NOT_MAX)
		{
			accel += CalcAccel(id, id + 1, Neighbour);
		}

		if (Y_NOT_MIN)
		{
			accel += CalcAccel(id, id - ClothResolution.x, Neighbour);
		}

		if (Y_NOT_MAX)
		{
			accel += CalcAccel(id, id + ClothResolution.x, Neighbour);
		}

		if (X_NOT_MIN && Y_NOT_MIN)
		{
			accel += CalcAccel(id, id - 1 - ClothResolution.x, Diagonal);
		}

		if (X_NOT_MAX && Y_NOT_MIN)
		{
			accel += CalcAccel(id, id + 1 - ClothResolution.x, Diagonal);
		}

		if (X_NOT_MIN && Y_NOT_MAX)
		{
			accel += CalcAccel(id, id - 1 + ClothResolution.x, Diagonal);
		}

		if (X_NOT_MAX && Y_NOT_MAX)
		{
			accel += CalcAccel(id, id + 1 + ClothResolution.x, Diagonal);
		}

		if (X_NOT_MIN2)
		{
			accel += CalcAccel(id, id - 2, Bending);
		}

		if (X_NOT_MAX2)
		{
			accel += CalcAccel(id, id + 2, Bending);
		}

		if (Y_NOT_MIN2)
		{
			accel += CalcAccel(id, id - ClothResolution.x * 2, Bending);
		}

		if (Y_NOT_MAX2)
		{
			accel += CalcAccel(id, id + ClothResolution.x * 2, Bending);
		}

		accel.y -= 9.8f;
	}

	float4 newVelocity = VelocitiesFrom[id] + accel * TimeStep;
	VelocitiesTo[id] = newVelocity;
	PositionsTo[id] = PositionsFrom[id] + newVelocity * TimeStep;

	float4 normal = float4(0.0f, 0.0f, 0.0f, 0.0f);

	if (X_NOT_MIN && Y_NOT_MIN)
	{
		normal.xyz += cross(
			PositionsFrom[id - 1] - PositionsFrom[id],
			PositionsFrom[id - ClothResolution.x] - PositionsFrom[id]);
	}

	if (X_NOT_MAX && Y_NOT_MIN)
	{
		normal.xyz += cross(
			PositionsFrom[id - ClothResolution.x] - PositionsFrom[id],
			PositionsFrom[id + 1] - PositionsFrom[id]);
	}

	if (X_NOT_MAX && Y_NOT_MAX)
	{
		normal.xyz += cross(
			PositionsFrom[id + 1] - PositionsFrom[id],
			PositionsFrom[id + ClothResolution.x] - PositionsFrom[id]);
	}

	if (X_NOT_MIN && Y_NOT_MAX)
	{
		normal.xyz += cross(
			PositionsFrom[id + ClothResolution.x] - PositionsFrom[id],
			PositionsFrom[id - 1] - PositionsFrom[id]);
	}

	Normals[id] = -normalize(normal);
}