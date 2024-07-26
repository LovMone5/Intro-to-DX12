#include "BoxApp.h"

using namespace DirectX;

BoxApp::BoxApp(HINSTANCE hInstance): D3DApp(hInstance)
{
}

bool BoxApp::Initialize()
{
	if (!D3DApp::Initialize()) return false;

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildBoxAndPyramidGeometry();
	BuildShadersAndInputLayout();
	BuildPSO();

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	FlushCommandQueue();

	return true;
}

void BoxApp::Update(const GameTimer& gt)
{
	// convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// build the view matrix
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMMatrixIdentity();
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstant objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mBoxCB->CopyData(0, objConstants);

	world = XMMatrixTranslation(0.0f, 1.2f, 0.0f);
	worldViewProj = world * view * proj;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mPyramidCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get()));

	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mCurrentBackBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &barrier);

	mCommandList->ClearRenderTargetView(
		CurrentBackBufferView(),
		DirectX::Colors::LightBlue,
		0, nullptr);
	mCommandList->ClearDepthStencilView(
		DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	auto cbv = CurrentBackBufferView();
	auto dsv = DepthStencilView();
	mCommandList->OMSetRenderTargets(1, &cbv, true, &dsv);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	ID3D12DescriptorHeap* descHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descHeaps), descHeaps);

	auto vbvp = mBoxGeo->VertexPosBufferView();
	auto vbvc = mBoxGeo->VertexColorBufferView();
	auto ibv = mBoxGeo->IndexBufferView();
	mCommandList->IASetVertexBuffers(0, 1, &vbvp);
	mCommandList->IASetVertexBuffers(1, 1, &vbvc);
	mCommandList->IASetIndexBuffer(&ibv);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	// mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	// mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	// mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	auto baseDesc = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	mCommandList->SetGraphicsRootDescriptorTable(0, baseDesc);
	const auto& box = mBoxGeo->DrawArgs["box"];
	mCommandList->DrawIndexedInstanced(box.IndexCount, 1, box.StartIndexLocation, box.BaseVertexLocation, 0);

	baseDesc.Offset(1, mCbvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(0, baseDesc);
	const auto& pyramid = mBoxGeo->DrawArgs["pyramid"];
	mCommandList->DrawIndexedInstanced(pyramid.IndexCount, 1, pyramid.StartIndexLocation, pyramid.BaseVertexLocation, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mCurrentBackBufferIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &barrier);

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* commandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	mSwapChain->Present(0, 0);
	mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % mSwapChainBufferCount;

	FlushCommandQueue();
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mHandle);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// make each pixel correspond to a quarter of a degree
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// update angles based on input to orbit camera around box
		mTheta += dx;
		mPhi += dy;
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// make each pixel correspond to 0.005 unit in the scene
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// update the camera radius based on input
		mRadius += dx - dy;
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 2;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;

	ThrowIfFailed(mD3DDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotParameters[1];
	CD3DX12_DESCRIPTOR_RANGE table;
	table.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0);
	slotParameters[0].InitAsDescriptorTable(1, &table);

	D3D12_ROOT_SIGNATURE_DESC desc;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = nullptr;
	desc.pParameters = slotParameters;
	desc.NumParameters = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
	mD3DDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature));
}

void BoxApp::BuildConstantBuffers()
{
	mBoxCB = std::make_unique<UploadBuffer<ObjectConstant>>(mD3DDevice.Get(), 1, true);

	auto objCBSize = d3dUtil::CalcConstantBufferSize(sizeof ObjectConstant);
	auto cbAddress = mBoxCB->Resource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = cbAddress;
	desc.SizeInBytes = objCBSize;

	auto destAddress = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
	mD3DDevice->CreateConstantBufferView(&desc, destAddress);


	mPyramidCB = std::make_unique<UploadBuffer<ObjectConstant>>(mD3DDevice.Get(), 1, true);

	objCBSize = d3dUtil::CalcConstantBufferSize(sizeof ObjectConstant);
	cbAddress = mPyramidCB->Resource()->GetGPUVirtualAddress();

	desc.BufferLocation = cbAddress;
	desc.SizeInBytes = objCBSize;

	destAddress.Offset(1, mCbvUavDescriptorSize);
	mD3DDevice->CreateConstantBufferView(&desc, destAddress);
}

