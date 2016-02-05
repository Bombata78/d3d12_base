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
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/type_ptr.hpp"
#include "glm/glm/gtx/transform.hpp"
#include "common/utils.h"
#include "common/heap.h"
#include "common/lib/d3d12/d3d12_utils.h"
#include "common/lib/core/mesh.h"
#include "common/lib/io/format/obj.h"
#include "common/lib/utils/trackball.h"

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
    D3D12_VERTEX_BUFFER_VIEW   vbView;
    D3D12_INDEX_BUFFER_VIEW    ibView;
    std::uint32_t              indexCount;
    D3D12_PRIMITIVE_TOPOLOGY   topology;
    std::vector<mesh::attribute> attributes;
};

struct Material
{
    ~Material()
    {
        if (pRootSignature) pRootSignature->Release();
        if (pVsBlob)        pVsBlob->Release();
        if (pPsBlob)        pPsBlob->Release();
        if (pHsBlob)        pHsBlob->Release();
        if (pDsBlob)        pDsBlob->Release();
        if (pGsBlob)        pGsBlob->Release();
    }
    Material()
        : pRootSignature(nullptr), pVsBlob(nullptr), pPsBlob(nullptr), pHsBlob(nullptr),
          pDsBlob(nullptr), pGsBlob(nullptr)
    {
        
    }
    Material(Material&& rValue)
    {
        pRootSignature = rValue.pRootSignature;
        pVsBlob             = rValue.pVsBlob;
        pPsBlob             = rValue.pPsBlob;
        pHsBlob             = rValue.pHsBlob;
        pDsBlob             = rValue.pDsBlob;
        pGsBlob             = rValue.pGsBlob;

        rValue.pRootSignature = nullptr;
        rValue.pVsBlob        = nullptr;
        rValue.pPsBlob        = nullptr;
        rValue.pHsBlob        = nullptr;
        rValue.pDsBlob        = nullptr;
        rValue.pGsBlob        = nullptr;
    }

    ID3D12RootSignature*  pRootSignature;
    ID3DBlob*             pVsBlob;
    ID3DBlob*             pPsBlob;
    ID3DBlob*             pHsBlob;
    ID3DBlob*             pDsBlob;
    ID3DBlob*             pGsBlob;
};

struct Primitive
{
    Primitive() = delete;
    Primitive(Mesh* pMesh_, Material* pMaterial_)
        : pMesh(pMesh_), pMaterial(pMaterial_) {}

    Mesh*     pMesh;
    Material* pMaterial;
    glm::vec3 position;
    glm::quat orientation;
};

struct RenderPass
{
};

struct RasterPass : public RenderPass
{
    ~RasterPass()
    {
        if (pPSO) pPSO->Release();
    }

    std::vector<Primitive*> primitives;
    ID3D12PipelineState*    pPSO;
};

struct ComputePass : public RenderPass
{
    ID3D12RootSignature* pRootSignature;
    ID3D12PipelineState* pPSO;
};

struct Film
{
    ~Film()
    {
        if (pSwapChain) pSwapChain->Release();
    }

    glm::ivec2      dimension;
    DXGI_FORMAT     format;
    IDXGISwapChain* pSwapChain = nullptr;
    std::uint8_t    currentRTVIndex   = 0;
};

struct Scene
{
    static const unsigned int SWAP_CHAIN_SIZE = 2;

    ~Scene()
    {
        aPrimitive.clear();
        aMesh.clear();
        aMaterial.clear();
        pTexture->Release();
    }

    static const std::size_t  VB_IB_SUB_ALLOCATOR_SIZE = 16 * 1024 * 1024; // 16 MB of geometry
    using VbIbBufferSubAllocator = BufferSubAllocator<D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>;
    std::unique_ptr<VbIbBufferSubAllocator> pVbIbSubAllocator;

    std::vector<Mesh>      aMesh;
    std::vector<Material>  aMaterial;
    std::vector<Primitive> aPrimitive;

    ID3D12Resource*        pTexture          = nullptr;
};

struct Direct3D12Integrator
{
    static Direct3D12Integrator* create();

    ~Direct3D12Integrator()
    {
        delete pScene;
        pCbvSrvUavHeap->Release();
        pRTHeap->Release();
        pDSVHeap->Release();
        pDepthStencil->Release();
        pUploadHeap->Release();
        pD3DDevice->Release();
    }
    std::vector<RenderPass> nodes;

