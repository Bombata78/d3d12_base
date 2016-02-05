///////////////////////////////////////////////////////////////////////////////////////////////////
//  heap.h
//    Simple heap managment
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

template <typename T>
constexpr T AlignTo(T x, T alignment)
{
    return ((x + alignment-1)/alignment)*alignment;
}

struct BufferSubAllocation
{
    ID3D12Resource* pResource;
    std::size_t     offset;
    std::size_t     size;
};

template <D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags, std::uint32_t align>
class BufferSubAllocator
{
public:
    BufferSubAllocator(ID3D12Device* pDevice_, std::size_t size_);
    ~BufferSubAllocator();

    BufferSubAllocation&& subAllocate(std::size_t size_);

private:
    ID3D12Device*   pDevice;
    ID3D12Resource* pResource;
    std::size_t     size;
    std::size_t     currentOffset;
};

template <D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags, std::uint32_t align>
BufferSubAllocator<type, flags, align>::BufferSubAllocator(ID3D12Device* pDevice_, std::size_t size_)
    : pDevice(pDevice_), size(AlignTo(size_, static_cast<std::size_t>(align))), currentOffset(0)
{
    D3D12_HEAP_PROPERTIES  heapProperties;
    heapProperties.Type                 = type;
    heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask     = 0;
    heapProperties.VisibleNodeMask      = 0;

    D3D12_RESOURCE_DESC  resourceDesc;
    resourceDesc.Dimension              = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment              = 0;
    resourceDesc.Width                  = size;
    resourceDesc.Height                 = 1;
    resourceDesc.DepthOrArraySize       = 1;
    resourceDesc.MipLevels              = 1;
    resourceDesc.Format                 = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count       = 1;
    resourceDesc.SampleDesc.Quality     = 0;
    resourceDesc.Layout                 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags                  = flags;

    if (FAILED(pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&pResource))))
    {
        throw std::exception("** Can't create sub allocator\n");
    }
}

template <D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags, std::uint32_t align>
BufferSubAllocator<type, flags, align>::~BufferSubAllocator()
{
    pResource->Release();
}

template <D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags, std::uint32_t align>
BufferSubAllocation&& BufferSubAllocator<type, flags, align>::subAllocate(std::size_t size_)
{
    BufferSubAllocation subAllocation;
    subAllocation.pResource = pResource;
    subAllocation.offset    = currentOffset;
    subAllocation.size      = AlignTo(size_, static_cast<std::size_t>(align));
    
    if (currentOffset + subAllocation.size >= size) throw std::exception("Can't sub-allocate");
    currentOffset += subAllocation.size;

    return std::move(subAllocation);
}