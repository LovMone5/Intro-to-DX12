#include "BlurFilter.h"

BlurFilter::BlurFilter(ID3D12Device* device, LONG width, LONG height) : 
	mDevice(device), mWidth(width), mHeight(height)
{
	BuildResource();
	BuildShadersAndPSOs();
	mGaussWeights = GetGaussWeights(2.5);
}

void BlurFilter::OnResize(LONG width, LONG height)
{
	if (width != mWidth || height != mHeight)
	{
		mWidth = width, mHeight = height;
		BuildResource();
		BuildDescriptors();
	}
}

void BlurFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleStart, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleStart, UINT descriptorSize)
{
	mCPUHandleSRV0 = cpuHandleStart.Offset(0, descriptorSize);
	mCPUHandleUAV1 = cpuHandleStart.Offset(1, descriptorSize);
	mCPUHandleSRV1 = cpuHandleStart.Offset(1, descriptorSize);
	mCPUHandleUAV0 = cpuHandleStart.Offset(1, descriptorSize);

	mGPUHandleSRV0 = gpuHandleStart.Offset(0, descriptorSize);
	mGPUHandleUAV1 = gpuHandleStart.Offset(1, descriptorSize);
	mGPUHandleSRV1 = gpuHandleStart.Offset(1, descriptorSize);
	mGPUHandleUAV0 = gpuHandleStart.Offset(1, descriptorSize);

	BuildDescriptors();
}

ID3D12Resource* BlurFilter::OutputBuffer()
{
	return mBlurMap0.Get();
}

void BlurFilter::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* input, int blurCount)
{
	int radius = mGaussWeights.size() / 2;
	cmdList->SetComputeRootSignature(mPostProcessRootSignature.Get());
	cmdList->SetComputeRoot32BitConstant(0, radius, 0);
	cmdList->SetComputeRoot32BitConstants(0, mGaussWeights.size(), mGaussWeights.data(), 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(input,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	cmdList->CopyResource(mBlurMap0.Get(), input);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	for (int i = 0; i < blurCount; ++i)
	{
		cmdList->SetPipelineState(mHorzBlurPSO.Get());

		cmdList->SetComputeRootDescriptorTable(1, mGPUHandleSRV0);
		cmdList->SetComputeRootDescriptorTable(2, mGPUHandleUAV1);

		// How many groups do we need to dispatch to cover a row of pixels, where each
		// group covers 256 pixels (the 256 is defined in the ComputeShader).
		UINT numGroupsX = (UINT)ceilf(mWidth / 256.0f);
		cmdList->Dispatch(numGroupsX, mHeight, 1);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));


		cmdList->SetPipelineState(mVertBlurPSO.Get());

		cmdList->SetComputeRootDescriptorTable(1, mGPUHandleSRV1);
		cmdList->SetComputeRootDescriptorTable(2, mGPUHandleUAV0);

		// How many groups do we need to dispatch to cover a column of pixels, where each
		// group covers 256 pixels  (the 256 is defined in the ComputeShader).
		UINT numGroupsY = (UINT)ceilf(mHeight / 256.0f);
		cmdList->Dispatch(mWidth, numGroupsY, 1);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));
}

void BlurFilter::BuildResource()
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.MipLevels = 1;
	desc.DepthOrArraySize = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Width = mWidth;
	desc.Height = mHeight;

	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBlurMap0)
	);

	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBlurMap1)
	);
}

void BlurFilter::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	mDevice->CreateShaderResourceView(mBlurMap0.Get(), &srvDesc, mCPUHandleSRV0);
	mDevice->CreateUnorderedAccessView(mBlurMap1.Get(), nullptr, &uavDesc, mCPUHandleUAV1);
	mDevice->CreateShaderResourceView(mBlurMap1.Get(), &srvDesc, mCPUHandleSRV1);
	mDevice->CreateUnorderedAccessView(mBlurMap0.Get(), nullptr, &uavDesc, mCPUHandleUAV0);
}

void BlurFilter::BuildShadersAndPSOs()
{
	mHorzBlurCS = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", "HorzBlurCS", "cs_5_0");
	mVertBlurCS = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", "VertBlurCS", "cs_5_0");

	CD3DX12_DESCRIPTOR_RANGE srvRange, uavRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER parameter[3];
	parameter[0].InitAsConstants(12, 0);
	parameter[1].InitAsDescriptorTable(1, &srvRange);
	parameter[2].InitAsDescriptorTable(1, &uavRange);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, parameter);
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig, errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));

	mDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mPostProcessRootSignature)
	);

	D3D12_COMPUTE_PIPELINE_STATE_DESC vertDesc = {};
	vertDesc.CS = { mVertBlurCS->GetBufferPointer(), mVertBlurCS->GetBufferSize() };
	vertDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	vertDesc.NodeMask = 0;
	vertDesc.pRootSignature = mPostProcessRootSignature.Get();
	mDevice->CreateComputePipelineState(&vertDesc, IID_PPV_ARGS(&mVertBlurPSO));

	D3D12_COMPUTE_PIPELINE_STATE_DESC horzDesc = vertDesc;
	horzDesc.CS = { mHorzBlurCS->GetBufferPointer(), mHorzBlurCS->GetBufferSize() };
	mDevice->CreateComputePipelineState(&horzDesc, IID_PPV_ARGS(&mHorzBlurPSO));
}

std::vector<float> BlurFilter::GetGaussWeights(double sigma)
{
	std::vector<float> ret;

	int radius = std::ceil(sigma * 2.0f);
	ret.resize(2 * radius + 1);

	double twoSigma2 = 2.0f * sigma * sigma;
	double sum = 0.0f;
	for (int x = -radius; x <= radius; x++) 
	{
		double res = std::exp(-x * x / twoSigma2);
		ret[x + radius] = res;
		sum += res;
	}
	for (auto& e : ret)
		e /= sum;

	return ret;
}
