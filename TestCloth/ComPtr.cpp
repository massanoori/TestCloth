#include "stdafx.h"

void intrusive_ptr_add_ref(IUnknown* p)
{
	p->AddRef();
}

void intrusive_ptr_release(IUnknown* p)
{
	p->Release();
}
