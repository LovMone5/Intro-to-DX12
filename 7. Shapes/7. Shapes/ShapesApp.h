#pragma once
#include <memory>
#include <vector>
#include <array>
#include <DirectXPackedVector.h>
#include "../../Common/D3DApp.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/MathHelper.h"

struct ObjectConstant {
	DirectX::XMFLOAT4X4 World;
};

struct PassConstant {
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

const INT gFrameResourcesCount = 3;

struct FrameResource {
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstant>> PassCB;
	std::unique_ptr<UploadBuffer<ObjectConstant>> ObjectCB;

	UINT Fence;
};

struct RenderItem {
	DirectX::XMFLOAT4X4 World;
	INT NumFramesDirty = gFrameResourcesCount;

	d3dUtil::MeshGeometry* Geo = nullptr;
	UINT ObjCBIndex;
	D3D_PRIMITIVE_TOPOLOGY PrimitiveType;

	UINT BaseVertexLocation;
	UINT IndexCount;
	UINT StartIndexLocation;
};

class ShapesApp: public D3DApp {
public:
	ShapesApp(HINSTANCE hInstance);

	bool Initialize();
	void Update(const GameTimer& gt);
	void Draw(const GameTimer& gt);

private:
	void OnResize();
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildRenderItems();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSO();
	void BuildFrameResource();
	void BuildConstantBufferViews();
	void DrawRenderItems(const std::vector<RenderItem*>& ritems);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap;

	std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> mGeometries;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

	Microsoft::WRL::ComPtr<ID3DBlob> mVertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mPixelShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	DirectX::XMFLOAT3 mEyePos;
	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = DirectX::XM_PIDIV4;
	float mRadius = 5.0f;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	UINT mCurrFrameResourceIndex = 0;
	UINT mPassCbvOffset;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mOpaqueRitems;

	PassConstant mMainPassCB;
};
