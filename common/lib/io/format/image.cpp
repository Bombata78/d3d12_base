///////////////////////////////////////////////////////////////////////////////////////////////////
//  buffer.cpp
//    Manage buffer to system memory
///////////////////////////////////////////////////////////////////////////////////////////////////
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "core/utils.h"
#include "core/texture.h"
#include "io/format/image.h"

namespace io
{
///////////////////////////////////////////////////////////////////////////////////////////////////
//  buffer::buffer
//    Constructor
///////////////////////////////////////////////////////////////////////////////////////////////////
texture* create_texture_from_file(const std::string filename)
{
    // Load the specified texture in a buffer object
    int x, y, n;
    std::uint8_t* pFileContent{stbi_load(filename.c_str(), &x, &y, &n, 0)};
    if (!pFileContent) throw std::invalid_argument(filename);

    try
    {
        // Insert extra padding if alpha is ommited from image specification
        assert(n == 3 || n == 4);
        std::size_t size = x * y * get_stride_from_format(::format::r8g8b8a8_unorm);
        std::unique_ptr<io::buffer> pBuffer{std::make_unique<io::buffer>(std::make_unique<std::uint8_t[]>(size), ::format::r8g8b8a8_unorm, size)};

        // Linearize data supposing a gamma encoding of 2.2
        auto gammaDecode = [] (uint8_t srgbVal) {return static_cast<uint8_t>(std::powf(static_cast<float>(srgbVal)/255.0f, 2.2f) * 255.0f);};
        std::uint8_t* pSrc  = pFileContent;
        std::uint8_t* pDest = pBuffer->data();
        for (std::uint64_t i = 0; i < x*y; ++i)
        {
            pDest[0] = gammaDecode(pSrc[0]);
            pDest[1] = gammaDecode(pSrc[1]);
            pDest[2] = gammaDecode(pSrc[2]);

            // Don't gamma decode alpha channel -- set alpha to opaque if not specified.
            pDest[3] = (n == 4) ? pSrc[3] : 0xFF;

            pSrc    += n;
            pDest   += 4;
        }

        STBI_FREE(pFileContent);

        return new texture(x, y, std::move(pBuffer));
    }
    catch (std::exception& e)
    {
        STBI_FREE(pFileContent);
        throw e;
    }
}

};