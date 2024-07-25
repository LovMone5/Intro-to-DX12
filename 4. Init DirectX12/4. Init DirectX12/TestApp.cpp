#include "TestApp.h"

TestApp::TestApp(HINSTANCE hInstance): D3DApp(hInstance) {
}

bool TestApp::Initialize()
{
	if (!D3DApp::Initialize()) return false;

	return true;
}

void TestApp::Update(const GameTimer& gt)
{
}

void TestApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

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
	mCommandList->OMSetRenderTargets(
		1,
		&cbv,
		true,
		&dsv);

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
