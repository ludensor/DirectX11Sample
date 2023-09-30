#include <windowsx.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

namespace VendorId
{
	constexpr uint32_t INTEL = 0x8086;
	constexpr uint32_t NVIDIA = 0x10DE;
	constexpr uint32_t AMD = 0x1002;
}

struct VertexData
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
};

struct ConstantBufferData
{
	XMMATRIX WorldMatrix;
	XMMATRIX ViewMatrix;
	XMMATRIX ProjectionMatrix;
	XMVECTOR WorldLightPosition;
	XMVECTOR WorldCameraPosition;
};

enum INPUT_FLAGS : uint32_t
{
	INPUT_FLAGS_NONE = 0,
	INPUT_FLAGS_1 = 1 << 0,
	INPUT_FLAGS_2 = 1 << 1,
	INPUT_FLAGS_A = 1 << 2,
	INPUT_FLAGS_D = 1 << 3,
	INPUT_FLAGS_E = 1 << 4,
	INPUT_FLAGS_Q = 1 << 5,
	INPUT_FLAGS_S = 1 << 6,
	INPUT_FLAGS_W = 1 << 7,
	INPUT_FLAGS_RBUTTON = 1 << 8
};

const WCHAR* Title = TEXT("Direct3D 11 - Rendering a Sphere and Lighting    (1: Solid 2: Wireframe)");
constexpr int32_t WIN_WIDTH = 1600;
constexpr int32_t WIN_HEIGHT = 900;
POINT CursorPoint;

IDXGIFactory* Factory;
IDXGIAdapter* Adapter;
ID3D11Device* Device;
ID3D11DeviceContext* ImmediateContext;
IDXGISwapChain* SwapChain;
ID3D11RenderTargetView* RenderTargetView;
ID3D11Texture2D* DepthStencilBuffer;
ID3D11DepthStencilView* DepthStencilView;
ID3D11Buffer* VertexBuffer;
ID3D11Buffer* IndexBuffer;
ID3D11Buffer* ConstantBuffer;
ID3D11InputLayout* InputLayout;
ID3D11VertexShader* VertexShader;
ID3D11PixelShader* PixelShader;
ID3D11RasterizerState* SolidRasterizerState;
ID3D11RasterizerState* WireframeRasterizerState;

constexpr float CLEAR_COLOR[]{ 0.0f, 0.125f, 0.3f, 1.0f };

constexpr float OBJECT_ROTATION_SPEED = 45.0f;
constexpr int32_t SLICE_COUNT = 32;
constexpr int32_t RING_COUNT = 32;
XMMATRIX ObjectWorldMatrix;

XMVECTOR LightWorldPosition = XMVectorSet(5.0f, 5.0f, 0.0f, 1.0f);

constexpr float CAMERA_MOVEMENT_SPEED = 10.0f;
constexpr float CAMERA_ROTATION_SPEED = 0.002f;
XMVECTOR CameraRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR CameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
XMVECTOR CameraForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR CameraPosition = XMVectorSet(0.0f, 1.0f, -5.0f, 1.0f);
XMMATRIX ViewMatrix;

constexpr float FOV = XMConvertToRadians(45.0f);
constexpr float NEAR_Z = 0.1f;
constexpr float FAR_Z = 1000.0f;
XMMATRIX ProjectionMatrix;

uint32_t InputFlags;

bool InitDevice(HWND hWnd);
void Update(float deltaTime);
void Render();
void FreeDevice();
HRESULT CompileShaderFromFile(const WCHAR* fileName, const char* entryPoint, const char* shaderModel, ID3DBlob** outBlob);

