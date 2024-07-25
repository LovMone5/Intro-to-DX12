#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
#include "D3DUtils.h"
#include "DxException.h"

template<class T>
class UploadBuffer {
public:
	UploadBuffer(
		ID3D12Device* device,
		UINT elemCount,
		BOOL isConstantBuffer) 
	{
		UINT bufferSize = sizeof T;
		if (isConstantBuffer)
			bufferSize = d3dUtil::CalcConstantBufferSize(bufferSize);
		mBufferSize = bufferSize;
		
		auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(elemCount * bufferSize);
		device->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer));
		ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
	}
	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer()
	{
		if (mMappedData)
			mUploadBuffer->Unmap(0, nullptr);
		mMappedData = nullptr;
	}

	void CopyData(UINT elemIndex, const T& data)
	{
		memcpy(&mMappedData[elemIndex * mBufferSize], &data, sizeof T);
	}
	ID3D12Resource* Resource() 
	{
		return mUploadBuffer.Get();
	}

private:
	BYTE* mMappedData;
	UINT mBufferSize;
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
};
