///////////////////////////////////////////////////////////////////////////////////////////////////
//  d3d12_base.cpp
//    Direct3D 12 base application
//     
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <queue>
#include <memory>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include "lib/glm/glm/glm.hpp"
#include "lib/glm/glm/gtc/type_ptr.hpp"
#include "lib/glm/glm/gtx/transform.hpp"
#include "common/utils.h"
#include "common/mesh.h"
#include "common/heap.h"

//-------------------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------------------
struct TransformCB
{
    glm::mat4 PVMMatrix;
    glm::mat4 modelMatrix;
};

struct CommandListSubmission
{
    CommandListSubmission(ID3D12CommandAllocator* pCA_, ID3D12GraphicsCommandList* pCL_, UINT64 fence_)
        : pCA(pCA_), pCL(pCL_), fence(fence_) {}

    ID3D12CommandAllocator* pCA;
    ID3D12CommandList*      pCL;
    UINT64                  fence;
};

struct CommandQueueData
{
    ID3D12CommandQueue*               pCommandQueue = nullptr;
    ID3D12Fence*                      pFence        = nullptr;
    UINT64                            fenceValue    = 0;
    std::queue<CommandListSubmission> runningCL;
};

struct Mesh
{
    D3D12_VERTEX_BUFFER_VIEW vbView;
    D3D12_INDEX_BUFFER_VIEW  ibView;
    std::uint32_t            indexCount;
    D3D12_PRIMITIVE_TOPOLOGY topology;
};

struct Material
{
    ID3D12PipelineState*  pPSO;
};

struct Primitive
{
    Mesh*                 pMesh;
    Material*             pMaterial;
    ID3D12RootSignature*  pRootSignature;
};

struct GraphicsPass
{
    std::vector<Primitive*> primitive;
};

struct ComputePass
{
    ID3D12RootSignature* pRootSignature;
    ID3D12PipelineState* pPSO;
};

constexpr unsigned int SWAP_CHAIN_SIZE = 2;
constexpr std::size_t  VB_IB_SUB_ALLOCATOR_SIZE = 16 * 1024 * 1024; // 16 MB of geometry
using VbIbBufferSubAllocator = BufferSubAllocator<D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>;

HWND                   hWnd;
IDXGISwapChain*        pSwapChain        = nullptr;
ID3D12Device*          pD3DDevice        = nullptr;
ID3D12Resource*        pSwapChainRT      = nullptr;
ID3D12Resource*        pDepthStencil     = nullptr;
ID3D12DescriptorHeap*  pCbvSrvUavHeap    = nullptr;
ID3D12DescriptorHeap*  pRTHeap           = nullptr;
ID3D12DescriptorHeap*  pDSVHeap          = nullptr;
ID3D12RootSignature*   pRootSignature    = nullptr;
ID3D12PipelineState*   pGraphicsPSO      = nullptr;
ID3D12Resource*        pUploadHeap       = nullptr;
ID3D12Resource*        pTexture          = nullptr;

std::unique_ptr<VbIbBufferSubAllocator> pVbIbSubAllocator;

//std::unique_ptr<Mesh>  pMesh;
Mesh                   mesh;
CommandQueueData       gfxCommandQueue;
std::uint8_t           currentRTVIndex   = 0;
glm::ivec2             windowDimension(1024, 768);

