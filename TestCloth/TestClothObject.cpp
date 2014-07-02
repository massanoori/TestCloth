#include "stdafx.h"
#include "TestClothObject.h"
#include "Globals.h"

namespace
{
	const int NDIM_HORIZONTAL = 128;
	const int NDIM_VERTICAL = 128;

	struct SpringCS
	{
		float stiffness;
		float damping;
		float restLength;
		float dummy;
	};

	struct CB_TEST_CLOTH_UPDATE
	{
		SpringCS Neighbour;
		SpringCS Diagonal;
		SpringCS Bending;
		DirectX::XMUINT2 ClothResolution;
		float TimeStep;
		float dummy;
	};
}

class TestClothObject : public Object
{
private:
	struct SimulationBuffers
	{
		ComPtr<ID3D11ShaderResourceView> ClothPositionSRV;
		ComPtr<ID3D11UnorderedAccessView> ClothPositionUAV;
		ComPtr<ID3D11ShaderResourceView> ClothVelocitySRV;
		ComPtr<ID3D11UnorderedAccessView> ClothVelocityUAV;
		ComPtr<ID3D11Buffer> ClothPositionBuffer;
		ComPtr<ID3D11Buffer> ClothVelocityBuffer;
	};

	struct CB_TEST_CLOTH
	{
		DirectX::XMMATRIX WorldView;
		DirectX::XMMATRIX Projection;
		DirectX::XMUINT2 ClothResolution;
		DirectX::XMUINT2 dummy;
	};