    ID3D12DescriptorHeap*  pCbvSrvUavHeap    = nullptr;
    ID3D12DescriptorHeap*  pRTHeap           = nullptr;
    ID3D12DescriptorHeap*  pDSVHeap          = nullptr;
    ID3D12Resource*        pDepthStencil     = nullptr;
    ID3D12Resource*        pUploadHeap       = nullptr;

    CommandQueueData       gfxCommandQueue;
    ID3D12Device*          pD3DDevice        = nullptr;

    Scene*                 pScene;
    Film                   film;
};

HWND                                  hWnd;
std::unique_ptr<Direct3D12Integrator> g_pIntegrator;

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

Mesh                  CreateMeshFromObj(const std::string& filename);
Material              CreateDefaultMaterial();
void CreateScene();

///////////////////////////////////////////////////////////////////////////////////////////////////
//  wWinMain
//    Program entry point
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "io/format/image.h"
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

    try
    {
        // Create the scene object
        g_pIntegrator = std::make_unique<Direct3D12Integrator>();
        g_pIntegrator->pScene = new Scene();
        g_pIntegrator->film.dimension = glm::ivec2(1024, 768);

        CreateScene();

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
    g_pIntegrator->gfxCommandQueue.pFence->SetEventOnCompletion(g_pIntegrator->gfxCommandQueue.fenceValue, waitEvent);
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
    RECT rc = { 0, 0, g_pIntegrator->film.dimension.x, g_pIntegrator->film.dimension.y };
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
    // Allocate a single render pass
    g_pIntegrator->nodes.resize(1);

    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pIntegrator->pD3DDevice))))
    {
        throw std::exception("** Can't create D3D12 device\n");
    }

    // Create a graphics command queue
    D3D12_COMMAND_QUEUE_DESC desc;
    desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = 0;
    desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;
    if (FAILED(g_pIntegrator->pD3DDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pIntegrator->gfxCommandQueue.pCommandQueue))))
    {
        throw std::exception("** Can't create D3D12 gfx queue\n");
    }

    // Create the tracking fence
    if (FAILED(g_pIntegrator->pD3DDevice->CreateFence(g_pIntegrator->gfxCommandQueue.fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_pIntegrator->gfxCommandQueue.pFence))))
    {
        throw std::exception("** Can't create D3D12 gfx fence\n");
    }

    // Create the VB/IB sub allocator
    g_pIntegrator->pScene->pVbIbSubAllocator = std::make_unique<Scene::VbIbBufferSubAllocator>(g_pIntegrator->pD3DDevice, Scene::VB_IB_SUB_ALLOCATOR_SIZE);

    // Create the mesh
    g_pIntegrator->pScene->aMesh.push_back(CreateMeshFromObj("../../../assets/models/misc/box.obj"));

    // Create default material
    g_pIntegrator->pScene->aMaterial.push_back(CreateDefaultMaterial());

    // Create the defaul primitive
    g_pIntegrator->pScene->aPrimitive.push_back(Primitive(&g_pIntegrator->pScene->aMesh[0], &g_pIntegrator->pScene->aMaterial[0]));

    // ----- Create the descriptor heap -----
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask       = 0;
    g_pIntegrator->pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_pIntegrator->pCbvSrvUavHeap));
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 2;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask       = 0;
    g_pIntegrator->pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_pIntegrator->pRTHeap));
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask       = 0;
    g_pIntegrator->pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_pIntegrator->pDSVHeap));

    // ----- Create the graphics PSO -----
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gfxPSO;
    memset(&gfxPSO, 0, sizeof(gfxPSO));
    gfxPSO.pRootSignature = g_pIntegrator->pScene->aMaterial[0].pRootSignature;
    gfxPSO.VS.pShaderBytecode = g_pIntegrator->pScene->aMaterial[0].pVsBlob->GetBufferPointer();
    gfxPSO.VS.BytecodeLength  = g_pIntegrator->pScene->aMaterial[0].pVsBlob->GetBufferSize();
    gfxPSO.PS.pShaderBytecode = g_pIntegrator->pScene->aMaterial[0].pPsBlob->GetBufferPointer();
    gfxPSO.PS.BytecodeLength  = g_pIntegrator->pScene->aMaterial[0].pPsBlob->GetBufferSize();

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
    assert(g_pIntegrator->pScene->aMesh[0].attributes.size() == 3);
    D3D12_INPUT_ELEMENT_DESC iaDesc[3];
    gfxPSO.InputLayout.NumElements = 0;
    gfxPSO.InputLayout.pInputElementDescs = iaDesc;
    for (const mesh::attribute& attribute : g_pIntegrator->pScene->aMesh[0].attributes)
    {
        switch (attribute.semantic)
        {
        case mesh::attribute::position:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "POSITION";
            break;

        case mesh::attribute::normal:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "NORMAL";
            break;

        case mesh::attribute::texcoord:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "TEXCOORD";
            break;
        }

        iaDesc[gfxPSO.InputLayout.NumElements].SemanticIndex        = 0;
        iaDesc[gfxPSO.InputLayout.NumElements].Format               = d3d12::core_to_dxgi_format(attribute.format);
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
    gfxPSO.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    gfxPSO.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    gfxPSO.SampleDesc.Count      = 1;
    gfxPSO.SampleDesc.Quality    = 0;
    gfxPSO.NodeMask              = 0;
    gfxPSO.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

    if (FAILED(g_pIntegrator->pD3DDevice->CreateGraphicsPipelineState(&gfxPSO, IID_PPV_ARGS(&static_cast<RasterPass&>(g_pIntegrator->nodes[0]).pPSO))))
    {
        throw std::exception("** Can't create graphics PSO\n");
    }

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
    resourceDesc.Width                  = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * 1024 * 1024;
    resourceDesc.Height                 = 1;
    resourceDesc.DepthOrArraySize       = 1;
    resourceDesc.MipLevels              = 1;
    resourceDesc.Format                 = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count       = 1;
    resourceDesc.SampleDesc.Quality     = 0;
    resourceDesc.Layout                 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags                  = D3D12_RESOURCE_FLAG_NONE;
    if (FAILED(g_pIntegrator->pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&g_pIntegrator->pUploadHeap))))
    {
        throw std::exception("** Can't cb heap\n");
    }

    std::unique_ptr<texture> pLoadedTexture{io::create_texture_from_file("../tex.png")};
    heapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask     = 0;
    heapProperties.VisibleNodeMask      = 0;
    resourceDesc.Dimension              = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment              = 0;
    resourceDesc.Width                  = pLoadedTexture->width();
    resourceDesc.Height                 = pLoadedTexture->height();
    resourceDesc.DepthOrArraySize       = 1;
    resourceDesc.MipLevels              = 1;
    resourceDesc.Format                 = DXGI_FORMAT_R8G8B8A8_UNORM;
    resourceDesc.SampleDesc.Count       = 1;
    resourceDesc.SampleDesc.Quality     = 0;
    resourceDesc.Layout                 = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags                  = D3D12_RESOURCE_FLAG_NONE;
    if (FAILED(g_pIntegrator->pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&g_pIntegrator->pScene->pTexture))))
    {
        throw std::exception("** Can't create the texture heap\n");
    }

    // Initialize texture content
    {
        std::uint8_t* pTexData;
        g_pIntegrator->pUploadHeap->Map(0, NULL, reinterpret_cast<void**>(&pTexData));
        std::uint8_t* pSrc = pLoadedTexture->data();
        for (std::uint32_t line = 0; line < pLoadedTexture->height(); ++line)
        {
            std::size_t size        = pLoadedTexture->width() * 4;
            std::size_t alignedSize = AlignTo(size, static_cast<std::size_t>(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
            memcpy(pTexData, pSrc, size);
            pSrc += size;
            pTexData += alignedSize;
        }
        g_pIntegrator->pUploadHeap->Unmap(0, NULL);

        CommandListSubmission submission = GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE_DIRECT);
        AddBarrierTransition(static_cast<ID3D12GraphicsCommandList*>(submission.pCL), g_pIntegrator->pScene->pTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        CopyTexture<true>(submission.pCL,  g_pIntegrator->pUploadHeap, g_pIntegrator->pScene->pTexture, 0, 0);
        AddBarrierTransition(static_cast<ID3D12GraphicsCommandList*>(submission.pCL), g_pIntegrator->pScene->pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        static_cast<ID3D12GraphicsCommandList*>(submission.pCL)->Close();
        SubmitCL(g_pIntegrator->gfxCommandQueue, submission);

        // Create the SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip     = 0;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.PlaneSlice          = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        g_pIntegrator->pD3DDevice->CreateShaderResourceView(g_pIntegrator->pScene->pTexture, &srvDesc, g_pIntegrator->pCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
    }

    ResizeWindow(g_pIntegrator->film.dimension);

    return S_OK;
}

CommandListSubmission GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE type)
{
    std::queue<CommandListSubmission>* pQueue      = nullptr;
    ID3D12Fence*                       pCQFence    = nullptr;

    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        pQueue   = &g_pIntegrator->gfxCommandQueue.runningCL;
        pCQFence = g_pIntegrator->gfxCommandQueue.pFence;
    }

    if (pQueue->empty() || pQueue->front().fence > pCQFence->GetCompletedValue())
    {
        ID3D12CommandAllocator* pCA = nullptr;
        if (FAILED(g_pIntegrator->pD3DDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCA))))
        {
            throw std::exception("** Can't create command allocator\n");
        }
        ID3D12GraphicsCommandList* pCL = nullptr;
        if (FAILED(g_pIntegrator->pD3DDevice->CreateCommandList(0, type, pCA, NULL, IID_PPV_ARGS(&pCL))))
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

        if (FAILED(g_pIntegrator->pD3DDevice->CreateCommandList(0, type, cl.pCA, NULL, IID_PPV_ARGS(&cl.pCL))))
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
    cq.pCommandQueue->Signal(g_pIntegrator->gfxCommandQueue.pFence, ++cq.fenceValue);
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
glm::vec3 cameraPos(0.0f, 0.0f, 5.0f);
void Render()
{
    // Submit graphics work
    {
        CommandListSubmission submission = GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE_DIRECT);
        ID3D12GraphicsCommandList* pCL = static_cast<ID3D12GraphicsCommandList*>(submission.pCL);

        // Initialize descriptor heaps
        pCL->SetPipelineState(static_cast<RasterPass&>(g_pIntegrator->nodes[0]).pPSO);
        pCL->SetGraphicsRootSignature(g_pIntegrator->pScene->aMaterial[0].pRootSignature);
        pCL->SetDescriptorHeaps(1, &g_pIntegrator->pCbvSrvUavHeap);
        pCL->SetGraphicsRootDescriptorTable(1, g_pIntegrator->pCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

        // Set root constant buffer
        //glm::mat4 rotationMatrix = g_pIntegrator->pScene->aPrimitive[0].rotation;
        glm::mat4 rotationMatrix = glm::mat4_cast(g_pIntegrator->pScene->aPrimitive[0].orientation);
        //glm::mat4 rotationMatrix      = glm::rotate(g_pIntegrator->pScene->aPrimitive[0].eulerAngles.x, 1.0f, 0.0f, 0.0f);
        //rotationMatrix               *= glm::rotate(g_pIntegrator->pScene->aPrimitive[0].eulerAngles.y, 0.0f, 1.0f, 0.0f);
        glm::mat4 viewTransform       = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 perspectiveTranform;
        Perspective(&perspectiveTranform, 50.0f, static_cast<float>(g_pIntegrator->film.dimension.x)/static_cast<float>(g_pIntegrator->film.dimension.y), 0.2f, 10.0f);
        perspectiveTranform = perspectiveTranform * viewTransform * rotationMatrix;

        pCL->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(perspectiveTranform), 0);
        pCL->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(rotationMatrix), 16);

        // Set VP and scissors
        D3D12_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width = static_cast<float>(g_pIntegrator->film.dimension.x);
        vp.Height = static_cast<float>(g_pIntegrator->film.dimension.y);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        pCL->RSSetViewports(1, &vp);
        D3D12_RECT scissorRect;
        scissorRect.left = 0;
        scissorRect.top = 0;
        scissorRect.right = g_pIntegrator->film.dimension.x;
        scissorRect.bottom = g_pIntegrator->film.dimension.y;
        pCL->RSSetScissorRects(1, &scissorRect);

        // Set and clear RT
        D3D12_CPU_DESCRIPTOR_HANDLE hRT(g_pIntegrator->pRTHeap->GetCPUDescriptorHandleForHeapStart());
        hRT.ptr += g_pIntegrator->pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * g_pIntegrator->film.currentRTVIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE hDS(g_pIntegrator->pDSVHeap->GetCPUDescriptorHandleForHeapStart());
        pCL->OMSetRenderTargets(1, &hRT, TRUE, &hDS);
        FLOAT clearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        pCL->ClearRenderTargetView(hRT, clearColor, 0, NULL);
        pCL->ClearDepthStencilView(hDS, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

        // Set VB/IB
        pCL->IASetVertexBuffers(0, 1, &g_pIntegrator->pScene->aMesh[0].vbView);
        pCL->IASetIndexBuffer(&g_pIntegrator->pScene->aMesh[0].ibView);

        pCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        AddBarrierTransition(pCL, g_pIntegrator->pScene->pTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCL->DrawIndexedInstanced(g_pIntegrator->pScene->aMesh[0].indexCount, 1, 0, 0, 0);
        AddBarrierTransition(pCL, g_pIntegrator->pScene->pTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

        pCL->Close();

        SubmitCL(g_pIntegrator->gfxCommandQueue, submission);

        g_pIntegrator->film.pSwapChain->Present(0, 0);

        g_pIntegrator->film.currentRTVIndex = (g_pIntegrator->film.currentRTVIndex + 1) % 2;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  WndProc
//    Win32 event procedure
///////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static glm::vec2 pointerDownPos;

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
                glm::vec2 pointerUpPos = glm::vec2(LOWORD(lParam)/static_cast<float>(g_pIntegrator->film.dimension.x), -HIWORD(lParam)/static_cast<float>(g_pIntegrator->film.dimension.y));
                g_pIntegrator->pScene->aPrimitive[0].orientation = glm::normalize(utils::trackball(0.9f, pointerDownPos, pointerUpPos) * g_pIntegrator->pScene->aPrimitive[0].orientation);
                pointerDownPos = pointerUpPos;
            }
        }
        break;

    case WM_LBUTTONDOWN:
        {
            if (wParam & MK_LBUTTON)
            {
                pointerDownPos = glm::vec2(LOWORD(lParam)/static_cast<float>(g_pIntegrator->film.dimension.x) , -HIWORD(lParam)/static_cast<float>(g_pIntegrator->film.dimension.y));
            }
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
    if (g_pIntegrator && g_pIntegrator->pD3DDevice)
    {
        // Wait for the GPU to complete operations.
        HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
        g_pIntegrator->gfxCommandQueue.pFence->SetEventOnCompletion(g_pIntegrator->gfxCommandQueue.fenceValue, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);

        // Resize/create the swapchain
        if (g_pIntegrator->film.pSwapChain)
        {
            HRESULT hr = g_pIntegrator->film.pSwapChain->ResizeBuffers(Scene::SWAP_CHAIN_SIZE, newDimension.x, newDimension.y, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
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

            if (FAILED(pDXGIFactory->CreateSwapChain(g_pIntegrator->gfxCommandQueue.pCommandQueue, &sd, &g_pIntegrator->film.pSwapChain)))
            {
                throw std::exception("** Can't create swap chain\n");
            }
            pDXGIFactory->Release();
        }

        if (g_pIntegrator->pDepthStencil) g_pIntegrator->pDepthStencil->Release();
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
        D3D12_CLEAR_VALUE clearValue;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        clearValue.Format               = DXGI_FORMAT_D32_FLOAT;
        if (FAILED(g_pIntegrator->pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&g_pIntegrator->pDepthStencil))))
        {
            throw std::exception("** Can't create the texture heap\n");
        }

            // Create the render target view
        ID3D12Resource* pSwapChainRT;
        if (FAILED(g_pIntegrator->film.pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pSwapChainRT))))
        {
            throw std::exception("** Can't get back buffer\n");
        }
        D3D12_RENDER_TARGET_VIEW_DESC rtView;
        rtView.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtView.Texture2D.MipSlice = 0;
        rtView.Texture2D.PlaneSlice = 0;
        g_pIntegrator->pD3DDevice->CreateRenderTargetView(pSwapChainRT, &rtView, g_pIntegrator->pRTHeap->GetCPUDescriptorHandleForHeapStart());

        pSwapChainRT->Release();
        if (FAILED(g_pIntegrator->film.pSwapChain->GetBuffer(1, IID_PPV_ARGS(&pSwapChainRT))))
        {
            throw std::exception("** Can't get back buffer\n");
        }
        
        D3D12_CPU_DESCRIPTOR_HANDLE h(g_pIntegrator->pRTHeap->GetCPUDescriptorHandleForHeapStart());
        h.ptr += g_pIntegrator->pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        g_pIntegrator->pD3DDevice->CreateRenderTargetView(pSwapChainRT, &rtView, h);
        pSwapChainRT->Release();
        pSwapChainRT = nullptr;

        // Create the depth stencil view
        D3D12_DEPTH_STENCIL_VIEW_DESC dsView;
        dsView.Format             = DXGI_FORMAT_D32_FLOAT;
        dsView.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsView.Flags              = D3D12_DSV_FLAG_NONE;
        dsView.Texture2D.MipSlice = 0;
        g_pIntegrator->pD3DDevice->CreateDepthStencilView(g_pIntegrator->pDepthStencil, &dsView, g_pIntegrator->pDSVHeap->GetCPUDescriptorHandleForHeapStart());

        g_pIntegrator->film.currentRTVIndex = 0;
        g_pIntegrator->film.dimension = newDimension;
    }
}