//-------------------------------------------------------------------------------------------------
// Forward declarations
//-------------------------------------------------------------------------------------------------
HRESULT               InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT               InitD3D12();
CommandListSubmission GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE type);
void                  SubmitCL(CommandQueueData& cq, CommandListSubmission& submission);
LRESULT CALLBACK      WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void                  AddBarrierTransition(ID3D12GraphicsCommandList* pCL, ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
void                  Render();
void                  ResizeWindow(const glm::ivec2& newDimension);
void                  DestroyGlobalObjects();

Mesh*                 CreateMesh();

///////////////////////////////////////////////////////////////////////////////////////////////////
//  wWinMain
//    Program entry point
///////////////////////////////////////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

    try
    {
        InitWindow(hInstance, nCmdShow);
        InitD3D12();

        // Main message loop
        MSG msg = {0};
        while(WM_QUIT != msg.message) {
           if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
              TranslateMessage(&msg);
              DispatchMessage(&msg);
           } else {
              Render();
           }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Something gone wrong: " << e.what() << std::endl;
    }

    // Wait for work completion before freeing objects
    HANDLE waitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    gfxCommandQueue.pFence->SetEventOnCompletion(gfxCommandQueue.fenceValue, waitEvent);
    WaitForSingleObject(waitEvent, INFINITE);

    DestroyGlobalObjects();

	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//  InitWindow
//    Initialize a Win32 window
///////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, reinterpret_cast<LPCTSTR>("IDI_ICON1"));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "d3d12_base";
    wcex.hIconSm = LoadIcon(wcex.hInstance, reinterpret_cast<LPCTSTR>(IDI_WINLOGO));
    if (!RegisterClassEx(&wcex)) throw std::exception();

    // Create window
    RECT rc = { 0, 0, windowDimension.x, windowDimension.y };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    hWnd = CreateWindow("d3d12_base", "Direct3D 12 Renderer", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        UnregisterClass("d3d12_base", (HINSTANCE)GetModuleHandle(NULL));
        throw std::exception();
    }

    ShowWindow(hWnd, nCmdShow);

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  InitD3D12
//    Initialize the direct3d 12 device
///////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT InitD3D12()
{
    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pD3DDevice))))
    {
        throw std::exception("** Can't create D3D12 device\n");
    }

    // Create a graphics command queue
    D3D12_COMMAND_QUEUE_DESC desc;
    desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = 0;
    desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;
    if (FAILED(pD3DDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&gfxCommandQueue.pCommandQueue))))
    {
        throw std::exception("** Can't create D3D12 gfx queue\n");
    }

    // Create the tracking fence
    if (FAILED(pD3DDevice->CreateFence(gfxCommandQueue.fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gfxCommandQueue.pFence))))
    {
        throw std::exception("** Can't create D3D12 gfx fence\n");
    }

    // Create the VB/IB sub allocator
    pVbIbSubAllocator = std::make_unique<VbIbBufferSubAllocator>(pD3DDevice, VB_IB_SUB_ALLOCATOR_SIZE);

    // ----- Create the descriptor heap -----
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask       = 0;
    pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pCbvSrvUavHeap));
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 2;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask       = 0;
    pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pRTHeap));
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask       = 0;
    pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pDSVHeap));

    // ----- Create the root signature -----
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    D3D12_DESCRIPTOR_RANGE    descRange;
    D3D12_ROOT_PARAMETER      rootParameter[2];

    // cb0
    rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameter[0].Constants.ShaderRegister = 0;
    rootParameter[0].Constants.RegisterSpace  = 0;
    rootParameter[0].Constants.Num32BitValues = 32;
    rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    // t0
    descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange.BaseShaderRegister = 0;
    descRange.NumDescriptors = 1;
    descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descRange.RegisterSpace = 0;
    rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter[1].DescriptorTable.pDescriptorRanges = &descRange;
    rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    // s0
    D3D12_STATIC_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MipLODBias       = 0.0f;
    samplerDesc.MaxAnisotropy    = 1;
    samplerDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    samplerDesc.MinLOD           = 0;
    samplerDesc.MaxLOD           = 0;
    samplerDesc.ShaderRegister   = 0;
    samplerDesc.RegisterSpace    = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootSignatureDesc.NumParameters     = 2;
    rootSignatureDesc.pParameters       = rootParameter;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers   = &samplerDesc;
    rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    {
        ID3DBlob* pBlob = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pBlob, NULL)))
        {
            throw std::exception("** Can't create root signature\n");
        }

        if (FAILED(pD3DDevice->CreateRootSignature(0, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature))))
        {
            throw std::exception("** Can't create root signature\n");
        }
        pBlob->Release();
    }

    // ----- Load the base mesh -----
    std::unique_ptr<IOMesh> pMesh{LoadObj("../../../assets/models/misc/box.obj", true)};

    // ----- Create the graphics PSO -----
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gfxPSO;
    memset(&gfxPSO, 0, sizeof(gfxPSO));
    gfxPSO.pRootSignature = pRootSignature;

    // Compile the vertex shader
    ID3DBlob* pVSBlob = NULL;
    {
        // Load vertex shader code
        std::string shaderSource((std::istreambuf_iterator<char>(std::ifstream("shaders/base.vs"))), std::istreambuf_iterator<char>());

        // Compile shader
        ID3DBlob* pErrors = NULL;
        if (FAILED(D3DCompile(shaderSource.c_str(), shaderSource.length(), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &pVSBlob, &pErrors)) || pErrors)
        {
            char* d = (char*)pErrors->GetBufferPointer();
            pErrors->Release();
            throw std::exception("** Can't compile VS\n");
        }
        
        // Set the vertex shader
        gfxPSO.VS.pShaderBytecode = pVSBlob->GetBufferPointer();
        gfxPSO.VS.BytecodeLength  = pVSBlob->GetBufferSize();
    }

    // Compile the pixel shader
    ID3DBlob* pPSBlob = NULL;
    {
        // Load pixel shader code
        std::string shaderSource((std::istreambuf_iterator<char>(std::ifstream("shaders/base.ps"))), std::istreambuf_iterator<char>());

        // Compile shader
        ID3DBlob* pErrors = NULL;
        if (FAILED(D3DCompile(shaderSource.c_str(), shaderSource.length(), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &pPSBlob, &pErrors)) || pErrors)
        {
            char* d = (char*)pErrors->GetBufferPointer();
            pErrors->Release();
            throw std::exception("** Can't compile PS\n");
        }

        // Set the vertex shader
        gfxPSO.PS.pShaderBytecode = pPSBlob->GetBufferPointer();
        gfxPSO.PS.BytecodeLength  = pPSBlob->GetBufferSize();
    }

    // Set the blend state
    gfxPSO.BlendState.AlphaToCoverageEnable                 = FALSE;
    gfxPSO.BlendState.IndependentBlendEnable                = FALSE;
    gfxPSO.BlendState.RenderTarget[0].BlendEnable           = FALSE;
    gfxPSO.BlendState.RenderTarget[0].LogicOpEnable         = FALSE;
    gfxPSO.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
    gfxPSO.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_ZERO;
    gfxPSO.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
    gfxPSO.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
    gfxPSO.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
    gfxPSO.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    gfxPSO.BlendState.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
    gfxPSO.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    gfxPSO.SampleMask = ~0;

    // Set the rasterizer state
    gfxPSO.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    gfxPSO.RasterizerState.CullMode              = D3D12_CULL_MODE_BACK;
    gfxPSO.RasterizerState.FrontCounterClockwise = TRUE;
    gfxPSO.RasterizerState.DepthBias             = 0;
    gfxPSO.RasterizerState.DepthBiasClamp        = 0.0f;
    gfxPSO.RasterizerState.SlopeScaledDepthBias  = 0.0f;
    gfxPSO.RasterizerState.DepthClipEnable       = TRUE;
    gfxPSO.RasterizerState.MultisampleEnable     = FALSE;
    gfxPSO.RasterizerState.AntialiasedLineEnable = FALSE;
    gfxPSO.RasterizerState.ForcedSampleCount     = 0;
    gfxPSO.RasterizerState.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Set the depth stencil state
    gfxPSO.DepthStencilState.DepthEnable                  = TRUE;
    gfxPSO.DepthStencilState.DepthWriteMask               = D3D12_DEPTH_WRITE_MASK_ALL;
    gfxPSO.DepthStencilState.DepthFunc                    = D3D12_COMPARISON_FUNC_LESS;
    gfxPSO.DepthStencilState.StencilEnable                = FALSE;
    gfxPSO.DepthStencilState.StencilReadMask              = D3D12_DEFAULT_STENCIL_READ_MASK;
    gfxPSO.DepthStencilState.StencilWriteMask             = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    gfxPSO.DepthStencilState.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
    gfxPSO.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    gfxPSO.DepthStencilState.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
    gfxPSO.DepthStencilState.FrontFace.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
    gfxPSO.DepthStencilState.BackFace.StencilFunc         = D3D12_COMPARISON_FUNC_ALWAYS;
    gfxPSO.DepthStencilState.BackFace.StencilDepthFailOp  = D3D12_STENCIL_OP_KEEP;
    gfxPSO.DepthStencilState.BackFace.StencilPassOp       = D3D12_STENCIL_OP_KEEP;
    gfxPSO.DepthStencilState.BackFace.StencilFailOp       = D3D12_STENCIL_OP_KEEP;

    // Set the input layout definition
    assert(pMesh->getAttributes().size() == 3);
    D3D12_INPUT_ELEMENT_DESC iaDesc[3];
    gfxPSO.InputLayout.NumElements = 0;
    gfxPSO.InputLayout.pInputElementDescs = iaDesc;
    for (const IOMesh::Attribute& attribute : pMesh->getAttributes())
    {
        switch (attribute.semantic)
        {
        case IOMesh::Attribute::POSITION:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "POSITION";
            break;

        case IOMesh::Attribute::NORMAL:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "NORMAL";
            break;

        case IOMesh::Attribute::TEXCOORD:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "TEXCOORD";
            break;
        }

        iaDesc[gfxPSO.InputLayout.NumElements].SemanticIndex        = 0;
        iaDesc[gfxPSO.InputLayout.NumElements].Format               = attribute.format;
        iaDesc[gfxPSO.InputLayout.NumElements].InputSlot            = 0;
        iaDesc[gfxPSO.InputLayout.NumElements].AlignedByteOffset    = static_cast<UINT>(attribute.offset);
        iaDesc[gfxPSO.InputLayout.NumElements].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        iaDesc[gfxPSO.InputLayout.NumElements].InstanceDataStepRate = 0;
        ++gfxPSO.InputLayout.NumElements;
    }

    // Misc PSO attributes
    gfxPSO.IBStripCutValue       = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gfxPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gfxPSO.NumRenderTargets      = 1;
    gfxPSO.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    gfxPSO.DSVFormat             = DXGI_FORMAT_UNKNOWN;
    gfxPSO.SampleDesc.Count      = 1;
    gfxPSO.SampleDesc.Quality    = 0;
    gfxPSO.NodeMask              = 0;
    gfxPSO.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

    if (FAILED(pD3DDevice->CreateGraphicsPipelineState(&gfxPSO, IID_PPV_ARGS(&pGraphicsPSO))))
    {
        throw std::exception("** Can't create graphics PSO\n");
    }

    pVSBlob->Release();
    pPSBlob->Release();

    // Create the constant buffer heap
    D3D12_HEAP_PROPERTIES  heapProperties;
    heapProperties.Type                 = D3D12_HEAP_TYPE_UPLOAD;
    heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask     = 0;
    heapProperties.VisibleNodeMask      = 0;
    D3D12_RESOURCE_DESC  resourceDesc;
    resourceDesc.Dimension              = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment              = 0;
    resourceDesc.Width                  = 16 * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    resourceDesc.Height                 = 1;
    resourceDesc.DepthOrArraySize       = 1;
    resourceDesc.MipLevels              = 1;
    resourceDesc.Format                 = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count       = 1;
    resourceDesc.SampleDesc.Quality     = 0;
    resourceDesc.Layout                 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags                  = D3D12_RESOURCE_FLAG_NONE;
    if (FAILED(pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&pUploadHeap))))
    {
        throw std::exception("** Can't cb heap\n");
    }

    // Allocate and initialize the vertex buffer
    BufferSubAllocation vbSubAllocation = pVbIbSubAllocator->subAllocate(pMesh->getVertexBuffer().getSize());
    mesh.vbView.BufferLocation = vbSubAllocation.pResource->GetGPUVirtualAddress() + vbSubAllocation.offset;
    mesh.vbView.StrideInBytes  = static_cast<UINT>(pMesh->getVertexBuffer().getByteStride());
    mesh.vbView.SizeInBytes    = static_cast<UINT>(pMesh->getVertexBuffer().getSize());
    void* pMappedResource;
    if (FAILED(vbSubAllocation.pResource->Map(0, NULL, reinterpret_cast<void**>(&pMappedResource)))) throw std::exception("** Can't map constant buffer\n");
    memcpy(static_cast<std::uint8_t*>(pMappedResource) + vbSubAllocation.offset, pMesh->getVertexBuffer().getData(), pMesh->getVertexBuffer().getSize());
    vbSubAllocation.pResource->Unmap(0, NULL);

    // Allocate and initialize the index buffer
    BufferSubAllocation ibSubAllocation = pVbIbSubAllocator->subAllocate(pMesh->getIndexBuffer().getSize());
    mesh.ibView.BufferLocation = ibSubAllocation.pResource->GetGPUVirtualAddress() + ibSubAllocation.offset;
    mesh.ibView.Format         = pMesh->getIndexBuffer().getFormat();
    mesh.ibView.SizeInBytes    = static_cast<UINT>(pMesh->getIndexBuffer().getSize());
    mesh.indexCount            = static_cast<std::uint32_t>(mesh.ibView.SizeInBytes/GetByteStrideFromFormat(mesh.ibView.Format));
    mesh.topology              = pMesh->getTopology();
    if (FAILED(ibSubAllocation.pResource->Map(0, NULL, reinterpret_cast<void**>(&pMappedResource)))) throw std::exception("** Can't map constant buffer\n");
    memcpy(static_cast<std::uint8_t*>(pMappedResource) + ibSubAllocation.offset, pMesh->getIndexBuffer().getData(), pMesh->getIndexBuffer().getSize());
    vbSubAllocation.pResource->Unmap(0, NULL);

    heapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask     = 0;
    heapProperties.VisibleNodeMask      = 0;
    resourceDesc.Dimension              = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment              = 0;
    resourceDesc.Width                  = 4;
    resourceDesc.Height                 = 4;
    resourceDesc.DepthOrArraySize       = 1;
    resourceDesc.MipLevels              = 1;
    resourceDesc.Format                 = DXGI_FORMAT_R8G8B8A8_UNORM;
    resourceDesc.SampleDesc.Count       = 1;
    resourceDesc.SampleDesc.Quality     = 0;
    resourceDesc.Layout                 = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags                  = D3D12_RESOURCE_FLAG_NONE;
    if (FAILED(pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&pTexture))))
    {
        throw std::exception("** Can't create the texture heap\n");
    }

    // Initialize texture content
    {
        std::uint32_t* pTexData;
        pUploadHeap->Map(0, NULL, reinterpret_cast<void**>(&pTexData));
        pTexData[0] = 0xFFFFFFFF; pTexData[1] = 0x00000000; pTexData[2] = 0xFFFFFFFF; pTexData[3] = 0x00000000;
        pTexData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT/4;
        pTexData[0] = 0x00000000; pTexData[1] = 0xFFFFFFFF; pTexData[2] = 0x00000000; pTexData[3] = 0xFFFFFFFF;
        pTexData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT/4;
        pTexData[0] = 0xFFFFFFFF; pTexData[1] = 0x00000000; pTexData[2] = 0xFFFFFFFF; pTexData[3] = 0x00000000;
        pTexData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT/4;
        pTexData[0] = 0x00000000; pTexData[1] = 0xFFFFFFFF; pTexData[2] = 0x00000000; pTexData[3] = 0xFFFFFFFF;
        pUploadHeap->Unmap(0, NULL);

        CommandListSubmission submission = GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE_DIRECT);
        AddBarrierTransition(static_cast<ID3D12GraphicsCommandList*>(submission.pCL), pTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        CopyTexture<true>(submission.pCL,  pUploadHeap, pTexture, 0, 0);
        AddBarrierTransition(static_cast<ID3D12GraphicsCommandList*>(submission.pCL), pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        static_cast<ID3D12GraphicsCommandList*>(submission.pCL)->Close();
        SubmitCL(gfxCommandQueue, submission);

        // Create the SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip     = 0;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.PlaneSlice          = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        pD3DDevice->CreateShaderResourceView(pTexture, &srvDesc, pCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
    }

    ResizeWindow(windowDimension);

    return S_OK;
}