	static void InitializeBuffers(SimulationBuffers& buffers)
	{
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));

		ID3D11Buffer* pBuffer;

		// initialize buffer desc for structured buffer
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS |
			D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.CPUAccessFlags = 0;

		// buffer for vertices
		bufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) *
			NDIM_HORIZONTAL * NDIM_VERTICAL;
		bufferDesc.StructureByteStride = sizeof(DirectX::XMFLOAT4);
		DXUTGetD3D11Device()->CreateBuffer(&bufferDesc, nullptr, &pBuffer);
		ComPtr<ID3D11Buffer>(pBuffer, false)
			.swap(buffers.ClothPositionBuffer);

		// buffer for velocities
		DXUTGetD3D11Device()->CreateBuffer(&bufferDesc, nullptr, &pBuffer);
		ComPtr<ID3D11Buffer>(pBuffer, false)
			.swap(buffers.ClothVelocityBuffer);

		// SRV desc
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = NDIM_HORIZONTAL * NDIM_VERTICAL;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		ID3D11ShaderResourceView* pSRV;

		// UAV desc
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		ZeroMemory(&uavDesc, sizeof(uavDesc));
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = NDIM_HORIZONTAL * NDIM_VERTICAL;
		ID3D11UnorderedAccessView* pUAV;

		// positions
		DXUTGetD3D11Device()->CreateShaderResourceView(buffers.ClothPositionBuffer.get(),
			&srvDesc, &pSRV);
		ComPtr<ID3D11ShaderResourceView>(pSRV, false)
			.swap(buffers.ClothPositionSRV);
		DXUTGetD3D11Device()->CreateUnorderedAccessView(buffers.ClothPositionBuffer.get(),
			&uavDesc, &pUAV);
		ComPtr<ID3D11UnorderedAccessView>(pUAV, false)
			.swap(buffers.ClothPositionUAV);

		// velocities
		DXUTGetD3D11Device()->CreateShaderResourceView(buffers.ClothVelocityBuffer.get(),
			&srvDesc, &pSRV);
		ComPtr<ID3D11ShaderResourceView>(pSRV, false)
			.swap(buffers.ClothVelocitySRV);
		DXUTGetD3D11Device()->CreateUnorderedAccessView(buffers.ClothVelocityBuffer.get(),
			&uavDesc, &pUAV);
		ComPtr<ID3D11UnorderedAccessView>(pUAV, false)
			.swap(buffers.ClothVelocityUAV);
	}

	static void InitializePositions(ComPtr<ID3D11UnorderedAccessView>)
	{
	}

	static void InitializeVelocities(ComPtr<ID3D11UnorderedAccessView>)
	{
	}

	void UpdateBuffer(const SimulationBuffers& buffersFrom,
		SimulationBuffers& buffersTo)
	{
		CB_TEST_CLOTH_UPDATE cbTestCloth;
		cbTestCloth.Neighbour.stiffness = m_desc.Neighbour.Stiffness;
		cbTestCloth.Neighbour.damping = m_desc.Neighbour.Damping;
		cbTestCloth.Neighbour.restLength = 2.0f / (NDIM_HORIZONTAL - 1);

		cbTestCloth.Diagonal.stiffness = m_desc.Diagonal.Stiffness;
		cbTestCloth.Diagonal.damping = m_desc.Diagonal.Damping;
		cbTestCloth.Diagonal.restLength = 2.0f * std::sqrtf(2.0f) / (NDIM_HORIZONTAL - 1);

		cbTestCloth.Bending.stiffness = m_desc.Bending.Stiffness;
		cbTestCloth.Bending.damping = m_desc.Bending.Damping;
		cbTestCloth.Bending.restLength = 4.0f / (NDIM_HORIZONTAL - 1);

		cbTestCloth.ClothResolution.x = NDIM_HORIZONTAL;
		cbTestCloth.ClothResolution.y = NDIM_VERTICAL;
		cbTestCloth.TimeStep = m_desc.TimeStep;

		auto pCTX = DXUTGetD3D11DeviceContext();
		D3D11_MAPPED_SUBRESOURCE subres;
		ZeroMemory(&subres, sizeof(subres));
		pCTX->Map(m_pUpdateConstants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &subres);
		memcpy(subres.pData, &cbTestCloth, sizeof(cbTestCloth));
		pCTX->Unmap(m_pUpdateConstants.get(), 0);

		ID3D11ShaderResourceView* pSRVs[2] =
		{
			buffersFrom.ClothPositionSRV.get(),
			buffersFrom.ClothVelocitySRV.get(),
		};

		ID3D11UnorderedAccessView* pUAVs[3] =
		{
			buffersTo.ClothPositionUAV.get(),
			buffersTo.ClothVelocityUAV.get(),
			m_pClothNormalUAV.get(),
		};

		ID3D11Buffer* pConstants = m_pUpdateConstants.get();

		pCTX->CSSetShader(m_pUpdateShader.get(), nullptr, 0);
		pCTX->CSSetShaderResources(0, 2, pSRVs);
		pCTX->CSSetUnorderedAccessViews(0, 3, pUAVs, nullptr);
		pCTX->CSSetConstantBuffers(0, 1, &pConstants);

		pCTX->Dispatch(1, 64, 1);

		pSRVs[0] = nullptr;
		pSRVs[1] = nullptr;
		pUAVs[0] = nullptr;
		pUAVs[1] = nullptr;
		pUAVs[2] = nullptr;

		pCTX->CSSetShaderResources(0, 2, pSRVs);
		pCTX->CSSetUnorderedAccessViews(0, 3, pUAVs, nullptr);
	}

	void InitializeVertexShader()
	{
		// VS
		ID3DBlob* pShaderCode;
		if (FAILED(DXUTCompileFromFile(L"TestClothVS.hlsl", nullptr, "main",
			"vs_4_0", 0, 0,
			&pShaderCode)))
		{
			return;
		}

		ID3D11VertexShader* pVS;
		if (FAILED(DXUTGetD3D11Device()->CreateVertexShader(pShaderCode->GetBufferPointer(),
			pShaderCode->GetBufferSize(), nullptr, &pVS)))
		{
			pShaderCode->Release();
			return;
		}

		ComPtr<ID3D11VertexShader>(pVS, false)
			.swap(m_pTestClothVS);

		pShaderCode->Release();
	}

	void InitializeGeometryShader()
	{
		// GS
		ID3DBlob* pShaderCode;
		if (FAILED(DXUTCompileFromFile(L"TestClothGS.hlsl", nullptr, "main",
			"gs_4_0", 0, 0, &pShaderCode)))
		{
			return;
		}

		ID3D11GeometryShader* pGS;
		if (FAILED(DXUTGetD3D11Device()->CreateGeometryShader(pShaderCode->GetBufferPointer(),
			pShaderCode->GetBufferSize(), nullptr, &pGS)))
		{
			pShaderCode->Release();
			return;
		}

		pShaderCode->Release();

		ComPtr<ID3D11GeometryShader>(pGS, false)
			.swap(m_pTestClothGS);
	}

	void InitializePixelShader()
	{
		// PS
		ID3DBlob* pShaderCode;
		if (FAILED(DXUTCompileFromFile(L"TestClothPS.hlsl", nullptr,
			"main", "ps_4_0", 0, 0, &pShaderCode)))
		{
			return;
		}

		ID3D11PixelShader* pPS;
		if (FAILED(DXUTGetD3D11Device()->CreatePixelShader(pShaderCode->GetBufferPointer(),
			pShaderCode->GetBufferSize(), nullptr, &pPS)))
		{
			pShaderCode->Release();
			return;
		}

		pShaderCode->Release();

		ComPtr<ID3D11PixelShader>(pPS, false)
			.swap(m_pTestClothPS);
	}

	void InitializeConstantBuffer()
	{
		D3D11_BUFFER_DESC constBufDesc;
		ZeroMemory(&constBufDesc, sizeof(constBufDesc));
		constBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constBufDesc.ByteWidth = sizeof(CB_TEST_CLOTH);
		constBufDesc.Usage = D3D11_USAGE_DYNAMIC;

		ID3D11Buffer* pConstBuffer;
		assert(SUCCEEDED(DXUTGetD3D11Device()->CreateBuffer(&constBufDesc,
			nullptr, &pConstBuffer)));
		ComPtr<ID3D11Buffer>(pConstBuffer, false)
			.swap(m_pTestClothConstants);

		constBufDesc.ByteWidth = sizeof(CB_TEST_CLOTH_UPDATE);
		assert(SUCCEEDED(DXUTGetD3D11Device()->CreateBuffer(&constBufDesc,
			nullptr, &pConstBuffer)));
		ComPtr<ID3D11Buffer>(pConstBuffer, false)
			.swap(m_pUpdateConstants);
	}

	void InitializeRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerDesc;
		ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
		rasterizerDesc.CullMode = D3D11_CULL_BACK;
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.DepthClipEnable = TRUE;

		ID3D11RasterizerState* pRasterizerState;
		assert(SUCCEEDED(DXUTGetD3D11Device()
			->CreateRasterizerState(&rasterizerDesc,
			&pRasterizerState)));

		ComPtr<ID3D11RasterizerState>(pRasterizerState, false)
			.swap(m_pRasterizerState);
	}

	void InitializeNormals()
	{
		// buffer for normal
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS |
			D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) *
			NDIM_HORIZONTAL * NDIM_VERTICAL;
		bufferDesc.StructureByteStride = sizeof(DirectX::XMFLOAT4);

		ID3D11Buffer* pBuffer;
		DXUTGetD3D11Device()->CreateBuffer(&bufferDesc,
			nullptr, &pBuffer);
		ComPtr<ID3D11Buffer>(pBuffer, false)
			.swap(m_pClothNormalBuffer);

		// SRV for normal
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = NDIM_HORIZONTAL * NDIM_VERTICAL;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

		ID3D11ShaderResourceView* pSRV;
		DXUTGetD3D11Device()->CreateShaderResourceView(pBuffer, &srvDesc,
			&pSRV);
		ComPtr<ID3D11ShaderResourceView>(pSRV, false)
			.swap(m_pClothNormalSRV);

		// UAV for normal
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		ZeroMemory(&uavDesc, sizeof(uavDesc));
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = NDIM_HORIZONTAL * NDIM_VERTICAL;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

		ID3D11UnorderedAccessView* pUAV;
		DXUTGetD3D11Device()->CreateUnorderedAccessView(pBuffer, &uavDesc,
			&pUAV);
		ComPtr<ID3D11UnorderedAccessView>(pUAV, false)
			.swap(m_pClothNormalUAV);
	}

	void InitializeBufferContents()
	{
		ID3DBlob* pShaderBuffer;
		if (FAILED(DXUTCompileFromFile(L"TestClothInit.hlsl", nullptr, "main",
			"cs_5_0", 0, 0, &pShaderBuffer)))
		{
			return;
		}

		ID3D11ComputeShader* pShader;
		if (FAILED(DXUTGetD3D11Device()->CreateComputeShader(
			pShaderBuffer->GetBufferPointer(),
			pShaderBuffer->GetBufferSize(),
			nullptr, &pShader)))
		{
			pShaderBuffer->Release();
			return;
		}

		pShaderBuffer->Release();

		struct CB_TEST_CLOTH_INIT
		{
			DirectX::XMFLOAT4 FourPositions[4];
			DirectX::XMUINT2 ClothResolution, dummy;
		}  cbTestClothInit;

		float SQRT2 = std::sqrtf(2.0f);
		cbTestClothInit.FourPositions[0] = DirectX::XMFLOAT4(-1.0f, 1.0f, 0.0f, 1.0f);
		cbTestClothInit.FourPositions[1] = DirectX::XMFLOAT4( 1.0f, 1.0f, 0.0f, 1.0f);
		cbTestClothInit.FourPositions[2] = DirectX::XMFLOAT4(-1.0f, SQRT2 - 1.0f, SQRT2, 1.0f);
		cbTestClothInit.FourPositions[3] = DirectX::XMFLOAT4( 1.0f, SQRT2 - 1.0f, SQRT2, 1.0f);
		cbTestClothInit.ClothResolution.x = NDIM_HORIZONTAL;
		cbTestClothInit.ClothResolution.y = NDIM_VERTICAL;

		ID3D11Buffer* pConstBuffer;
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));
		bufferDesc.ByteWidth = sizeof(cbTestClothInit);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA subresData;
		ZeroMemory(&subresData, sizeof(subresData));
		subresData.pSysMem = &cbTestClothInit;
		DXUTGetD3D11Device()->CreateBuffer(&bufferDesc,
			&subresData, &pConstBuffer);

		ID3D11UnorderedAccessView* pUAVs[2] = {
			m_SimBuffers[0].ClothPositionUAV.get(),
			m_SimBuffers[0].ClothVelocityUAV.get(),
		};

		auto pCTX = DXUTGetD3D11DeviceContext();
		pCTX->CSSetShader(pShader, nullptr, 0);
		pCTX->CSSetConstantBuffers(0, 1, &pConstBuffer);
		pCTX->CSSetUnorderedAccessViews(0, 2, pUAVs, nullptr);

		pCTX->Dispatch(1, 64, 1);

		pConstBuffer->Release();
		pShader->Release();

		pUAVs[0] = nullptr;
		pUAVs[1] = nullptr;
		pConstBuffer = nullptr;

		pCTX->CSSetShader(nullptr, nullptr, 0);
		pCTX->CSSetConstantBuffers(0, 1, &pConstBuffer);
		pCTX->CSSetUnorderedAccessViews(0, 2, pUAVs, nullptr);
	}

	void InitializeShader()
	{
		ID3DBlob* pShaderBuffer;
		if (FAILED(DXUTCompileFromFile(L"TestClothUpdate.hlsl", nullptr, "main", "cs_5_0", 0, 0, &pShaderBuffer)))
		{
			return;
		}

		ID3D11ComputeShader* pShader;
		if (FAILED(DXUTGetD3D11Device()->CreateComputeShader(pShaderBuffer->GetBufferPointer(),
			pShaderBuffer->GetBufferSize(), nullptr, &pShader)))
		{
			pShaderBuffer->Release();
			return;
		}

		pShaderBuffer->Release();

		ComPtr<ID3D11ComputeShader>(pShader, false)
			.swap(m_pUpdateShader);
	}

