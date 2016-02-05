///////////////////////////////////////////////////////////////////////////////////////////////////
//  mesh.h
//    Utility functions to load mesh from disk
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "io.h"
#include <string>
#include <vector>
#include <dxgiformat.h>
#include <d3d12.h>

class IOMesh
{
public:
    struct Attribute
    {
        enum Semantic
        {
            POSITION = 0,
            TEXCOORD = 1,
            NORMAL   = 2,
            UNKNOWN  = ~0
        };
        Attribute() = delete;
        inline Attribute(Semantic, DXGI_FORMAT, std::size_t);

        Semantic    semantic;
        DXGI_FORMAT format;
        std::size_t offset;
    };

    IOMesh(D3D12_PRIMITIVE_TOPOLOGY topology_, IOBuffer&& vertexBuffer_, IOBuffer&& indexBuffer_, std::vector<IOMesh::Attribute>&& attributes_);

    inline D3D12_PRIMITIVE_TOPOLOGY              getTopology()     const;
    inline const IOBuffer&                       getVertexBuffer() const;
    inline const IOBuffer&                       getIndexBuffer()  const;
    inline const std::vector<IOMesh::Attribute>& getAttributes()   const;

private:
    IOMesh()                         = delete;
    IOMesh(const IOMesh&)            = delete;
    IOMesh(IOMesh&&)                 = delete;
    IOMesh& operator=(const IOMesh&) = delete;
    IOMesh& operator=(IOMesh&&)      = delete;

    D3D12_PRIMITIVE_TOPOLOGY       topology;
    IOBuffer                       vertexBuffer;
    IOBuffer                       indexBuffer;
    std::vector<IOMesh::Attribute> attributes;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// IOMesh::getTopology
//    Return mesh topology
////////////////////////////////////////////////////////////////////////////////////////////////////
D3D12_PRIMITIVE_TOPOLOGY IOMesh::getTopology() const
{
    return topology;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Mesh::getVertexBuffer
//    Return mesh vertex buffer
////////////////////////////////////////////////////////////////////////////////////////////////////
const IOBuffer& IOMesh::getVertexBuffer() const
{
    return vertexBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Mesh::getIndexBuffer
//    Return index buffer
////////////////////////////////////////////////////////////////////////////////////////////////////
const IOBuffer& IOMesh::getIndexBuffer() const
{
    return indexBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Mesh::getAttributes
//    Return mesh attributes
////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<IOMesh::Attribute>& IOMesh::getAttributes() const
{
    return attributes;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Mesh::Attribute::Attribute
//    Return mesh attributes
////////////////////////////////////////////////////////////////////////////////////////////////////
IOMesh::Attribute::Attribute(Semantic semantic_, DXGI_FORMAT format_, std::size_t offset_)
    : semantic(semantic_), format(format_), offset(offset_)
{
}

IOMesh* LoadObj(const std::string& filename, bool invertUVs);