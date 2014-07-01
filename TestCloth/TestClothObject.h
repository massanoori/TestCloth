#pragma once

#include "ObjectList.h"

namespace TestCloth
{
	struct Spring
	{
		float Stiffness;
		float Damping;
	};

	struct Desc
	{
		Spring Neighbour = Spring{ 100000.0f, 30.0f };
		Spring Diagonal = Spring{ 100000.0f, 30.0f };
		Spring Bending = Spring{ 400000.0f, 20.0f };
		float TimeStep = 0.001f;
	};

	ObjectHandle CreateObject(const Desc& desc);
}
