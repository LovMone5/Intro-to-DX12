#pragma once
#include <memory>
#include <vector>
#include <array>
#include <DirectXPackedVector.h>
#include "../../Common/D3DApp.h"
#include "../../Common/UploadBuffer.h"

struct ObjectConstant {
	DirectX::XMFLOAT4X4 WorldViewProj;
	DirectX::XMFLOAT4 PulseColor;
	FLOAT TotalTime;
};

struct VPosData
{
	DirectX::XMFLOAT3 Pos;
};

struct VColorData
{
	DirectX::PackedVector::XMCOLOR Color;
};

class BoxApp: public D3DApp {
public:
	BoxApp(HINSTANCE hInstance);

	bool Initialize();
	void Update(const GameTimer& gt);
	void Draw(const GameTimer& gt);

private:
	void OnResize();
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildConstantBuffers();
	void BuildShadersAndInputLayout();
	void BuildBoxAndPyramidGeometry();
	void BuildPSO();

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap;
	std::unique_ptr<UploadBuffer<ObjectConstant>> mBoxCB;
	std::unique_ptr<UploadBuffer<ObjectConstant>> mPyramidCB;
	std::unique_ptr<d3dUtil::MeshGeometry> mBoxGeo;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

	Microsoft::WRL::ComPtr<ID3DBlob> mVertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mPixelShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	DirectX::XMFLOAT4X4 mWorld;
	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = DirectX::XM_PIDIV4;
	float mRadius = 5.0f;
};