void MoveForward(float value);
void MoveRight(float value);
void MoveUp(float value);
void Rotate(float deltaX, float deltaY);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
INPUT_FLAGS ConvertVirtualKeyToInputKey(WPARAM wParam);

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

			const float deltaTime = (currentTime.QuadPart - prevTime.QuadPart) / (float)cpuTick.QuadPart;

			++frameCount;
			elapsedTime += deltaTime;
			if (elapsedTime >= 1.0f)
			{
				const float fps = (float)frameCount;
				const float mspf = 1000.0f / fps;

				constexpr uint32_t bufferSize = 512;
				WCHAR buff[bufferSize];
				swprintf_s(buff, bufferSize, TEXT("%s    fps: %0.2f    mspf: %f"), Title, fps, mspf);
				SetWindowText(hWnd, buff);

				frameCount = 0;
				elapsedTime = 0.0f;
			}

			prevTime = currentTime;

			Update(deltaTime);
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
		D3D_FEATURE_LEVEL_11_0
	};
	constexpr uint32_t numFeatureLevels = (uint32_t)std::size(featureLevels);

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

	// Create depth stencil view
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = WIN_WIDTH;
	depthStencilDesc.Height = WIN_HEIGHT;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	if (FAILED(Device->CreateTexture2D(&depthStencilDesc, nullptr, &DepthStencilBuffer)))
	{
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = depthStencilDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	if (FAILED(Device->CreateDepthStencilView(DepthStencilBuffer, &depthStencilViewDesc, &DepthStencilView)))
	{
		return false;
	}

	// Create vertex buffer
	std::vector<VertexData> vertices(SLICE_COUNT * RING_COUNT + 2);

	// Top
	vertices.front() = { XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) };

	// Bottom
	vertices.back() = { XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) };

	constexpr float deltaThetaAngle = XM_PI / (float)(RING_COUNT + 1);
	constexpr float deltaPhiAngle = XM_2PI / (float)SLICE_COUNT;

	float theta = 0.0f;
	for (int32_t ringIndex = 0; ringIndex < RING_COUNT; ++ringIndex)
	{
		theta += deltaThetaAngle;

		float sinTheta, cosTheta;
		XMScalarSinCos(&sinTheta, &cosTheta, theta);

		float phi = 0.0f;
		for (int32_t sliceIndex = 0; sliceIndex < SLICE_COUNT; ++sliceIndex)
		{
			float sinPhi, cosPhi;
			XMScalarSinCos(&sinPhi, &cosPhi, phi);

			const int32_t index = ringIndex * SLICE_COUNT + sliceIndex + 1;
			vertices[index].Position = XMFLOAT3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
			vertices[index].Normal = XMFLOAT3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);

			phi += deltaPhiAngle;
		}
	}

	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = sizeof(VertexData) * (uint16_t)vertices.size();
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData{};
	vertexBufferData.pSysMem = vertices.data();

	if (FAILED(Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &VertexBuffer)))
	{
		return false;
	}

	// Create index buffer
	std::vector<uint16_t> indices(SLICE_COUNT * RING_COUNT * 6);

	// Top
	int32_t index = 0;
	for (int32_t i = 1; i <= SLICE_COUNT; ++i)
	{
		indices[index++] = 0;
		indices[index++] = i % SLICE_COUNT + 1;
		indices[index++] = i;
	}

	for (int32_t i = 0; i < RING_COUNT - 1; ++i)
	{
		for (int32_t j = 1; j <= SLICE_COUNT; ++j)
		{
			const int32_t nextJ = j % SLICE_COUNT + 1;

			indices[index++] = SLICE_COUNT * i + j;
			indices[index++] = SLICE_COUNT * (i + 1) + nextJ;
			indices[index++] = SLICE_COUNT * (i + 1) + j;

			indices[index++] = SLICE_COUNT * i + j;
			indices[index++] = SLICE_COUNT * i + nextJ;
			indices[index++] = SLICE_COUNT * (i + 1) + nextJ;
		}
	}

	// Bottom
	for (int32_t i = 1; i <= SLICE_COUNT; ++i)
	{
		const int32_t baseIndex = SLICE_COUNT * (RING_COUNT - 1);

		indices[index++] = baseIndex + i;
		indices[index++] = baseIndex + i % SLICE_COUNT + 1;
		indices[index++] = SLICE_COUNT * RING_COUNT + 1;
	}

	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.ByteWidth = sizeof(uint16_t) * (uint16_t)indices.size();
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA indexBufferData{};
	indexBufferData.pSysMem = indices.data();

	if (FAILED(Device->CreateBuffer(&indexBufferDesc, &indexBufferData, &IndexBuffer)))
	{
		return false;
	}

	// Create constant buffer
	D3D11_BUFFER_DESC constantBufferDesc{};
	constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = 0;

	if (FAILED(Device->CreateBuffer(&constantBufferDesc, nullptr, &ConstantBuffer)))
	{
		return false;
	}

	// Create rasterizer state
	D3D11_RASTERIZER_DESC rasterizerDesc;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.ScissorEnable = false;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.AntialiasedLineEnable = false;

	if (FAILED(Device->CreateRasterizerState(&rasterizerDesc, &SolidRasterizerState)))
	{
		return false;
	}

	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;

	if (FAILED(Device->CreateRasterizerState(&rasterizerDesc, &WireframeRasterizerState)))
	{
		return false;
	}

	// Create vertex shader
	ID3DBlob* vertexShaderBlob;
	if (FAILED(CompileShaderFromFile(TEXT("Lighting.hlsl"), "VS", "vs_4_1", &vertexShaderBlob)))
	{
		return false;
	}

	if (FAILED(Device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &VertexShader)))
	{
		referenceCount = vertexShaderBlob->Release();
		return false;
	}

	// Create input layout
	constexpr D3D11_INPUT_ELEMENT_DESC elements[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	constexpr uint32_t numElements = (uint16_t)std::size(elements);

	hr = Device->CreateInputLayout(elements, numElements, vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &InputLayout);
	referenceCount = vertexShaderBlob->Release();
	if (FAILED(hr))
	{
		return false;
	}

	// Create pixel shader
	ID3DBlob* pixelShaderBlob;
	if (FAILED(CompileShaderFromFile(TEXT("Lighting.hlsl"), "PS", "ps_4_1", &pixelShaderBlob)))
	{
		return false;
	}

	hr = Device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &PixelShader);
	referenceCount = pixelShaderBlob->Release();
	if (FAILED(hr))
	{
		return false;
	}

	ImmediateContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)WIN_WIDTH;
	viewport.Height = (float)WIN_HEIGHT;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	ImmediateContext->RSSetViewports(1, &viewport);
	ImmediateContext->RSSetState(SolidRasterizerState);

	ImmediateContext->IASetInputLayout(InputLayout);

	constexpr uint32_t stride = sizeof(VertexData);
	constexpr uint32_t offset = 0;
	ImmediateContext->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);

	ImmediateContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ImmediateContext->VSSetShader(VertexShader, nullptr, 0);
	ImmediateContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
	ImmediateContext->PSSetShader(PixelShader, nullptr, 0);

	return true;
}

