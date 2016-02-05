///////////////////////////////////////////////////////////////////////////////////////////////////
//  mesh.h
//    Manage mesh object
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <vector>
#include "core/core.h"
#include "io/buffer.h"

class mesh
{
public:
    struct attribute
    {
        enum semantic_type
        {
            position = 0,
            texcoord = 1,
            normal   = 2,
            unknown  = ~0
        };

        inline attribute(semantic_type, ::format, std::size_t);
        attribute() = delete;
    
        semantic_type semantic;
        ::format      format;
        std::size_t   offset;
    };

    mesh(::topology topology__, io::buffer&& vertex_buffer__, io::buffer&& index_buffer__, std::vector<mesh::attribute>&& attributes__);

    inline ::topology topology() const noexcept;
    inline const io::buffer& vertex_buffer() const noexcept;
    inline const io::buffer& index_buffer() const noexcept;
    inline const std::vector<attribute>& attributes() const noexcept;

    mesh()  = delete;
    mesh(const mesh&) = delete;
    mesh(mesh&&) = delete;
    mesh& operator=(mesh&&) = delete;
    mesh& operator=(const mesh&) = delete;

private:
    ::topology             topology_;
    io::buffer             vertex_buffer_;
    io::buffer             index_buffer_;
    std::vector<attribute> attributes_;
};

//    -- inline definitions --
inline ::topology mesh::topology() const noexcept
{
    return topology_;
}

inline const io::buffer& mesh::vertex_buffer() const noexcept
{
    return vertex_buffer_;
}

inline const io::buffer& mesh::index_buffer() const noexcept
{
    return index_buffer_;
}

inline const std::vector<mesh::attribute>& mesh::attributes() const noexcept
{
    return attributes_;
}

inline mesh::attribute::attribute(mesh::attribute::semantic_type semantic__, ::format format__, std::size_t offset__)
    : semantic(semantic__), format(format__), offset(offset__)
{
}