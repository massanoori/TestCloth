#pragma once

#include <boost/intrusive_ptr.hpp>
#include <Unknwn.h>

template <typename Interface>
using ComPtr = boost::intrusive_ptr < Interface >;

void intrusive_ptr_add_ref(IUnknown* p);
void intrusive_ptr_release(IUnknown* p);

