#include "IcosahedronApp.h"

using namespace DirectX;
using namespace d3dUtil;
using namespace std;

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
	Fence = 0;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdListAlloc));

	PassCB = std::make_unique<UploadBuffer<PassConstant>>(device, passCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstant>>(device, objectCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstant>>(device, materialCount, true);
}

FrameResource::~FrameResource()
{
}

IcosahedronApp::IcosahedronApp(HINSTANCE hInstance): D3DApp(hInstance)
{
}

bool IcosahedronApp::Initialize()
{
	if (!D3DApp::Initialize()) return false;

	ThrowIfFailed(mCommandList->Reset(mMainCmdAllocator.Get(), nullptr));

	BuildRootSignature();
	BuildGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResource();
	BuildShadersAndInputLayout();
	BuildPSO();

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	FlushCommandQueue();

	return true;
}

void IcosahedronApp::Update(const GameTimer& gt)
{
	UpdateCamera(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gFrameResourcesCount;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateAnimate(gt);

	UpdateMainPassCB(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCB(gt);
}

void IcosahedronApp::Draw(const GameTimer& gt)
{
	auto cmdAlloc = mCurrFrameResource->CmdListAlloc;
	ThrowIfFailed(cmdAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdAlloc.Get(), nullptr));

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

	auto passCBSize = d3dUtil::CalcConstantBufferSize(sizeof PassConstant);
	auto address = mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(1, address);

	mCommandList->SetPipelineState(mPSO.Get());
	DrawRenderItems(mOpaqueRitems);

	mCommandList->SetPipelineState(mVertexNormalPSO.Get());
	DrawRenderItems(mOpaquePointsRitems);

	mCommandList->SetPipelineState(mPlaneNormalPSO.Get());
	DrawRenderItems(mOpaquePlaneRitems);

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

	mCurrFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void IcosahedronApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void IcosahedronApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mHandle);
}

void IcosahedronApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void IcosahedronApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// make each pixel correspond to a quarter of a degree
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// update angles based on input to orbit camera around icosa
		mTheta += dx;
		mPhi += dy;
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// make each pixel correspond to 0.005 unit in the scene
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// update the camera radius based on input
		mRadius += dx - dy;
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void IcosahedronApp::UpdateCamera(const GameTimer& gt)
{
	// convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// build the view matrix
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMStoreFloat3(&mEyePos, pos);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void IcosahedronApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstant objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixIdentity());

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// next FrameResource need to be updated too
			e->NumFramesDirty--;
		}
	}
}

void IcosahedronApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	auto detView = XMMatrixDeterminant(view);
	auto detProj = XMMatrixDeterminant(proj);
	auto detViewPorj = XMMatrixDeterminant(viewProj);
	XMMATRIX invView = XMMatrixInverse(&detView, view);
	XMMATRIX invProj = XMMatrixInverse(&detProj, proj);
	XMMATRIX invViewProj = XMMatrixInverse(&detViewPorj, viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void IcosahedronApp::UpdateMaterialCB(const GameTimer& gt)
{
	auto materialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials) {
		auto mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			MaterialConstant m;
			m.DiffuseAlbedo = mat->DiffuseAlbedo;
			m.FresnelR0 = mat->FresnelR0;
			m.Roughness = mat->Roughness;
			m.MatTransform = mat->MatTransform;

			materialCB->CopyData(mat->MatCBIndex, m);
			mat->NumFramesDirty--;
		}
	}
}

void IcosahedronApp::UpdateAnimate(const GameTimer& gt)
{
}


void IcosahedronApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotParameters[3];
	slotParameters[0].InitAsConstantBufferView(0);
	slotParameters[1].InitAsConstantBufferView(2);
	slotParameters[2].InitAsConstantBufferView(1);


	D3D12_ROOT_SIGNATURE_DESC desc;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = nullptr;
	desc.NumStaticSamplers = 0;
	desc.pParameters = slotParameters;
	desc.NumParameters = 3;

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
	ThrowIfFailed(mD3DDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void IcosahedronApp::BuildRenderItems()
{
	auto icosaRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&icosaRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	icosaRitem->ObjCBIndex = 0;
	icosaRitem->Geo = mGeometries["shapeGeo"].get();
	icosaRitem->Mat = mMaterials["woodCrate"].get();
	icosaRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	icosaRitem->IndexCount = icosaRitem->Geo->DrawArgs["icosa"].IndexCount;
	icosaRitem->StartIndexLocation = icosaRitem->Geo->DrawArgs["icosa"].StartIndexLocation;
	icosaRitem->BaseVertexLocation = icosaRitem->Geo->DrawArgs["icosa"].BaseVertexLocation;
	
	auto icosaPointsRitem = make_unique<RenderItem>();
	*icosaPointsRitem = *icosaRitem;
	mAllRitems.push_back(std::move(icosaRitem));
	mOpaqueRitems.push_back(mAllRitems.back().get());

	icosaPointsRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	mAllRitems.push_back(std::move(icosaPointsRitem));
	mOpaquePointsRitems.push_back(mAllRitems.back().get());

	mOpaquePlaneRitems.push_back(mAllRitems.front().get());
}

void IcosahedronApp::BuildShadersAndInputLayout()
{
	mVertexShader = d3dUtil::CompileShader(L"Shaders/SubDivide.hlsl", "VS", "vs_5_0");
	mPixelShader = d3dUtil::CompileShader(L"Shaders/SubDivide.hlsl", "PS", "ps_5_0"); 
	mGeometryShader = d3dUtil::CompileShader(L"Shaders/SubDivide.hlsl", "GS", "gs_5_0");

	mPointGS = d3dUtil::CompileShader(L"Shaders/SubDivide.hlsl", "GSPoint", "gs_5_0");
	mPlaneGS = d3dUtil::CompileShader(L"Shaders/SubDivide.hlsl", "GSPlane", "gs_5_0");
	
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void IcosahedronApp::BuildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData icosa = geoGen.CreateGeosphere(1.0f, 1);

	std::vector<Vertex> vertices(icosa.Vertices.size());

	UINT k = 0;
	for (size_t i = 0; i < icosa.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = icosa.Vertices[i].Position;
		vertices[k].Normal = icosa.Vertices[i].Normal;
		vertices[k].TexC = icosa.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(icosa.GetIndices16()), std::end(icosa.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mCommandList.Get(), geo->VertexBufferCPU.Get(), geo->VertexUploadBuffer);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mCommandList.Get(), geo->IndexBufferCPU.Get(), geo->IndexUploadBuffer);

	geo->VertexStride = sizeof(Vertex);
	geo->VertexBufferSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferSize = ibByteSize;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.BaseVertexLocation = 0;
	boxSubmesh.IndexCount = indices.size();
	boxSubmesh.StartIndexLocation = 0;

	geo->DrawArgs["icosa"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void IcosahedronApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = mRootSignature.Get();
	desc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	desc.VS = { mVertexShader->GetBufferPointer(), mVertexShader->GetBufferSize() };
	desc.PS = { mPixelShader->GetBufferPointer(), mPixelShader->GetBufferSize() };
	desc.GS = { mGeometryShader->GetBufferPointer(), mGeometryShader->GetBufferSize() };
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = mBackBufferFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.DSVFormat = mDepthStencilFormat;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(mD3DDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPSO)));

	auto pointDesc = desc;
	pointDesc.GS = { mPointGS->GetBufferPointer(), mPointGS->GetBufferSize() };
	pointDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	ThrowIfFailed(mD3DDevice->CreateGraphicsPipelineState(&pointDesc, IID_PPV_ARGS(&mVertexNormalPSO)));

	auto planeDesc = desc;
	planeDesc.GS = { mPlaneGS->GetBufferPointer(), mPlaneGS->GetBufferSize() };
	ThrowIfFailed(mD3DDevice->CreateGraphicsPipelineState(&planeDesc, IID_PPV_ARGS(&mPlaneNormalPSO)));
}

void IcosahedronApp::BuildFrameResource()
{
	for (int i = 0; i < gFrameResourcesCount; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(mD3DDevice.Get(), 1, mAllRitems.size(),
			(UINT)mMaterials.size()));
	}
}

void IcosahedronApp::DrawRenderItems(const vector<RenderItem*>& ritems)
{
	auto matCBSize = d3dUtil::CalcConstantBufferSize(sizeof MaterialConstant);
	auto objCBSize = d3dUtil::CalcConstantBufferSize(sizeof ObjectConstant);

	for (const auto& e : ritems) {
		auto vbv = e->Geo->VertexBufferView();
		auto ibv = e->Geo->IndexBufferView();
		mCommandList->IASetVertexBuffers(0, 1, &vbv);
		mCommandList->IASetIndexBuffer(&ibv);
		mCommandList->IASetPrimitiveTopology(e->PrimitiveType);

		auto objAdress = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress();
		objAdress += e->ObjCBIndex * objCBSize;
		mCommandList->SetGraphicsRootConstantBufferView(0, objAdress);

		auto matAddress = mCurrFrameResource->MaterialCB->Resource()->GetGPUVirtualAddress();
		matAddress += e->Mat->MatCBIndex * matCBSize;
		mCommandList->SetGraphicsRootConstantBufferView(2, matAddress);

		mCommandList->DrawIndexedInstanced(e->IndexCount, 1, e->StartIndexLocation, e->BaseVertexLocation, 0);
	}
}

void IcosahedronApp::BuildMaterials()
{
	auto woodCrate = std::make_unique<Material>();
	woodCrate->Name = "woodCrate";
	woodCrate->MatCBIndex = 0;
	woodCrate->DiffuseSrvHeapIndex = -1;
	woodCrate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	woodCrate->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	woodCrate->Roughness = 0.2f;
	woodCrate->MatTransform = MathHelper::Identity4x4();

	mMaterials["woodCrate"] = std::move(woodCrate);
}
