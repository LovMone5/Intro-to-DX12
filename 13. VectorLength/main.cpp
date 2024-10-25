#include "include/directx/d3dx12.h"
#include <fstream>
#include <wrl.h>
#include <iostream>
#include <assert.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include <random>
#include <format>
#include <comdef.h>

#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "DXGUID.lib")

// 设备相关
Microsoft::WRL::ComPtr<ID3D12CommandQueue> g_command_queue;
Microsoft::WRL::ComPtr<ID3D12Fence> g_fence;
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> g_command_allocator;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> g_command_list;
Microsoft::WRL::ComPtr<ID3D12RootSignature> g_root_signature;
Microsoft::WRL::ComPtr<ID3D12PipelineState> g_pso;
Microsoft::WRL::ComPtr<ID3D12Device> g_device;
Microsoft::WRL::ComPtr<ID3D12Debug> g_debug;
Microsoft::WRL::ComPtr<ID3DBlob> g_cs_shader; 
HRESULT g_hr;
UINT64 g_fence_val = 0;

// 数据相关
Microsoft::WRL::ComPtr<ID3D12Resource> g_input_default;
Microsoft::WRL::ComPtr<ID3D12Resource> g_input_upload;
Microsoft::WRL::ComPtr<ID3D12Resource> g_output_default;
Microsoft::WRL::ComPtr<ID3D12Resource> g_read_back;
const UINT32 data_count = 1000;
struct Data {
    DirectX::XMFLOAT3 v;
    FLOAT val;
    INT id;
};

void debug_message() {
    _com_error msg(g_hr);
    std::wcout << msg.ErrorMessage() << std::endl;
}

void create_input_buffer_by_uploader(const VOID* buffer, UINT size) {
    g_hr = g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&g_input_default)
    );
    assert(SUCCEEDED(g_hr));

    g_hr = g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_input_upload)
    );
    assert(SUCCEEDED(g_hr));

    D3D12_SUBRESOURCE_DATA sub_data;
    sub_data.pData = buffer;
    sub_data.RowPitch = size;
    sub_data.SlicePitch = size;

    UpdateSubresources<1>(
        g_command_list.Get(), 
        g_input_default.Get(), g_input_upload.Get(), 
        0, 0, 1, &sub_data
    );
    g_command_list->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
            g_input_default.Get(), 
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    ));
}

void init_data_and_buffers() {
    // 初始化随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-20.0, 20.0);

    std::vector<Data> data(data_count);
    for (INT i = 0; i < data_count; i++) {
        Data t;
        t.v = {dis(gen), dis(gen), dis(gen)};
        t.val = 0;
        data[i] = t;
    }
    UINT64 buffer_size = sizeof(Data) * data_count;

    // 创建输入缓冲区
    create_input_buffer_by_uploader(data.data(), buffer_size);

    // 创建输出缓冲区
    g_hr = g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(&g_output_default)
    );
    assert(SUCCEEDED(g_hr));

    // 创建回读缓冲区
    g_hr = g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&g_read_back)
    );
    assert(SUCCEEDED(g_hr));
}