Mesh CreateMeshFromObj(const std::string& filename)
{
    // ----- Load the base mesh -----
    std::unique_ptr<mesh> pIoMesh{io::create_mesh_from_obj(filename, true)};
    Mesh mesh;

    // Allocate and initialize the vertex buffer
    BufferSubAllocation vbSubAllocation = g_pIntegrator->pScene->pVbIbSubAllocator->subAllocate(pIoMesh->vertex_buffer().size());
    mesh.vbView.BufferLocation = vbSubAllocation.pResource->GetGPUVirtualAddress() + vbSubAllocation.offset;
    mesh.vbView.StrideInBytes  = static_cast<UINT>(pIoMesh->vertex_buffer().stride());
    mesh.vbView.SizeInBytes    = static_cast<UINT>(pIoMesh->vertex_buffer().size());
    void* pMappedResource;
    if (FAILED(vbSubAllocation.pResource->Map(0, NULL, reinterpret_cast<void**>(&pMappedResource)))) throw std::exception("** Can't map constant buffer\n");
    memcpy(static_cast<std::uint8_t*>(pMappedResource) + vbSubAllocation.offset, pIoMesh->vertex_buffer().data(), pIoMesh->vertex_buffer().size());
    vbSubAllocation.pResource->Unmap(0, NULL);

    // Allocate and initialize the index buffer
    BufferSubAllocation ibSubAllocation = g_pIntegrator->pScene->pVbIbSubAllocator->subAllocate(pIoMesh->index_buffer().size());
    mesh.ibView.BufferLocation = ibSubAllocation.pResource->GetGPUVirtualAddress() + ibSubAllocation.offset;
    mesh.ibView.Format         = d3d12::core_to_dxgi_format(pIoMesh->index_buffer().format());
    mesh.ibView.SizeInBytes    = static_cast<UINT>(pIoMesh->index_buffer().size());
    mesh.indexCount            = static_cast<std::uint32_t>(mesh.ibView.SizeInBytes/GetByteStrideFromFormat(mesh.ibView.Format));
    mesh.topology              = d3d12::core_to_d3d_topology(pIoMesh->topology());
    if (FAILED(ibSubAllocation.pResource->Map(0, NULL, reinterpret_cast<void**>(&pMappedResource)))) throw std::exception("** Can't map constant buffer\n");
    memcpy(static_cast<std::uint8_t*>(pMappedResource) + ibSubAllocation.offset, pIoMesh->index_buffer().data(), pIoMesh->index_buffer().size());
    vbSubAllocation.pResource->Unmap(0, NULL);

    mesh.attributes = pIoMesh->attributes();

    return mesh;
}

