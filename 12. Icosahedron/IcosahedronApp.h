#pragma once
#include <memory>
#include <vector>
#include <array>
#include <DirectXPackedVector.h>
#include "D3DApp.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "DDSTextureLoader.h"

#define MaxLights 16

struct ObjectConstant {
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 TexTransform;
};

struct Light
{
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
	float FalloffEnd = 10.0f;
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
	float SpotPower = 64.0f;
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
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 0.0f };
	Light Lights[MaxLights];
};

struct MaterialConstant {
	DirectX::XMFLOAT4 DiffuseAlbedo = { 0.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.0f, 0.0f, 0.0f };
	FLOAT Roughness = 0.0f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

const INT gFrameResourcesCount = 3;

struct FrameResource {
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstant>> PassCB;
	std::unique_ptr<UploadBuffer<ObjectConstant>> ObjectCB;
	std::unique_ptr<UploadBuffer<MaterialConstant>> MaterialCB;

	UINT Fence;
};

struct Material {
	std::string Name;
	UINT MatCBIndex;
	UINT DiffuseSrvHeapIndex;
	INT NumFramesDirty = gFrameResourcesCount;

	DirectX::XMFLOAT4 DiffuseAlbedo = { 0.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.0f, 0.0f, 0.0f };
	FLOAT Roughness = 0.0f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Texture {
	std::string Name;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

struct RenderItem {
	DirectX::XMFLOAT4X4 World;
	INT NumFramesDirty = gFrameResourcesCount;

	d3dUtil::MeshGeometry* Geo = nullptr;
	Material* Mat = nullptr;
	UINT ObjCBIndex;
	D3D_PRIMITIVE_TOPOLOGY PrimitiveType;

	UINT BaseVertexLocation;
	UINT IndexCount;
	UINT StartIndexLocation;
};

class IcosahedronApp: public D3DApp {
public:
	IcosahedronApp(HINSTANCE hInstance);

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
	void UpdateMaterialCB(const GameTimer& gt);
	void UpdateAnimate(const GameTimer& gt);

	void BuildRootSignature();
	void BuildRenderItems();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSO();
	void BuildFrameResource();
	void BuildMaterials();
	void DrawRenderItems(const std::vector<RenderItem*>& ritems);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

	std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> mGeometries;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

	Microsoft::WRL::ComPtr<ID3DBlob> mVertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mPixelShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mGeometryShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mPointGS;
	Microsoft::WRL::ComPtr<ID3DBlob> mPlaneGS;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	DirectX::XMFLOAT3 mEyePos;
	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = 0.2f * DirectX::XM_PI;
	float mRadius = 15.0f;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	UINT mCurrFrameResourceIndex = 0;
	UINT mPassCbvOffset;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mOpaqueRitems;

	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	PassConstant mMainPassCB;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mVertexNormalPSO;
	std::vector<RenderItem*> mOpaquePointsRitems;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPlaneNormalPSO;
	std::vector<RenderItem*> mOpaquePlaneRitems;
};