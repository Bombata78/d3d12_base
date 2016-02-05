///////////////////////////////////////////////////////////////////////////////////////////////////
//  texture.cpp
//    Manage texture object
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <cassert>
#include "core/texture.h"
#include "core/utils.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//  texture::texture
//    Constructor
///////////////////////////////////////////////////////////////////////////////////////////////////
texture::texture(std::uint32_t width__, std::uint32_t height__, std::unique_ptr<io::buffer>&& pData__)
    : width_(width__), height_(height__), pData_(std::move(pData__))
{
    assert(pData_.get() && pData_->format() != format::unknown && pData_->stride() == get_stride_from_format(pData_->format()));
}