void BoxApp::BuildShadersAndInputLayout()
{
	mVertexShader = d3dUtil::CompileShader(L"../Shaders/color.hlsl", "VS", "vs_5_0");
	mPixelShader = d3dUtil::CompileShader(L"../Shaders/color.hlsl", "PS", "ps_5_0"); 
	
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void BoxApp::BuildBoxAndPyramidGeometry()
{
	std::array<VPosData, 8 + 5> verticesPos = {
		// box
		VPosData({ XMFLOAT3(-1.0f, -1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, +1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, +1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, -1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, -1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, +1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, +1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, -1.0f, +1.0f) }),

		// pyramid
		VPosData({ XMFLOAT3(+0.0f, +1.0f, +0.0f) }),
		VPosData({ XMFLOAT3(-1.0f, +0.0f, +1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, +0.0f, +1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, +0.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, +0.0f, -1.0f) }),
	};
	std::array<VColorData, 8 + 5> verticesColor = {
		VColorData({ XMFLOAT4(Colors::White) }),
		VColorData({ XMFLOAT4(Colors::Black) }),
		VColorData({ XMFLOAT4(Colors::Red) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Blue) }),
		VColorData({ XMFLOAT4(Colors::Yellow) }),
		VColorData({ XMFLOAT4(Colors::Cyan) }),
		VColorData({ XMFLOAT4(Colors::Magenta) }),

		VColorData({ XMFLOAT4(Colors::Red) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Green) })
	};
	std::array<std::uint16_t, 36 + 6 * 3> indices = {
		// -------------box-------------
		// front face
		0, 1, 2,
		0, 2, 3,
		// back face
		4, 6, 5,
		4, 7, 6,
		// left face
		4, 5, 1,
		4, 1, 0,
		// right face
		3, 2, 6,
		3, 6, 7,
		// top face
		1, 5, 6,
		1, 6, 2,
		// bottom face
		4, 0, 3,
		4, 3, 7,

		// -------------pyramid-------------
		// front face
		0, 4, 3,
		// right face
		0, 2, 4,
		// back face
		0, 1, 2,
		// left face
		0, 3, 1,
		// bottom face
		3, 4, 1,
		4, 2, 1
	};

	SIZE_T vpbByteSize = verticesPos.size() * sizeof VPosData;
	SIZE_T vcbByteSize = verticesColor.size() * sizeof VColorData;
	SIZE_T ibByteSize = indices.size() * sizeof std::uint16_t;

	mBoxGeo = std::make_unique<d3dUtil::MeshGeometry>();
	mBoxGeo->Name = "BoxAndPyramid";

	ThrowIfFailed(D3DCreateBlob(vpbByteSize, &mBoxGeo->VertexPosBufferCPU));
	CopyMemory(mBoxGeo->VertexPosBufferCPU->GetBufferPointer(), verticesPos.data(), vpbByteSize);
	ThrowIfFailed(D3DCreateBlob(vcbByteSize, &mBoxGeo->VertexColorBufferCPU));
	CopyMemory(mBoxGeo->VertexColorBufferCPU->GetBufferPointer(), verticesColor.data(), vcbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mBoxGeo->VertexPosBufferGPU = d3dUtil::CreateDefaultBuffer(
		mD3DDevice.Get(), 
		mCommandList.Get(), 
		mBoxGeo->VertexPosBufferCPU.Get(),
		mBoxGeo->VertexPosUploadBuffer);
	mBoxGeo->VertexColorBufferGPU = d3dUtil::CreateDefaultBuffer(
		mD3DDevice.Get(),
		mCommandList.Get(),
		mBoxGeo->VertexColorBufferCPU.Get(),
		mBoxGeo->VertexColorUploadBuffer);
	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		mD3DDevice.Get(),
		mCommandList.Get(),
		mBoxGeo->IndexBufferCPU.Get(),
		mBoxGeo->IndexUploadBuffer);

	mBoxGeo->VertexBufferSizePos = vpbByteSize;
	mBoxGeo->VertexStridePos = sizeof VPosData;
	mBoxGeo->VertexBufferSizeColor = vcbByteSize;
	mBoxGeo->VertexStrideColor = sizeof VColorData;
	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferSize = ibByteSize;
	
	d3dUtil::SubmeshGeometry box;
	box.BaseVertexLocation = 0;
	box.IndexCount = 36;
	box.StartIndexLocation = 0;
	mBoxGeo->DrawArgs["box"] = box;

	d3dUtil::SubmeshGeometry pyramid;
	pyramid.BaseVertexLocation = 8;
	pyramid.IndexCount = 18;
	pyramid.StartIndexLocation = 36;
	mBoxGeo->DrawArgs["pyramid"] = pyramid;
}

void BoxApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = mRootSignature.Get();
	desc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	desc.VS = { mVertexShader->GetBufferPointer(), mVertexShader->GetBufferSize() };
	desc.PS = { mPixelShader->GetBufferPointer(), mPixelShader->GetBufferSize() };
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = mBackBufferFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(mD3DDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPSO)));
}