void Update(float deltaTime)
{
	if (InputFlags & INPUT_FLAGS_1)
	{
		ImmediateContext->RSSetState(SolidRasterizerState);
	}
	if (InputFlags & INPUT_FLAGS_2)
	{
		ImmediateContext->RSSetState(WireframeRasterizerState);
	}
	if (InputFlags & INPUT_FLAGS_W)
	{
		MoveForward(deltaTime);
	}
	if (InputFlags & INPUT_FLAGS_S)
	{
		MoveForward(-deltaTime);
	}
	if (InputFlags & INPUT_FLAGS_D)
	{
		MoveRight(deltaTime);
	}
	if (InputFlags & INPUT_FLAGS_A)
	{
		MoveRight(-deltaTime);
	}
	if (InputFlags & INPUT_FLAGS_E)
	{
		MoveUp(deltaTime);
	}
	if (InputFlags & INPUT_FLAGS_Q)
	{
		MoveUp(-deltaTime);
	}

	static POINT prevCursorPoint;
	if (InputFlags & INPUT_FLAGS_RBUTTON)
	{
		const float deltaX = (float)(CursorPoint.y - prevCursorPoint.y);
		const float deltaY = (float)(CursorPoint.x - prevCursorPoint.x);
		Rotate(deltaX, deltaY);
	}
	prevCursorPoint = CursorPoint;

	static float objectRotationAngle;
	objectRotationAngle += OBJECT_ROTATION_SPEED * deltaTime;
	ObjectWorldMatrix = XMMatrixRotationY(XMConvertToRadians(objectRotationAngle));

	ViewMatrix = XMMatrixLookAtLH(CameraPosition, CameraPosition + CameraForward, CameraUp);
	ProjectionMatrix = XMMatrixPerspectiveFovLH(FOV, WIN_WIDTH / (float)WIN_HEIGHT, NEAR_Z, FAR_Z);
}

