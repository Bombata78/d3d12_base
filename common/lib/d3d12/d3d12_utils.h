///////////////////////////////////////////////////////////////////////////////////////////////////
//  d3d12_utils.h
//    Utilities for Direct3D 12 implementation
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <dxgiformat.h>
#include "core/core.h"

namespace d3d12
{
    constexpr DXGI_FORMAT core_to_dxgi_format(::format format) { return static_cast<DXGI_FORMAT>(format); }
    constexpr ::format dxgi_to_core_format(DXGI_FORMAT format) { return static_cast<::format>(format); }

    constexpr D3D_PRIMITIVE_TOPOLOGY core_to_d3d_topology(::topology topology) { return static_cast<D3D_PRIMITIVE_TOPOLOGY>(topology); }
    constexpr ::topology d3d_to_core_topology(D3D_PRIMITIVE_TOPOLOGY topology) { return static_cast<::topology>(topology); }
};
