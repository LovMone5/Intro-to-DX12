#include "SobelFilter.h"

SobelFilter::SobelFilter(ID3D12Device* device, LONG width, LONG height) : 
	mDevice(device), mWidth(width), mHeight(height)
{
	BuildResource();
	BuildShadersAndPSOs();
}

void SobelFilter::OnResize(LONG width, LONG height)
{
	if (width != mWidth || height != mHeight)
	{
		mWidth = width, mHeight = height;
		BuildResource();
		BuildDescriptors();
	}
}

void SobelFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleStart, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleStart, UINT descriptorSize)
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

ID3D12Resource* SobelFilter::OutputBuffer()
{
	return mOutput.Get();
}

void SobelFilter::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* input)
{
	cmdList->SetComputeRootSignature(mPostProcessRootSignature.Get());

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(input,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mInput.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	cmdList->CopyResource(mInput.Get(), input);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mInput.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutput.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->SetPipelineState(mSobelPSO.Get());

	cmdList->SetComputeRootDescriptorTable(0, mGPUHandleSRV0);
	cmdList->SetComputeRootDescriptorTable(1, mGPUHandleUAV0);

	cmdList->Dispatch((UINT)ceilf(mWidth / 16.0f), (UINT)ceilf(mHeight / 16.0f), 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mInput.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutput.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));
}

void SobelFilter::BuildResource()
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
		IID_PPV_ARGS(&mOutput)
	);

	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mInput)
	);
}

void SobelFilter::BuildDescriptors()
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

	mDevice->CreateShaderResourceView(mInput.Get(), &srvDesc, mCPUHandleSRV0);
	mDevice->CreateUnorderedAccessView(mOutput.Get(), nullptr, &uavDesc, mCPUHandleUAV0);
}

void SobelFilter::BuildShadersAndPSOs()
{
	mSobelCS = d3dUtil::CompileShader(L"Shaders\\Sobel.hlsl", "SobelCS", "cs_5_0");
	//mSobelVS = d3dUtil::CompileShader(L"Shaders\\Sobel.hlsl", "VS", "vs_5_0");
	//mSobelPS = d3dUtil::CompileShader(L"Shaders\\Sobel.hlsl", "VS", "vs_5_0");

	CD3DX12_DESCRIPTOR_RANGE srvRange, uavRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER parameter[2];
	parameter[0].InitAsDescriptorTable(1, &srvRange);
	parameter[1].InitAsDescriptorTable(1, &uavRange);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, parameter);
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig, errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));

	mDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mPostProcessRootSignature)
	);

	D3D12_COMPUTE_PIPELINE_STATE_DESC vertDesc = {};
	vertDesc.CS = { mSobelCS->GetBufferPointer(), mSobelCS->GetBufferSize() };
	vertDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	vertDesc.NodeMask = 0;
	vertDesc.pRootSignature = mPostProcessRootSignature.Get();
	mDevice->CreateComputePipelineState(&vertDesc, IID_PPV_ARGS(&mSobelPSO));
}
