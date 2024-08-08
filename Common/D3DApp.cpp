#include "D3DApp.h"

using Microsoft::WRL::ComPtr;


LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp::D3DApp(HINSTANCE hInstance): mInstance(hInstance) {
	mApp = this;
}

D3DApp::~D3DApp()
{
	FlushCommandQueue();
}

bool D3DApp::Initialize() {
	// initialize window
	WNDCLASS wc;
	wc.hInstance = mInstance;
	wc.lpfnWndProc = MainWndProc;
	wc.lpszClassName = L"MainWnd";
	wc.lpszMenuName = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	
	if (!RegisterClass(&wc)) {
		MessageBox(nullptr, L"Register class failed!", nullptr, 0);
		return false;
	}

	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mHandle = CreateWindow(L"MainWnd", mWndCaptain.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		width, height, nullptr, nullptr, mInstance, nullptr);

	if (!mHandle) {
		MessageBox(nullptr, L"Create window failed!", nullptr, 0);
		return false;
	}

	ShowWindow(mHandle, SW_SHOW);
	UpdateWindow(mHandle);

	// initialize DirectX
#if defined (DEBUG) || defined (_DEBUG)
	// enable the debug layer
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif

	ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS(&mDXGIFactory)));
	auto hr = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&mD3DDevice));
	if (FAILED(hr)) {
		// back to wrap adapter
		ComPtr<IDXGIAdapter> pWrapAdapter;
		mDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWrapAdapter));
		ThrowIfFailed(D3D12CreateDevice(
			pWrapAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&mD3DDevice)));
	}

	// ThrowIfFailed(mDXGIFactory->MakeWindowAssociation(mHandle, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(mD3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
	mCbvUavDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mDsvDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mRtvDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	OnResize();

	return true;
}

int D3DApp::Run()
{
	MSG msg = { 0 };
	mTimer.Reset();

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// do game logic
			mTimer.Tick();
			CalculateFrame();
			if (!mAppPaused) {
				Update(mTimer);
				Draw(mTimer);
			}
		}
	}

	return (int)msg.wParam;
}

float D3DApp::AspectRatio()
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;
	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (mD3DDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{

				}
				else 
				{
					OnResize();
				}
			}
		}
		return 0;
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::GetApp()
{
	return mApp;
}

void D3DApp::OnResize()
{
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mMainCmdAllocator.Get(), nullptr));
	// realase the previous resources we will be creating
	for (int i = 0; i < mSwapChainBufferCount; i++)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// resize the swap chain
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mSwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	mCurrentBackBufferIndex = 0;

	// create render target buffer view
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < mSwapChainBufferCount; i++) {
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));

		mD3DDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(mRtvDescriptorSize);
	}

	// create depth/stencil buffer view
	D3D12_RESOURCE_DESC depthRourceDesc;
	depthRourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthRourceDesc.Alignment = 0;
	depthRourceDesc.Format = mDepthStencilFormat;
	depthRourceDesc.Width = mClientWidth;
	depthRourceDesc.Height = mClientHeight;
	depthRourceDesc.DepthOrArraySize = 1;
	depthRourceDesc.MipLevels = 1;
	depthRourceDesc.SampleDesc.Count = 1;
	depthRourceDesc.SampleDesc.Quality = 0;
	depthRourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthRourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClearValue;
	optClearValue.Format = mDepthStencilFormat;
	optClearValue.DepthStencil.Depth = 1.0f;
	optClearValue.DepthStencil.Stencil = 0;

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mD3DDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthRourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optClearValue,
		IID_PPV_ARGS(&mDepthStencilBuffer)));
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	mD3DDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, dsvHandle);

	// execute the resize commands
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();

	// recreate viewport and scissor rect
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = static_cast<float>(mClientWidth);
	mViewport.Height = static_cast<float>(mClientHeight);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

void D3DApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC desc;
	desc.NodeMask = 0;
	desc.Priority = 0;
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(mD3DDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(mD3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mMainCmdAllocator)));
	ThrowIfFailed(mD3DDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mMainCmdAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&mCommandList)));

	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC desc;
	desc.BufferDesc.Width = mClientWidth;
	desc.BufferDesc.Height = mClientHeight;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.Format = mBackBufferFormat;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = mSwapChainBufferCount;
	desc.OutputWindow = mHandle;
	desc.Windowed = true;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(mDXGIFactory->CreateSwapChain(mCommandQueue.Get(), &desc, &mSwapChain));
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = mSwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3DDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3DDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
}

void D3DApp::FlushCommandQueue()
{
	mCurrentFence++;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	while (mFence->GetCompletedValue() != mCurrentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void D3DApp::CalculateFrame()
{
	mFrameCount++;

	if (mTimer.TotalTime() - mElapsedTime >= 1.0f) {
		double mspf = 1000.0f / mFrameCount;

		auto fpsStr = std::to_wstring(mFrameCount);

		auto windowText = mWndCaptain + L"      fps: " + fpsStr;
		SetWindowText(mHandle, windowText.c_str());

		mFrameCount = 0;
		mElapsedTime += 1.0f;
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE top(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	return top.Offset(mCurrentBackBufferIndex, mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

