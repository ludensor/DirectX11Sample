#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define ARRAY_COUNT(a) (sizeof(a) / sizeof(a[0]))

LPCWSTR Title = TEXT("Direct3D 11 Sample 1 - Device Initialization");
constexpr int32_t WIN_WIDTH = 1600;
constexpr int32_t WIN_HEIGHT = 900;

IDXGIFactory* Factory;
IDXGIAdapter* Adapter;
ID3D11Device* Device;
ID3D11DeviceContext* ImmediateContext;
IDXGISwapChain* SwapChain;
ID3D11RenderTargetView* RenderTargetView;

namespace VendorId
{
	constexpr uint32_t INTEL = 0x8086;
	constexpr uint32_t NVIDIA = 0x10DE;
	constexpr uint32_t AMD = 0x1002;
}

constexpr float CLEAR_COLOR[]{ 0.0f, 0.125f, 0.3f, 1.0f };

bool InitDevice(HWND hWnd);
void Render();
void FreeDevice();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	WNDCLASSEX wc;
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = nullptr;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = TEXT("SampleWindowClass");
	wc.hIconSm = nullptr;
	RegisterClassEx(&wc);

	RECT rc{ 0, 0, WIN_WIDTH, WIN_HEIGHT };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

	HWND hWnd = CreateWindow(wc.lpszClassName, Title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);

	if (!InitDevice(hWnd))
	{
		PostQuitMessage(1);
	}

	LARGE_INTEGER prevTime, currentTime;
	QueryPerformanceCounter(&prevTime);

	LARGE_INTEGER cpuTick;
	QueryPerformanceFrequency(&cpuTick);

	float elapsedTime = 0.0f;
	int32_t frameCount = 0;

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			QueryPerformanceCounter(&currentTime);

			const float deltaTime = (float)(currentTime.QuadPart - prevTime.QuadPart) / cpuTick.QuadPart;

			++frameCount;
			elapsedTime += deltaTime;
			if (elapsedTime >= 1.0f)
			{
				const float fps = (float)frameCount;
				const float mspf = 1000.0f / fps;

				WCHAR buffer[256];
				swprintf_s(buffer, ARRAY_COUNT(buffer), TEXT("%s    fps: %0.2f    mspf: %f"), Title, fps, mspf);
				SetWindowText(hWnd, buffer);

				frameCount = 0;
				elapsedTime = 0.0f;
			}

			prevTime = currentTime;

			Render();
		}
	}

	UnregisterClass(wc.lpszClassName, hInstance);
	FreeDevice();

	return (int)msg.wParam;
}

bool InitDevice(HWND hWnd)
{
	uint32_t referenceCount = 0;

	// Create factory
	if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&Factory))))
	{
		return false;
	}

	// Enum adapter
	IDXGIAdapter* adapter;
	for (uint32_t adapterIndex = 0; Factory->EnumAdapters(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC adapterDesc;
		adapter->GetDesc(&adapterDesc);

		if (adapterDesc.VendorId == VendorId::NVIDIA ||
			adapterDesc.VendorId == VendorId::AMD ||
			adapterDesc.VendorId == VendorId::INTEL)
		{
			Adapter = adapter;
			break;
		}

		referenceCount = adapter->Release();
	}

	// Create device and device context
	uint32_t createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

	constexpr D3D_FEATURE_LEVEL featureLevels[]
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	constexpr size_t numFeatureLevels = ARRAY_COUNT(featureLevels);

	D3D_FEATURE_LEVEL maxSupportedFeatureLevel;
	if (FAILED(D3D11CreateDevice(Adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &Device, &maxSupportedFeatureLevel, &ImmediateContext)))
	{
		return false;
	}

	// Create swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferDesc.Width = WIN_WIDTH;
	swapChainDesc.BufferDesc.Height = WIN_HEIGHT;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = 0;

	if (FAILED(Factory->CreateSwapChain(Device, &swapChainDesc, &SwapChain)))
	{
		return false;
	}

	// Create render target view
	ID3D11Texture2D* BackBuffer;
	if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer))))
	{
		return false;
	}

	HRESULT hr = Device->CreateRenderTargetView(BackBuffer, nullptr, &RenderTargetView);
	referenceCount = BackBuffer->Release();
	if (FAILED(hr))
	{
		return false;
	}

	// Settings
	ImmediateContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)WIN_WIDTH;
	viewport.Height = (float)WIN_HEIGHT;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	ImmediateContext->RSSetViewports(1, &viewport);

	return true;
}

void Render()
{
	ImmediateContext->ClearRenderTargetView(RenderTargetView, CLEAR_COLOR);
	SwapChain->Present(0, 0);
}

void FreeDevice()
{
	if (ImmediateContext) { ImmediateContext->ClearState(); }

	uint32_t referenceCount = 0;
	if (RenderTargetView) { referenceCount = RenderTargetView->Release(); }
	if (SwapChain) { referenceCount = SwapChain->Release(); }
	if (ImmediateContext) { referenceCount = ImmediateContext->Release(); }
	if (Device) { referenceCount = Device->Release(); }
	if (Adapter) { referenceCount = Adapter->Release(); }
	if (Factory) { referenceCount = Factory->Release(); }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