Material CreateDefaultMaterial()
{
    Material m;
    memset(&m, 0, sizeof(m));

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
    samplerDesc.Filter           = D3D12_FILTER_ANISOTROPIC;
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

        if (FAILED(g_pIntegrator->pD3DDevice->CreateRootSignature(0, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(&m.pRootSignature))))
        {
            throw std::exception("** Can't create root signature\n");
        }
        pBlob->Release();
    }

    // Compile the vertex shader
    {
        // Load vertex shader code
        std::string shaderSource((std::istreambuf_iterator<char>(std::ifstream("shaders/base.vs"))), std::istreambuf_iterator<char>());

        // Compile shader
        ID3DBlob* pErrors = NULL;
        if (FAILED(D3DCompile(shaderSource.c_str(), shaderSource.length(), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &m.pVsBlob, &pErrors)) || pErrors)
        {
            char* d = (char*)pErrors->GetBufferPointer();
            pErrors->Release();
            throw std::exception("** Can't compile VS\n");
        }
    }

    // Compile the pixel shader
    {
        // Load pixel shader code
        std::string shaderSource((std::istreambuf_iterator<char>(std::ifstream("shaders/base.ps"))), std::istreambuf_iterator<char>());

        // Compile shader
        ID3DBlob* pErrors = NULL;
        if (FAILED(D3DCompile(shaderSource.c_str(), shaderSource.length(), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &m.pPsBlob, &pErrors)) || pErrors)
        {
            char* d = (char*)pErrors->GetBufferPointer();
            pErrors->Release();
            throw std::exception("** Can't compile PS\n");
        }
    }

    return m;
}

