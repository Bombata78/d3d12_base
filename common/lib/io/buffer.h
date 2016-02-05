///////////////////////////////////////////////////////////////////////////////////////////////////
//  buffer.h
//    Manage buffer to system memory
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstddef>
#include <memory>
#include "core/core.h"

namespace io
{

class buffer
{
public:
    buffer(::format format__, std::size_t size__);
    buffer(std::unique_ptr<std::uint8_t[]> pData__, ::format format__, std::size_t size__);
    buffer(std::size_t stride__, std::size_t size__);
    buffer(buffer&& rhs);

    inline std::uint8_t* data()   const noexcept;
    inline ::format      format() const noexcept;
    inline std::size_t   size()   const noexcept;
    inline std::size_t   stride() const noexcept;

    buffer()                         = delete;
    buffer(const buffer&)            = delete;
    buffer& operator=(buffer&&)      = delete;
    buffer& operator=(const buffer&) = delete;

private:
    std::unique_ptr<std::uint8_t[]> pData_;
    ::format                        format_;
    std::size_t                     size_;
    std::size_t                     stride_;
};

//    -- inline definitions --
inline std::uint8_t* buffer::data() const noexcept
{
    return pData_.get();
}

inline ::format buffer::format() const noexcept
{
    return format_;
}

inline std::size_t buffer::size()   const noexcept
{
    return size_;
}

inline std::size_t buffer::stride() const noexcept
{
    return stride_;
}

};