public:
	void Initialize(const TestCloth::Desc& desc)
	{
		m_desc = desc;

		// initialize normals
		InitializeNormals();

		// initialize gpu buffers
		InitializeBuffers(m_SimBuffers[0]);
		InitializeBuffers(m_SimBuffers[1]);
		InitializeBufferContents();

		// initialize shaders
		InitializeVertexShader();
		InitializeGeometryShader();
		InitializePixelShader();

		// initialize constant buffer
		InitializeConstantBuffer();

		// initialize rasterizer state
		InitializeRasterizerState();

		// initialize shader
		InitializeShader();
	}

private:
	void UpdateImpl() override
	{
		UpdateBuffer(m_SimBuffers[m_iFrom], m_SimBuffers[m_iFrom ^ 1]);
		m_iFrom ^= 1;
	}

	void RenderImpl() const override
	{
		auto pCTX = DXUTGetD3D11DeviceContext();

		// update constant buffer
		D3D11_MAPPED_SUBRESOURCE cbTestClothRes;
		pCTX->Map(m_pTestClothConstants.get(),
			0, D3D11_MAP_WRITE_DISCARD, 0,
			&cbTestClothRes);
		auto pCamera = GetGlobalCamera();
		auto& cbTestCloth =
			*reinterpret_cast<CB_TEST_CLOTH*>(cbTestClothRes.pData);
		cbTestCloth.WorldView = DirectX::XMMatrixTranspose(pCamera->GetViewMatrix());
		cbTestCloth.Projection = DirectX::XMMatrixTranspose(pCamera->GetProjMatrix());
		cbTestCloth.ClothResolution.x = NDIM_HORIZONTAL;
		cbTestCloth.ClothResolution.y = NDIM_VERTICAL;
		pCTX->Unmap(m_pTestClothConstants.get(), 0);

		pCTX->VSSetShader(m_pTestClothVS.get(),
			nullptr, 0);
		pCTX->GSSetShader(m_pTestClothGS.get(),
			nullptr, 0);
		pCTX->PSSetShader(m_pTestClothPS.get(),
			nullptr, 0);

		// vs shader resources

		// gs shader resources
		ID3D11ShaderResourceView* pSRV =
			m_SimBuffers[m_iFrom].ClothPositionSRV.get();
		ID3D11Buffer* pConstantBuffer = m_pTestClothConstants.get();
		pCTX->GSSetConstantBuffers(0, 1, &pConstantBuffer);
		pCTX->GSSetShaderResources(0, 1, &pSRV);
		pSRV = m_pClothNormalSRV.get();
		pCTX->GSSetShaderResources(1, 1, &pSRV);

		// ps shader resources (no resources)

		// input assembly (no input)
		pCTX->IASetInputLayout(nullptr);
		pCTX->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pCTX->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

		// rasterizer state
		pCTX->RSSetState(m_pRasterizerState.get());

		pCTX->Draw(NDIM_VERTICAL * NDIM_HORIZONTAL, 0);

		pSRV = nullptr;
		pCTX->GSSetShaderResources(0, 1, &pSRV);
		pCTX->GSSetShaderResources(1, 1, &pSRV);
	}

private:
	ComPtr<ID3D11Buffer> m_pClothNormalBuffer;
	ComPtr<ID3D11ShaderResourceView> m_pClothNormalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_pClothNormalUAV;
	ComPtr<ID3D11Buffer> m_pTestClothConstants;
	ComPtr<ID3D11VertexShader> m_pTestClothVS;
	ComPtr<ID3D11GeometryShader> m_pTestClothGS;
	ComPtr<ID3D11PixelShader> m_pTestClothPS;
	ComPtr<ID3D11RasterizerState> m_pRasterizerState;
	ComPtr<ID3D11ComputeShader> m_pUpdateShader;
	ComPtr<ID3D11Buffer> m_pUpdateConstants;
	std::uint32_t m_iFrom = 0;

	TestCloth::Desc m_desc;
	SimulationBuffers m_SimBuffers[2];
};

namespace TestCloth
{
	ObjectHandle CreateObject(const Desc& desc)
	{
		auto ret = new TestClothObject();
		ret->Initialize(desc);

		return ObjectHandle(ret);
	}
}