void DestroyGlobalObjects()
{
    while (!g_pIntegrator->gfxCommandQueue.runningCL.empty())
    {
        g_pIntegrator->gfxCommandQueue.runningCL.front().pCL->Release();
        g_pIntegrator->gfxCommandQueue.runningCL.front().pCA->Release();
        g_pIntegrator->gfxCommandQueue.runningCL.pop();
    }

    if (g_pIntegrator->gfxCommandQueue.pFence)         g_pIntegrator->gfxCommandQueue.pFence->Release();
    if (g_pIntegrator->gfxCommandQueue.pCommandQueue)  g_pIntegrator->gfxCommandQueue.pCommandQueue->Release();

    if (hWnd)
    {
        UnregisterClass("d3d12_base", (HINSTANCE)GetModuleHandle(NULL));
        DestroyWindow(hWnd);
    }
}

Direct3D12Integrator* Direct3D12Integrator::create()
{
    Direct3D12Integrator* pIntegrator = new Direct3D12Integrator();

    // Allocate a single render pass
    pIntegrator->nodes.resize(1);

    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pIntegrator->pD3DDevice))))
    {
        throw std::exception("** Can't create D3D12 device\n");
    }

    // Create a graphics command queue
    D3D12_COMMAND_QUEUE_DESC desc;
    desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = 0;
    desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;
    if (FAILED(pIntegrator->pD3DDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pIntegrator->gfxCommandQueue.pCommandQueue))))
    {
        throw std::exception("** Can't create D3D12 gfx queue\n");
    }

    // Create the tracking fence
    if (FAILED(pIntegrator->pD3DDevice->CreateFence(pIntegrator->gfxCommandQueue.fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIntegrator->gfxCommandQueue.pFence))))
    {
        throw std::exception("** Can't create D3D12 gfx fence\n");
    }

    // ----- Create the descriptor heap -----
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask       = 0;
    pIntegrator->pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pIntegrator->pCbvSrvUavHeap));
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 2;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask       = 0;
    pIntegrator->pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pIntegrator->pRTHeap));
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask       = 0;
    pIntegrator->pD3DDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pIntegrator->pDSVHeap));

    // Create the VB/IB sub allocator
    g_pIntegrator->pScene->pVbIbSubAllocator = std::make_unique<Scene::VbIbBufferSubAllocator>(pIntegrator->pD3DDevice, Scene::VB_IB_SUB_ALLOCATOR_SIZE);

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
    if (FAILED(pIntegrator->pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&pIntegrator->pUploadHeap))))
    {
        throw std::exception("** Can't cb heap\n");
    }

    // -- LOAD THE SCENE --
    // Create the mesh
    g_pIntegrator->pScene->aMesh.push_back(CreateMeshFromObj("../../../assets/models/misc/box.obj"));

    // Create default material
    g_pIntegrator->pScene->aMaterial.push_back(CreateDefaultMaterial());

    // Create the defaul primitive
    g_pIntegrator->pScene->aPrimitive.push_back(Primitive(&g_pIntegrator->pScene->aMesh[0], &g_pIntegrator->pScene->aMaterial[0]));

    // ----- Create the graphics PSO -----
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gfxPSO;
    memset(&gfxPSO, 0, sizeof(gfxPSO));
    gfxPSO.pRootSignature = g_pIntegrator->pScene->aMaterial[0].pRootSignature;
    gfxPSO.VS.pShaderBytecode = g_pIntegrator->pScene->aMaterial[0].pVsBlob->GetBufferPointer();
    gfxPSO.VS.BytecodeLength  = g_pIntegrator->pScene->aMaterial[0].pVsBlob->GetBufferSize();
    gfxPSO.PS.pShaderBytecode = g_pIntegrator->pScene->aMaterial[0].pPsBlob->GetBufferPointer();
    gfxPSO.PS.BytecodeLength  = g_pIntegrator->pScene->aMaterial[0].pPsBlob->GetBufferSize();

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
    assert(g_pIntegrator->pScene->aMesh[0].attributes.size() == 3);
    D3D12_INPUT_ELEMENT_DESC iaDesc[3];
    gfxPSO.InputLayout.NumElements = 0;
    gfxPSO.InputLayout.pInputElementDescs = iaDesc;
    for (const mesh::attribute& attribute : g_pIntegrator->pScene->aMesh[0].attributes)
    {
        switch (attribute.semantic)
        {
        case mesh::attribute::position:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "POSITION";
            break;

        case mesh::attribute::normal:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "NORMAL";
            break;

        case mesh::attribute::texcoord:
            iaDesc[gfxPSO.InputLayout.NumElements].SemanticName = "TEXCOORD";
            break;
        }

        iaDesc[gfxPSO.InputLayout.NumElements].SemanticIndex        = 0;
        iaDesc[gfxPSO.InputLayout.NumElements].Format               = d3d12::core_to_dxgi_format(attribute.format);
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

    if (FAILED(pIntegrator->pD3DDevice->CreateGraphicsPipelineState(&gfxPSO, IID_PPV_ARGS(&static_cast<RasterPass&>(pIntegrator->nodes[0]).pPSO))))
    {
        throw std::exception("** Can't create graphics PSO\n");
    }

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
    if (FAILED(pIntegrator->pD3DDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&g_pIntegrator->pScene->pTexture))))
    {
        throw std::exception("** Can't create the texture heap\n");
    }

    // Initialize texture content
    {
        std::uint32_t* pTexData;
        pIntegrator->pUploadHeap->Map(0, NULL, reinterpret_cast<void**>(&pTexData));
        pTexData[0] = 0xFFFFFFFF; pTexData[1] = 0x00000000; pTexData[2] = 0xFFFFFFFF; pTexData[3] = 0x00000000;
        pTexData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT/4;
        pTexData[0] = 0x00000000; pTexData[1] = 0xFFFFFFFF; pTexData[2] = 0x00000000; pTexData[3] = 0xFFFFFFFF;
        pTexData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT/4;
        pTexData[0] = 0xFFFFFFFF; pTexData[1] = 0x00000000; pTexData[2] = 0xFFFFFFFF; pTexData[3] = 0x00000000;
        pTexData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT/4;
        pTexData[0] = 0x00000000; pTexData[1] = 0xFFFFFFFF; pTexData[2] = 0x00000000; pTexData[3] = 0xFFFFFFFF;
        pIntegrator->pUploadHeap->Unmap(0, NULL);

        CommandListSubmission submission = GetAvailableCommandListSubmission(D3D12_COMMAND_LIST_TYPE_DIRECT);
        AddBarrierTransition(static_cast<ID3D12GraphicsCommandList*>(submission.pCL), g_pIntegrator->pScene->pTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        CopyTexture<true>(submission.pCL,  pIntegrator->pUploadHeap, g_pIntegrator->pScene->pTexture, 0, 0);
        AddBarrierTransition(static_cast<ID3D12GraphicsCommandList*>(submission.pCL), g_pIntegrator->pScene->pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        static_cast<ID3D12GraphicsCommandList*>(submission.pCL)->Close();
        SubmitCL(pIntegrator->gfxCommandQueue, submission);

        // Create the SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip     = 0;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.PlaneSlice          = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        pIntegrator->pD3DDevice->CreateShaderResourceView(g_pIntegrator->pScene->pTexture, &srvDesc, pIntegrator->pCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
    }

    ResizeWindow(g_pIntegrator->film.dimension);

    return pIntegrator;
}

// ----- TEST -- 
#include "common/lib/core/scene.h"
#include "common/lib/core/material.h"
#include "common/lib/core/primitive.h"
#include "common/lib/core/camera.h"
#include "common/lib/core/light.h"
std::unique_ptr<scene> pScene;
std::unique_ptr<camera> pCamera;
void CreateScene()
{
    // Create the scene object
    pScene = std::make_unique<scene>();

    // Add simple primitive to the scene
    {
        // Load mesh from obj
        std::shared_ptr<mesh> boxMesh{io::create_mesh_from_obj("../../../assets/models/misc/box.obj", true)};

        // Create a simple textured material
        std::shared_ptr<material> simpleMaterial = std::make_shared<material>(std::vector<bxdf>{bxdf::lambertian_reflection, bxdf::blinn_phong_reflection}, 
                                                                              std::shared_ptr<texture>{io::create_texture_from_file("../tex.png")},
                                                                              1.0f);

        pScene->add_primitive(std::make_shared<primitive>(boxMesh, simpleMaterial));
    }

    // Add a simple point light to the scene
    pScene->add_light(std::make_shared<light>(light::point, glm::vec3{0.0, 0.0, 5.0f}, glm::vec3{1.0f, 1.0f, 1.0f}, glm::vec3{0.0f, 0.0f, -1.0f}));

    // Create the camera object
    pCamera = std::make_unique<camera>(glm::vec3{0.0f, 0.0f, 5.0f}, glm::vec3{0.0f, 0.0f, -1.0f});
}