CommandListSubmission GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE type)
{
    std::queue<CommandListSubmission>* pQueue      = nullptr;
    ID3D12Fence*                       pCQFence    = nullptr;

    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        pQueue   = &gfxCommandQueue.runningCL;
        pCQFence = gfxCommandQueue.pFence;
    }

    if (pQueue->empty() || pQueue->front().fence > pCQFence->GetCompletedValue())
    {
        ID3D12CommandAllocator* pCA = nullptr;
        if (FAILED(pD3DDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCA))))
        {
            throw std::exception("** Can't create command allocator\n");
        }
        ID3D12GraphicsCommandList* pCL = nullptr;
        if (FAILED(pD3DDevice->CreateCommandList(0, type, pCA, NULL, IID_PPV_ARGS(&pCL))))
        {
            throw std::exception("** Can't create command list\n");
        }

        return CommandListSubmission(pCA, pCL, 0);
    }
    else
    {
        CommandListSubmission cl = pQueue->front();
        pQueue->pop();

        cl.pCA->Reset();
        cl.pCL->Release();

        if (FAILED(pD3DDevice->CreateCommandList(0, type, cl.pCA, NULL, IID_PPV_ARGS(&cl.pCL))))
        {
            throw std::exception("** Can't create command list\n");
        }

        cl.fence = 0;
        return cl;
    }
}

