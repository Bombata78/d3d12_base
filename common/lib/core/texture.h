///////////////////////////////////////////////////////////////////////////////////////////////////
//  texture.h
//    Manage texture object
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <memory>
#include "io/buffer.h"

class texture
{
public:
    texture(std::uint32_t width__, std::uint32_t height__, std::unique_ptr<io::buffer>&& pData__);

    inline std::uint32_t width() const noexcept;
    inline std::uint32_t height() const noexcept;
    inline format format() const noexcept;
    inline std::uint8_t* data() const noexcept;
    inline std::size_t size() const noexcept;

    texture(const texture&)            = delete;
    texture(texture&&)                 = delete;
    texture& operator=(texture&&)      = delete;
    texture& operator=(const texture&) = delete;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::unique_ptr<io::buffer> pData_;
};

//    -- inline definitions --
inline std::uint32_t texture::width() const noexcept
{
    return width_;
}

inline std::uint32_t texture::height() const noexcept
{
    return height_;
}

inline format texture::format() const noexcept
{
    return pData_->format();
}

inline std::uint8_t* texture::data() const noexcept
{
    return pData_->data();
}

inline std::size_t texture::size() const noexcept
{
    return pData_->size();
}