void Render()
{
	ImmediateContext->ClearRenderTargetView(RenderTargetView, CLEAR_COLOR);
	ImmediateContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	ConstantBufferData constantBufferData;
	constantBufferData.WorldMatrix = XMMatrixTranspose(ObjectWorldMatrix);
	constantBufferData.ViewMatrix = XMMatrixTranspose(ViewMatrix);
	constantBufferData.ProjectionMatrix = XMMatrixTranspose(ProjectionMatrix);
	constantBufferData.WorldLightPosition = LightWorldPosition;
	constantBufferData.WorldCameraPosition = CameraPosition;
	ImmediateContext->UpdateSubresource(ConstantBuffer, 0, nullptr, &constantBufferData, 0, 0);

	ImmediateContext->DrawIndexed(SLICE_COUNT * RING_COUNT * 6, 0, 0);

	SwapChain->Present(0, 0);
}

void FreeDevice()
{
	if (ImmediateContext) { ImmediateContext->ClearState(); }

	uint32_t referenceCount = 0;
	if (WireframeRasterizerState) { referenceCount = WireframeRasterizerState->Release(); }
	if (SolidRasterizerState) { referenceCount = SolidRasterizerState->Release(); }
	if (PixelShader) { referenceCount = PixelShader->Release(); }
	if (InputLayout) { referenceCount = InputLayout->Release(); }
	if (VertexShader) { referenceCount = VertexShader->Release(); }
	if (ConstantBuffer) { referenceCount = ConstantBuffer->Release(); }
	if (IndexBuffer) { referenceCount = IndexBuffer->Release(); }
	if (VertexBuffer) { referenceCount = VertexBuffer->Release(); }
	if (DepthStencilView) { referenceCount = DepthStencilView->Release(); }
	if (DepthStencilBuffer) { referenceCount = DepthStencilBuffer->Release(); }
	if (RenderTargetView) { referenceCount = RenderTargetView->Release(); }
	if (SwapChain) { referenceCount = SwapChain->Release(); }
	if (ImmediateContext) { referenceCount = ImmediateContext->Release(); }
	if (Device) { referenceCount = Device->Release(); }
	if (Adapter) { referenceCount = Adapter->Release(); }
	if (Factory) { referenceCount = Factory->Release(); }
}

HRESULT CompileShader(const void* srcData, size_t srcDataSize, const char* entryPoint, const char* shaderModel, ID3DBlob** outBlob)
{
	if (!outBlob)
	{
		return E_FAIL;
	}

	uint32_t referenceCount = 0;

	uint32_t shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	shaderFlags |= D3DCOMPILE_DEBUG;
#endif // _DEBUG

	ID3DBlob* shaderCode = nullptr;
	ID3DBlob* errorMessage;
	HRESULT hr = D3DCompile(srcData, srcDataSize, nullptr, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel, shaderFlags, 0, &shaderCode, &errorMessage);
	if (FAILED(hr))
	{
		if (errorMessage)
		{
			OutputDebugStringA((char*)errorMessage->GetBufferPointer());
			referenceCount = errorMessage->Release();
		}
	}

	if (shaderCode)
	{
		uint32_t disassembleFlags = D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING;

		ID3DBlob* disassembly;
		if (SUCCEEDED(D3DDisassemble(shaderCode->GetBufferPointer(), shaderCode->GetBufferSize(), disassembleFlags, nullptr, &disassembly)))
		{
			OutputDebugStringA((char*)disassembly->GetBufferPointer());
			referenceCount = disassembly->Release();
		}
	}

	*outBlob = shaderCode;

	return hr;
}

