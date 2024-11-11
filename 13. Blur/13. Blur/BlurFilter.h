#pragma once

#include "../../Common/D3DUtils.h"
#include <vector>

class BlurFilter
{
public:
	BlurFilter() = delete;
	BlurFilter(ID3D12Device* device, LONG width, LONG height);

	void OnResize(LONG width, LONG height);
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleStart, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleStart, UINT descriptorSize);
	ID3D12Resource* OutputBuffer();
	void Execute(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* input, int blurCount);
private:
	void BuildResource();
	void BuildDescriptors();
	void BuildShadersAndPSOs();
	std::vector<float> GetGaussWeights(double sigma);

	LONG mWidth, mHeight;
	ID3D12Device* mDevice;
	D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandleSRV0, mCPUHandleSRV1, mCPUHandleUAV0, mCPUHandleUAV1;
	D3D12_GPU_DESCRIPTOR_HANDLE mGPUHandleSRV0, mGPUHandleSRV1, mGPUHandleUAV0, mGPUHandleUAV1;
	std::vector<float> mGaussWeights;
	
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mHorzBlurPSO, mVertBlurPSO;
	Microsoft::WRL::ComPtr<ID3DBlob> mHorzBlurCS, mVertBlurCS;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostProcessRootSignature;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap0, mBlurMap1;
};
