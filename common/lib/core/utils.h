///////////////////////////////////////////////////////////////////////////////////////////////////
//  utils.h
//    Utilities for core object
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cassert>
#include <cstddef>
#include "core/core.h"

inline std::size_t get_stride_from_format(::format format)
{
    std::size_t size = 0;
    switch (format)
    {
    case ::format::r32g32b32_float:
        size = 12;
        break;
    
    case ::format::r32g32_float:
        size = 8;
        break;
    
    case ::format::r8g8b8a8_unorm:
    case ::format::r32_uint:
        size = 4;
        break;
    
    case ::format::r16_uint:
        size = 2;
        break;
    
    default:
        assert(0);
        size = 0;
        break;
    }

    return size;
}