void SubmitCL(CommandQueueData& cq, CommandListSubmission& submission)
{
    cq.pCommandQueue->ExecuteCommandLists(1, &submission.pCL);
    cq.pCommandQueue->Signal(gfxCommandQueue.pFence, ++cq.fenceValue);
    submission.fence = cq.fenceValue;
    cq.runningCL.push(submission);
}

void AddBarrierTransition(ID3D12GraphicsCommandList* pCL, ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = pResource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter  = stateAfter;
    pCL->ResourceBarrier(1, &barrier);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  Render()
//    Main render function
///////////////////////////////////////////////////////////////////////////////////////////////////
glm::vec3 eulerAngle;
glm::vec3 cameraPos(0.0f, 0.0f, 5.0f);
void Render()
{
    // Submit graphics work
    {
        CommandListSubmission submission = GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE_DIRECT);
        ID3D12GraphicsCommandList* pCL = static_cast<ID3D12GraphicsCommandList*>(submission.pCL);

        // Initialize descriptor heaps
        pCL->SetPipelineState(pGraphicsPSO);
        pCL->SetGraphicsRootSignature(pRootSignature);
        pCL->SetDescriptorHeaps(1, &pCbvSrvUavHeap);
        pCL->SetGraphicsRootDescriptorTable(1, pCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

        // Set root constant buffer
        glm::mat4 rotationMatrix      = glm::rotate(eulerAngle.x, 1.0f, 0.0f, 0.0f);
        rotationMatrix               *= glm::rotate(eulerAngle.y, 0.0f, 1.0f, 0.0f);
        glm::mat4 viewTransform       = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 perspectiveTranform;
        Perspective(&perspectiveTranform, 50.0f, static_cast<float>(windowDimension.x)/static_cast<float>(windowDimension.y), 0.2f, 10.0f);
        perspectiveTranform = perspectiveTranform * viewTransform * rotationMatrix;

        pCL->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(perspectiveTranform), 0);
        pCL->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(rotationMatrix), 16);

        // Set VP and scissors
        D3D12_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width = static_cast<float>(windowDimension.x);
        vp.Height = static_cast<float>(windowDimension.y);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        pCL->RSSetViewports(1, &vp);
        D3D12_RECT scissorRect;
        scissorRect.left = 0;
        scissorRect.top = 0;
        scissorRect.right = windowDimension.x;
        scissorRect.bottom = windowDimension.y;
        pCL->RSSetScissorRects(1, &scissorRect);

        // Set and clear RT
        D3D12_CPU_DESCRIPTOR_HANDLE hRT(pRTHeap->GetCPUDescriptorHandleForHeapStart());
        hRT.ptr += pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * currentRTVIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE hDS(pDSVHeap->GetCPUDescriptorHandleForHeapStart());
        pCL->OMSetRenderTargets(1, &hRT, TRUE, &hDS);
        FLOAT clearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        pCL->ClearRenderTargetView(hRT, clearColor, 0, NULL);
        pCL->ClearDepthStencilView(hDS, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

        // Set VB/IB
        pCL->IASetVertexBuffers(0, 1, &mesh.vbView);
        pCL->IASetIndexBuffer(&mesh.ibView);

        pCL->IASetPrimitiveTopology(mesh.topology);
        AddBarrierTransition(pCL, pTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCL->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        AddBarrierTransition(pCL, pTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

        pCL->Close();

        SubmitCL(gfxCommandQueue, submission);

        pSwapChain->Present(0, 0);

        currentRTVIndex = (currentRTVIndex + 1) % 2;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  WndProc
//    Win32 event procedure
///////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static glm::ivec2 pointerPos;

    switch (message)
    {
    case WM_MOUSEWHEEL:
        if (HIWORD(wParam) == WHEEL_DELTA)
        {
            cameraPos.z -= 0.1f;
        }
        else
        {
            cameraPos.z += 0.1f;
        }
        break;

    case WM_MOUSEMOVE:
        {
            if (wParam & MK_LBUTTON)
            {
                glm::ivec2 deltaPos(LOWORD(lParam) - pointerPos.x, HIWORD(lParam) - pointerPos.y);
                eulerAngle.y += deltaPos.x * 0.2f;
                eulerAngle.x += deltaPos.y * 0.2f;
            }

            pointerPos.x = LOWORD(lParam);
            pointerPos.y = HIWORD(lParam);
        }
        break;

    case WM_SIZE:
        ResizeWindow(glm::ivec2(LOWORD(lParam), HIWORD(lParam)));
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 0;
}

void ResizeWindow(const glm::ivec2& newDimension)
{
    if (pD3DDevice)
    {
        // Wait for the GPU to complete operations.
        HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
        gfxCommandQueue.pFence->SetEventOnCompletion(gfxCommandQueue.fenceValue, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);

        // Resize/create the swapchain
        if (pSwapChain)
        {
            HRESULT hr = pSwapChain->ResizeBuffers(SWAP_CHAIN_SIZE, newDimension.x, newDimension.y, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
            if (FAILED(hr)) throw std::exception("Can't recreate swapchains on resize.");
        }
        else
        {
            // Create the swap chain
            IDXGIFactory* pDXGIFactory = nullptr;
            if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&pDXGIFactory))))
            {
                throw std::exception("** Can't get the DXGI factory\n");
            }

            // Swap chain definition
            DXGI_SWAP_CHAIN_DESC sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = 2;
            sd.BufferDesc.Width = newDimension.x;
            sd.BufferDesc.Height = newDimension.y;
            sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BufferDesc.RefreshRate.Numerator = 0;
            sd.BufferDesc.RefreshRate.Denominator = 0;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hWnd;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Windowed = TRUE;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

            if (FAILED(pDXGIFactory->CreateSwapChain(gfxCommandQueue.pCommandQueue, &sd, &pSwapChain)))
            {
                throw std::exception("** Can't create swap chain\n");
            }
            pDXGIFactory->Release();
        }

        if (pDepthStencil) pDepthStencil->Release();
        D3D12_HEAP_PROPERTIES  heapProperties;
        heapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask     = 0;
        heapProperties.VisibleNodeMask      = 0;
        D3D12_RESOURCE_DESC  resourceDesc;
        resourceDesc.Dimension              = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment              = 0;
        resourceDesc.Width                  = newDimension.x;
        resourceDesc.Height                 = newDimension.y;
        resourceDesc.DepthOrArraySize       = 1;
        resourceDesc.MipLevels              = 1;
        resourceDesc.Format                 = DXGI_FORMAT_D32_FLOAT;
        resourceDesc.SampleDesc.Count       = 1;
        resourceDesc.SampleDesc.Quality     = 0;
        resourceDesc.Layout                 = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags                  = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (FAILED(pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, NULL, IID_PPV_ARGS(&pDepthStencil))))
        {
            throw std::exception("** Can't create the texture heap\n");
        }

            // Create the render target view
        if (FAILED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pSwapChainRT))))
        {
            throw std::exception("** Can't get back buffer\n");
        }
        D3D12_RENDER_TARGET_VIEW_DESC rtView;
        rtView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtView.Texture2D.MipSlice = 0;
        rtView.Texture2D.PlaneSlice = 0;
        pD3DDevice->CreateRenderTargetView(pSwapChainRT, &rtView, pRTHeap->GetCPUDescriptorHandleForHeapStart());

        pSwapChainRT->Release();
        if (FAILED(pSwapChain->GetBuffer(1, IID_PPV_ARGS(&pSwapChainRT))))
        {
            throw std::exception("** Can't get back buffer\n");
        }
        
        D3D12_CPU_DESCRIPTOR_HANDLE h(pRTHeap->GetCPUDescriptorHandleForHeapStart());
        h.ptr += pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        pD3DDevice->CreateRenderTargetView(pSwapChainRT, &rtView, h);
        pSwapChainRT->Release();
        pSwapChainRT = nullptr;

        // Create the depth stencil view
        D3D12_DEPTH_STENCIL_VIEW_DESC dsView;
        dsView.Format             = DXGI_FORMAT_D32_FLOAT;
        dsView.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsView.Flags              = D3D12_DSV_FLAG_NONE;
        dsView.Texture2D.MipSlice = 0;
        pD3DDevice->CreateDepthStencilView(pDepthStencil, &dsView, pDSVHeap->GetCPUDescriptorHandleForHeapStart());

        currentRTVIndex = 0;
        windowDimension = newDimension;
    }
}

