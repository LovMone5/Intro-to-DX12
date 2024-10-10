#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include "d3dx12.h"

namespace d3dUtil 
{
	inline UINT CalcConstantBufferSize(UINT size) 
	{
		return (size + 255) & ~255;
	}

	struct SubmeshGeometry
	{
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		INT BaseVertexLocation = 0;
	};

	struct MeshGeometry
	{
		std::string Name;

		Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU;
		Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexUploadBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexUploadBuffer;

		UINT VertexBufferSize;
		UINT VertexStride;
		DXGI_FORMAT IndexFormat;
		UINT IndexBufferSize;

		std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView()
		{
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.SizeInBytes = VertexBufferSize;
			vbv.StrideInBytes = VertexStride;

			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView()
		{
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferSize;

			return ibv;
		}
	};

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device, 
		ID3D12GraphicsCommandList* cmdList,
		ID3DBlob* data,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
	{
		UINT byteSize = data->GetBufferSize();

		Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;
		
		CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProp, 
			D3D12_HEAP_FLAG_NONE, 
			&desc, 
			D3D12_RESOURCE_STATE_COMMON, 
			nullptr, 
			IID_PPV_ARGS(&defaultBuffer)));

		// create an intermediary
		heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));

		D3D12_SUBRESOURCE_DATA subresourceData;
		subresourceData.pData = data->GetBufferPointer();
		subresourceData.RowPitch = byteSize;
		subresourceData.SlicePitch = byteSize;

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &barrier);
		UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subresourceData);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &barrier);

		return defaultBuffer;
	}

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		std::wstring file,
		std::string entry,
		std::string target,
		const D3D_SHADER_MACRO* defines = nullptr)
	{
		UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
		compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		Microsoft::WRL::ComPtr<ID3DBlob> blob;
		Microsoft::WRL::ComPtr<ID3DBlob> error = nullptr;

		auto hr = D3DCompileFromFile(
			file.c_str(), 
			defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry.c_str(), target.c_str(),
			compileFlags, 0, &blob, &error);

		if (FAILED(hr)) 
		{
			if (error)
				::OutputDebugString((WCHAR*)error->GetBufferPointer());
			ThrowIfFailed(hr);
		}

		return blob;
	}


	inline std::string ToStringAlign(int x, int al) 
	{
		std::stringstream ss;
		ss << std::setw(al) << std::setfill('0') << x;
		return ss.str();
	}

	inline std::wstring ToWStringAlign(int x, int al)
	{
		std::wstringstream ss;
		ss << std::setw(al) << std::setfill(L'0') << x;
		return ss.str();
	}
}