void init() {
    // 启动调试层
    g_hr = D3D12GetDebugInterface(IID_PPV_ARGS(&g_debug));
    assert(SUCCEEDED(g_hr));
    g_debug->EnableDebugLayer();
    
    // 创建设备
    g_hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device));
    assert(SUCCEEDED(g_hr));

    // 创建命令队列
    D3D12_COMMAND_QUEUE_DESC cmd_que_desc;
    cmd_que_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cmd_que_desc.NodeMask = 0;
    cmd_que_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    cmd_que_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    g_device->CreateCommandQueue(&cmd_que_desc, IID_PPV_ARGS(&g_command_queue));

    // 创建围栏
    g_hr = g_device->CreateFence(g_fence_val, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    assert(SUCCEEDED(g_hr));

    // 创建命令分配器
    g_hr = g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&g_command_allocator));
    assert(SUCCEEDED(g_hr));

    // 创建命令列表
    g_hr = g_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_COMPUTE, 
        g_command_allocator.Get(), nullptr, 
        IID_PPV_ARGS(&g_command_list));
    assert(SUCCEEDED(g_hr));

    // 创建根参数
    const UINT param_count = 2;
    D3D12_ROOT_PARAMETER root_parameters[param_count];
    root_parameters[0] = {D3D12_ROOT_PARAMETER_TYPE_SRV, {0, 0}, D3D12_SHADER_VISIBILITY_ALL};
    root_parameters[1] = {D3D12_ROOT_PARAMETER_TYPE_UAV, {0, 0}, D3D12_SHADER_VISIBILITY_ALL};

    // 创建根签名
    Microsoft::WRL::ComPtr<ID3DBlob> signature, signature_error;
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    root_signature_desc.NumParameters = param_count;
    root_signature_desc.pParameters = root_parameters;
    root_signature_desc.NumStaticSamplers = 0;
    root_signature_desc.pStaticSamplers = nullptr;

    g_hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &signature_error);
    assert(SUCCEEDED(g_hr));
    g_hr = g_device->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), 
        IID_PPV_ARGS(&g_root_signature));
    assert(SUCCEEDED(g_hr));

    // 加载着色器
    g_hr = D3DReadFileToBlob(L"shader/cs.cso", &g_cs_shader);
    assert(SUCCEEDED(g_hr));

    // 创建PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc;
    compute_pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    compute_pso_desc.NodeMask = 0;
    compute_pso_desc.pRootSignature = g_root_signature.Get();
    compute_pso_desc.CS = {g_cs_shader->GetBufferPointer(), g_cs_shader->GetBufferSize()};
    compute_pso_desc.CachedPSO = {};
    g_hr = g_device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&g_pso));
    assert(SUCCEEDED(g_hr));

    // 初始化数据
    init_data_and_buffers();
}

void compute() {
    g_command_list->SetPipelineState(g_pso.Get());
    g_command_list->SetComputeRootSignature(g_root_signature.Get());

    g_command_list->SetComputeRootShaderResourceView(0, g_input_default->GetGPUVirtualAddress());
    g_command_list->SetComputeRootUnorderedAccessView(1, g_output_default->GetGPUVirtualAddress());

    g_command_list->Dispatch((data_count + 255) / 256, 1, 1);

    // 将结果复制到回读缓冲区
    g_command_list->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
            g_output_default.Get(), 
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
    ));
    g_command_list->CopyResource(g_read_back.Get(), g_output_default.Get());

    g_hr = g_command_list->Close();
    assert(SUCCEEDED(g_hr));

    ID3D12CommandList* cmds_lists[] = {g_command_list.Get()};
    g_command_queue->ExecuteCommandLists(1, cmds_lists);
}

void wait_for_gpu() {
    ++g_fence_val;
    g_hr = g_command_queue->Signal(g_fence.Get(), g_fence_val);
    assert(SUCCEEDED(g_hr));

    if (g_fence->GetCompletedValue() < g_fence_val) {
        HANDLE event_handle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        g_hr = g_fence->SetEventOnCompletion(g_fence_val, event_handle);
        assert(SUCCEEDED(g_hr));
        WaitForSingleObject(event_handle, INFINITE);
        CloseHandle(event_handle);
    }
}

void write_to_file(std::string file_path) {
    Data* data;
    D3D12_RANGE range;
    range.Begin = 0;
    range.End = sizeof(Data) * data_count;
    g_hr = g_read_back->Map(0, &range, (VOID**)&data);
    assert(SUCCEEDED(g_hr));

    std::ofstream fout(file_path);
    
    for (INT i = 0; i < data_count; i++) {
        Data t = data[i];
        fout << std::format("{} {} {} {} {}\n", t.id, t.v.x, t.v.y, t.v.z, t.val);
    }

    g_read_back->Unmap(0, nullptr);
    fout.close();
}

int main() {
    init();

    compute();
    wait_for_gpu();

    write_to_file("result.txt");
    std::cout << "OK" << std::endl;

    return 0;
}