Mesh* CreateMesh()
{
    Mesh* pNewMesh = new Mesh;
    if (pNewMesh) throw std::exception("Can't allocate mesh object.");

    return pNewMesh;
}

void DestroyGlobalObjects()
{
    while (!gfxCommandQueue.runningCL.empty())
    {
        gfxCommandQueue.runningCL.front().pCL->Release();
        gfxCommandQueue.runningCL.front().pCA->Release();
        gfxCommandQueue.runningCL.pop();
    }

    pVbIbSubAllocator.release();
    if (pDSVHeap)          pDSVHeap->Release();
    if (pRTHeap)           pRTHeap->Release();
    if (pCbvSrvUavHeap)    pCbvSrvUavHeap->Release();
    if (pTexture)          pTexture->Release();
    if (pUploadHeap)       pUploadHeap->Release();
    if (pGraphicsPSO)      pGraphicsPSO->Release();
    if (pRootSignature)    pRootSignature->Release();
    if (pSwapChainRT)      pSwapChainRT->Release();
    if (gfxCommandQueue.pFence)         gfxCommandQueue.pFence->Release();
    if (gfxCommandQueue.pCommandQueue)  gfxCommandQueue.pCommandQueue->Release();
    if (pD3DDevice)        pD3DDevice->Release();

    if (hWnd)
    {
        UnregisterClass("d3d12_base", (HINSTANCE)GetModuleHandle(NULL));
        DestroyWindow(hWnd);
    }
}