HRESULT CompileShaderFromFile(const WCHAR* fileName, const char* entryPoint, const char* shaderModel, ID3DBlob** outBlob)
{
	if (!outBlob)
	{
		return E_FAIL;
	}

	uint32_t referenceCount = 0;

	uint32_t shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	shaderFlags |= D3DCOMPILE_DEBUG;
#endif // _DEBUG

	ID3DBlob* shaderCode = nullptr;
	ID3DBlob* errorMessage = nullptr;
	HRESULT hr = D3DCompileFromFile(fileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel, shaderFlags, 0, &shaderCode, &errorMessage);
	if (FAILED(hr))
	{
		if (errorMessage)
		{
			OutputDebugStringA((char*)errorMessage->GetBufferPointer());
			referenceCount = errorMessage->Release();
		}
	}

	if (shaderCode)
	{
		uint32_t disassembleFlags = D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING;

		ID3DBlob* disassembly;
		if (SUCCEEDED(D3DDisassemble(shaderCode->GetBufferPointer(), shaderCode->GetBufferSize(), disassembleFlags, nullptr, &disassembly)))
		{
			OutputDebugStringA((char*)disassembly->GetBufferPointer());
			referenceCount = disassembly->Release();
		}
	}

	*outBlob = shaderCode;

	return hr;
}

void MoveForward(float value)
{
	CameraPosition += CameraForward * value * CAMERA_MOVEMENT_SPEED;
}

void MoveRight(float value)
{
	CameraPosition += CameraRight * value * CAMERA_MOVEMENT_SPEED;
}

void MoveUp(float value)
{
	CameraPosition += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * value * CAMERA_MOVEMENT_SPEED;
}

void Rotate(float deltaX, float deltaY)
{
	const float pitchAngle = deltaX * CAMERA_ROTATION_SPEED;
	const float yawAngle = deltaY * CAMERA_ROTATION_SPEED;

	const XMVECTOR pitchRotation = XMQuaternionRotationAxis(CameraRight, pitchAngle);
	const XMVECTOR yawRotation = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), yawAngle);
	const XMVECTOR rotation = XMQuaternionMultiply(pitchRotation, yawRotation);

	CameraRight = XMVector3Rotate(CameraRight, yawRotation);
	CameraUp = XMVector3Rotate(CameraUp, rotation);
	CameraForward = XMVector3Rotate(CameraForward, rotation);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			PostMessage(hWnd, WM_DESTROY, 0, 0);
			break;
		}
		InputFlags |= ConvertVirtualKeyToInputKey(wParam);
		break;
	case WM_KEYUP:
		InputFlags &= ~ConvertVirtualKeyToInputKey(wParam);
		break;

	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
		CursorPoint.x = GET_X_LPARAM(lParam);
		CursorPoint.y = GET_Y_LPARAM(lParam);
		if (message == WM_NCMOUSEMOVE)
		{
			ScreenToClient(hWnd, &CursorPoint);
		}
		break;

	case WM_RBUTTONDOWN:
		if (!(InputFlags & INPUT_FLAGS_RBUTTON) && !GetCapture())
		{
			SetCapture(hWnd);
		}
		InputFlags |= INPUT_FLAGS_RBUTTON;
		break;
	case WM_RBUTTONUP:
		InputFlags &= ~INPUT_FLAGS_RBUTTON;
		if (!(InputFlags & INPUT_FLAGS_RBUTTON) && GetCapture() == hWnd)
		{
			ReleaseCapture();
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

INPUT_FLAGS ConvertVirtualKeyToInputKey(WPARAM wParam)
{
	switch (wParam)
	{
	case '1': return INPUT_FLAGS_1;
	case '2': return INPUT_FLAGS_2;
	case 'A': return INPUT_FLAGS_A;
	case 'D': return INPUT_FLAGS_D;
	case 'E': return INPUT_FLAGS_E;
	case 'Q': return INPUT_FLAGS_Q;
	case 'S': return INPUT_FLAGS_S;
	case 'W': return INPUT_FLAGS_W;
	default: return INPUT_FLAGS_NONE;
	}
}

