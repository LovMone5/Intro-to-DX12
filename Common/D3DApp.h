#pragma once
#include <Windows.h>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>
#include <windowsx.h>
#include "d3dx12.h"
#include "DxException.h"
#include "GameTimer.h"
#include "D3DUtils.h"

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class D3DApp {
public:
	D3DApp(HINSTANCE hInstance);
	~D3DApp();

	virtual bool Initialize();
	virtual void Update(const GameTimer& gt) = 0;
	virtual void Draw(const GameTimer& gt) = 0;
	int Run();
	
	float AspectRatio();

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static D3DApp* GetApp();

protected:
	virtual void OnResize();
	virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;

	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void FlushCommandQueue();
	void CalculateFrame();

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView();
	ID3D12Resource* CurrentBackBuffer();

	HINSTANCE mInstance;
	static D3DApp* mApp;
	std::wstring mWndCaptain = L"Learn DX12";
	long mClientWidth = 1024;
	long mClientHeight = 768;
	HWND mHandle = nullptr;

	GameTimer mTimer;
	bool mAppPaused = false;
	bool mMinimized = false;
	bool mMaximized = false;
	bool mResizing = false;
	DOUBLE mElapsedTime = 1.0f;
	LONGLONG mFrameCount = 0;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mDXGIFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> mD3DDevice;
	ULONGLONG mCurrentFence = 0;
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT mCbvUavDescriptorSize;
	static const UINT mSwapChainBufferCount = 2;
	UINT mCurrentBackBufferIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mMainCmdAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	D3D12_VIEWPORT mViewport;
	RECT mScissorRect;

	POINT mLastMousePos;
};
