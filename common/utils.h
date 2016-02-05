///////////////////////////////////////////////////////////////////////////////////////////////////
//  utils.h
//    Utility functions
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <d3d12.h>
#include <cstddef>
#include <cassert>
#include "glm/glm/glm.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
//  CopyTexture()
//    
///////////////////////////////////////////////////////////////////////////////////////////////////
template<bool bIsUpload>
void CopyTexture(ID3D12CommandList* pCL,  ID3D12Resource* pSource, ID3D12Resource* pDest, unsigned int subResource, unsigned int heapOffset)
{
    D3D12_TEXTURE_COPY_LOCATION dst;

    ID3D12Device* pDevice = nullptr;
    pCL->GetDevice(IID_PPV_ARGS(&pDevice));

    if (bIsUpload)
    {
        dst.pResource = pDest;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = subResource;
    }
    else
    {
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.pResource = pDest;
        pDevice->GetCopyableFootprints(&pSource->GetDesc(), subResource, 1, heapOffset, &dst.PlacedFootprint, NULL, NULL, NULL);
    }

    D3D12_TEXTURE_COPY_LOCATION src;
    D3D12_BOX srcBox;
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    if (bIsUpload)
    {
        src.pResource = pSource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        pDevice->GetCopyableFootprints(&pDest->GetDesc(), subResource, 1, heapOffset, &src.PlacedFootprint, NULL, NULL, NULL);

        srcBox.right  = src.PlacedFootprint.Footprint.Width;
        srcBox.bottom = src.PlacedFootprint.Footprint.Height;
        srcBox.back   = src.PlacedFootprint.Footprint.Depth;
    }
    else
    {
        src.pResource = pSource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = subResource;

        srcBox.right  = dst.PlacedFootprint.Footprint.Width;
        srcBox.bottom = dst.PlacedFootprint.Footprint.Height;
        srcBox.back   = dst.PlacedFootprint.Footprint.Depth;
    }

    static_cast<ID3D12GraphicsCommandList*>(pCL)->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox);
}

inline std::size_t GetByteStrideFromFormat(DXGI_FORMAT format)
{
    std::size_t size = 0;
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32_FLOAT:
        size = 12;
        break;

    case DXGI_FORMAT_R32G32_FLOAT:
        size = 8;
        break;

    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R32_UINT:
        size = 4;
        break;

    case DXGI_FORMAT_R16_UINT:
        size = 2;
        break;

    default:
        assert(0);
        size = 0;
        break;
    }

    return size;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  perspective
//     Builds a D3D comaptible perspective transforms, flip the z and scale [0..1]
// y+^
//   |    /
//   |   1
//   |  /: 
//   | / :
//   |/  :
//   +---------------------------> z-
//   | d :
//
//   | d/a             0.0 |  |  x  |
//   |      d          0.0 |  |  y  |
//   |          d      0.0 |  |  z  |
//   |            -1.0 0.0 |  | 1.0 |
//   x/w, y/w, z/w == -1..1, -1..1, d
//
//   z between [n f]
//   z = n  ==>  0.0
//   z = f  ==>  1.0   
//
//   | d/a             0.0      |  |  x  |
//   |      d          0.0      |  |  y  |
//   |         f/(n-f) fn/(n-f) |  |  z  |
//   |          -1.0   0.0      |  | 1.0 |
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
void Perspective(glm::mat4* pMatrix, T fovy, T aspect, T n, T f)
{
   T d(T(1)/std::tan(glm::radians(fovy/T(2))));

   // row 0
   (*pMatrix)[0][0] = d/aspect;
   (*pMatrix)[1][0] = T(0);
   (*pMatrix)[2][0] = T(0);
   (*pMatrix)[3][0] = T(0);
   // row 1
   (*pMatrix)[0][1] = T(0);
   (*pMatrix)[1][1] = d;
   (*pMatrix)[2][1] = T(0);
   (*pMatrix)[3][1] = T(0);
   // row 2
   (*pMatrix)[0][2] = T(0);
   (*pMatrix)[1][2] = T(0);
   (*pMatrix)[2][2] = f/(n-f);
   (*pMatrix)[3][2] = (f*n)/(n-f);
   // row 3
   (*pMatrix)[0][3] = T(0);
   (*pMatrix)[1][3] = T(0);
   (*pMatrix)[2][3] = T(-1);
   (*pMatrix)[3][3] = T(0);
}