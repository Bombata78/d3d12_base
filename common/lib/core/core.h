///////////////////////////////////////////////////////////////////////////////////////////////////
//  core.h
//    Core definition
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstddef>
#include <cstdint>

enum class topology
{
    triangle_list = 4
};

// Follow D3D_PRIMITIVE_TOPOLOGY
#ifdef _WIN32
#include <d3dcommon.h>
static_assert(static_cast<std::uint32_t>(topology::triangle_list)  == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,  "topology::triangle_list different than D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST");
#endif

enum class format
{
    unknown = 0,
    r32g32b32_float = 6,
    r32g32_float = 16,
    r8g8b8a8_unorm = 28,
    r32_uint = 42,
    r16_uint = 57
};

// Follow DXGI_FORMAT
#ifdef _WIN32
#include <dxgiformat.h>
static_assert(static_cast<std::uint32_t>(format::unknown)  == DXGI_FORMAT_UNKNOWN, "format::unknown different than DXGI_FORMAT");
static_assert(static_cast<std::uint32_t>(format::r32g32b32_float)  == DXGI_FORMAT_R32G32B32_FLOAT,  "format::r32g32b32_float different than DXGI_FORMAT");
static_assert(static_cast<std::uint32_t>(format::r32g32_float)  == DXGI_FORMAT_R32G32_FLOAT,  "format::r32g32_float different than DXGI_FORMAT");
static_assert(static_cast<std::uint32_t>(format::r8g8b8a8_unorm)  == DXGI_FORMAT_R8G8B8A8_UNORM,  "format::r8g8b8a8_unorm different than DXGI_FORMAT");
static_assert(static_cast<std::uint32_t>(format::r32_uint)  == DXGI_FORMAT_R32_UINT,  "format::r32_uint different than DXGI_FORMAT");
static_assert(static_cast<std::uint32_t>(format::r16_uint) == DXGI_FORMAT_R16_UINT, "format::r16_uint different than DXGI_FORMAT");
#endif

