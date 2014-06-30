#include "stdafx.h"
#include "TestClothObject.h"
#include "SDKmisc.h"
#include <memory>
#include <amp.h>
#include <amp_math.h>
#include <amp_graphics.h>
#include <DirectXMath.h>
#include <boost/intrusive_ptr.hpp>

namespace cc = concurrency;
namespace ccm = cc::fast_math;
namespace ccg = cc::graphics;

namespace
{
	const int NDIM_HORIZONTAL = 64;
	const int NDIM_VERTICAL = 64;

	void intrusive_ptr_add_ref(IUnknown* p)
	{
		p->AddRef();
	}

	void intrusive_ptr_release(IUnknown* p)
	{
		p->Release();
	}
}

class TestClothObject : public Object
{
private:
	struct SimulationBuffers
	{
		std::unique_ptr<cc::array<ccg::float_4, 2>> ClothPositions;
		std::unique_ptr<cc::array<ccg::float_4, 2>> ClothVelocities;
		boost::intrusive_ptr<ID3D11Buffer> ClothPositionBuffer;
		boost::intrusive_ptr<ID3D11ShaderResourceView> ClothPositionSRV;
	} m_SimBuffers[2];

	struct CB_TEST_CLOTH
	{
		DirectX::XMMATRIX WorldView;
		DirectX::XMMATRIX Projection;
	};

	static void InitializeBuffers(SimulationBuffers& buffers,
		cc::accelerator_view& av)
	{
		buffers.ClothPositions.reset(
			new cc::array<ccg::float_4, 2>(NDIM_HORIZONTAL, NDIM_VERTICAL, av));
		buffers.ClothVelocities.reset(
			new cc::array<ccg::float_4, 2>(NDIM_HORIZONTAL, NDIM_VERTICAL, av));

		InitializePositions(*buffers.ClothPositions);
		InitializeVelocities(*buffers.ClothVelocities);

		IUnknown* pBufferUnknown;
		ID3D11Buffer* pBuffer;
		pBufferUnknown = cc::direct3d::get_buffer(*buffers.ClothPositions);
		if (SUCCEEDED(pBufferUnknown->QueryInterface<ID3D11Buffer>(&pBuffer)))
		{
			boost::intrusive_ptr<ID3D11Buffer>(pBuffer, false)
				.swap(buffers.ClothPositionBuffer);
		}
		pBufferUnknown->Release();

		D3D11_BUFFER_DESC bufferDesc;
		pBuffer->GetDesc(&bufferDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.BufferEx.FirstElement = 0;
		srvDesc.BufferEx.NumElements = bufferDesc.ByteWidth / 4;
		srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;

		ID3D11ShaderResourceView* pSRV;
		DXUTGetD3D11Device()->CreateShaderResourceView(pBuffer, &srvDesc,
			&pSRV);
		boost::intrusive_ptr<ID3D11ShaderResourceView>(pSRV, false)
			.swap(buffers.ClothPositionSRV);
	}

	static void InitializePositions(cc::array<ccg::float_4, 2>& positions)
	{
		cc::parallel_for_each(positions.extent.tile<NDIM_HORIZONTAL, 4>(),
			[&positions](cc::tiled_index<NDIM_HORIZONTAL, 4> idx) restrict(amp)
		{
			const float pfx = idx.global[0] * (1.0f / (NDIM_HORIZONTAL - 1));
			const float pfy = idx.global[1] * (1.0f / (NDIM_VERTICAL - 1));
			const float nfx = 1.0f - pfx;
			const float nfy = 1.0f - pfy;
			const ccg::float_4 CLOTH_POS00 = ccg::float_4(-1.0f, 1.0f, 0.0f, 1.0f);
			const ccg::float_4 CLOTH_POS01 = ccg::float_4(1.0f, 1.0f, 0.0f, 1.0f);
			const ccg::float_4 CLOTH_POS10 = ccg::float_4(-1.0f, -1.0f, 0.0f, 1.0f);
			const ccg::float_4 CLOTH_POS11 = ccg::float_4(1.0f, -1.0f, 0.0f, 1.0f);

			positions[idx.global] =
				nfx * nfy * CLOTH_POS00 +
				pfx * nfy * CLOTH_POS01 +
				nfx * pfy * CLOTH_POS10 +
				pfx * pfy * CLOTH_POS11;
		});
	}

	static void InitializeVelocities(cc::array<ccg::float_4, 2>& velocities)
	{
		cc::parallel_for_each(velocities.extent.tile<NDIM_HORIZONTAL, 4>(),
			[&velocities](cc::tiled_index<NDIM_HORIZONTAL, 4> idx) restrict(amp)
		{
			velocities[idx.global] = ccg::float_4(0.0f, 0.0f, 0.0f, 0.0f);
		});
	}

	static ccg::float_4 CalcAccel(cc::index<2> idx0, cc::index<2> idx1,
		const cc::array<ccg::float_4, 2>& positions,
		const cc::array<ccg::float_4, 2>& velocities,
		float restLen,
		float spring, float damping)
		restrict(amp)
	{
		auto dp = positions[idx1] - positions[idx0];
		auto dv = velocities[idx1] - velocities[idx0];

		float dpLenSq = dp.x * dp.x + dp.y * dp.y + dp.z * dp.z;
		float dpLen = ccm::sqrtf(dpLenSq);

		return (spring * (restLen / dpLen - 1.0f) + 
			damping * (dp.x * dv.x + dp.y * dv.y + dp.z * dv.z) / dpLenSq) * dp;
	}

	static void UpdateBuffer(const SimulationBuffers& buffersFrom,
		SimulationBuffers& buffersTo)
	{
		const auto& posFrom = *buffersFrom.ClothPositions;
		const auto& velFrom = *buffersFrom.ClothVelocities;
		auto& posTo = *buffersTo.ClothPositions;
		auto& velTo = *buffersTo.ClothVelocities;
		const float dt = DXUTGetElapsedTime();

		cc::parallel_for_each(velFrom.extent.tile<NDIM_HORIZONTAL, 4>(),
			[&, dt](cc::tiled_index<NDIM_HORIZONTAL, 4> idx) restrict(amp)
		{
			const float SPRING_N = 1.0f;
			const float DAMPING_N = 0.3f;
			const float SPRING_D = 1.0f;
			const float DAMPING_D = 0.3f;
			const float SPRING_B = 1.0f;
			const float DAMPING_B = 0.3f;
			const float REST_LEN_X = 2.0f / (NDIM_HORIZONTAL - 1);
			const float REST_LEN_Y = 2.0f / (NDIM_VERTICAL - 1);
			const float REST_LEN_XY = ccm::sqrtf(REST_LEN_X * REST_LEN_X
				+ REST_LEN_Y * REST_LEN_Y);

			ccg::float_4 accel(0.0f, 0.0f, 0.0f, 0.0f);

			if (!(idx.global[1] == 0 && (idx.global[0] == 0 || idx.global[0] == NDIM_HORIZONTAL - 1)))
			{
				if (idx.global[0] > 0)
				{
					auto idx2 = idx.global;
					idx2[0]--;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (idx.global[1] > 0)
				{
					auto idx2 = idx.global;
					idx2[1]--;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (idx.global[0] < NDIM_HORIZONTAL - 1)
				{
					auto idx2 = idx.global;
					idx2[0]++;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (idx.global[1] < NDIM_VERTICAL - 1)
				{
					auto idx2 = idx.global;
					idx2[1]++;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				accel.y -= 9.8f;
			}

			velTo[idx.global] = velFrom[idx.global]
				+ accel * dt;
			posTo[idx.global] = posFrom[idx.global]
				+ velTo[idx.global] * dt;
		});
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

		boost::intrusive_ptr<ID3D11VertexShader>(pVS, false)
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

		boost::intrusive_ptr<ID3D11GeometryShader>(pGS, false)
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

		boost::intrusive_ptr<ID3D11PixelShader>(pPS, false)
			.swap(m_pTestClothPS);
	}

	void InitializeConstantBuffer()
	{
		CB_TEST_CLOTH cbTestCloth;
		cbTestCloth.WorldView = DirectX::XMMatrixLookAtLH(
			DirectX::XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f),
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		cbTestCloth.Projection = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(45.0f),
			4.0f / 3.0f,
			0.1f, 1000.0f);

		cbTestCloth.WorldView = DirectX::XMMatrixTranspose(cbTestCloth.WorldView);
		cbTestCloth.Projection = DirectX::XMMatrixTranspose(cbTestCloth.Projection);

		D3D11_BUFFER_DESC constBufDesc;
		ZeroMemory(&constBufDesc, sizeof(constBufDesc));
		constBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constBufDesc.ByteWidth = sizeof(cbTestCloth);
		constBufDesc.Usage = D3D11_USAGE_DYNAMIC;

		D3D11_SUBRESOURCE_DATA constBufSubRes;
		ZeroMemory(&constBufSubRes, sizeof(constBufSubRes));
		constBufSubRes.pSysMem = &cbTestCloth;

		ID3D11Buffer* pConstBuffer;
		assert(SUCCEEDED(DXUTGetD3D11Device()->CreateBuffer(&constBufDesc,
			&constBufSubRes, &pConstBuffer)));
		boost::intrusive_ptr<ID3D11Buffer>(pConstBuffer, false)
			.swap(m_pTestClothConstants);
	}

	void InitializeRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerDesc;
		ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
		rasterizerDesc.CullMode = D3D11_CULL_NONE;
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.DepthClipEnable = TRUE;

		ID3D11RasterizerState* pRasterizerState;
		assert(SUCCEEDED(DXUTGetD3D11Device()
			->CreateRasterizerState(&rasterizerDesc,
			&pRasterizerState)));

		boost::intrusive_ptr<ID3D11RasterizerState>(pRasterizerState, false)
			.swap(m_pRasterizerState);
	}

public:
	void Initialize()
	{
		m_AccelView.reset(new cc::accelerator_view(
			cc::direct3d::create_accelerator_view(DXUTGetD3D11Device())));

		// initialize gpu buffers
		InitializeBuffers(m_SimBuffers[0], *m_AccelView);
		InitializeBuffers(m_SimBuffers[1], *m_AccelView);

		// initialize shaders
		InitializeVertexShader();
		InitializeGeometryShader();
		InitializePixelShader();

		// initialize constant buffer
		InitializeConstantBuffer();

		// initialize 
		InitializeRasterizerState();
	}

private:
	void UpdateImpl() override
	{
		UpdateBuffer(m_SimBuffers[m_iFrom], m_SimBuffers[m_iFrom ^ 1]);
		m_iFrom ^= m_iFrom;
	}

	void RenderImpl() const override
	{
		auto pCTX = DXUTGetD3D11DeviceContext();
		pCTX->VSSetShader(m_pTestClothVS.get(),
			nullptr, 0);
		pCTX->GSSetShader(m_pTestClothGS.get(),
			nullptr, 0);
		pCTX->PSSetShader(m_pTestClothPS.get(),
			nullptr, 0);

		// vs shader resources
		ID3D11ShaderResourceView* pSRV =
			m_SimBuffers[m_iFrom].ClothPositionSRV.get();
		pCTX->VSSetShaderResources(0, 1, &pSRV);

		// gs shader resources
		ID3D11Buffer* pConstantBuffer = m_pTestClothConstants.get();
		pCTX->GSSetConstantBuffers(0, 1, &pConstantBuffer);

		// ps shader resources (no resources)

		// input assembly (no input)
		pCTX->IASetInputLayout(nullptr);
		pCTX->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pCTX->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

		// rasterizer state
		pCTX->RSSetState(m_pRasterizerState.get());

		pCTX->Draw(NDIM_VERTICAL * NDIM_HORIZONTAL, 0);

		pSRV = nullptr;
		pCTX->VSSetShaderResources(0, 1, &pSRV);
	}

private:
	std::unique_ptr<cc::accelerator_view> m_AccelView;
	boost::intrusive_ptr<ID3D11Buffer> m_pTestClothConstants;
	boost::intrusive_ptr<ID3D11VertexShader> m_pTestClothVS;
	boost::intrusive_ptr<ID3D11GeometryShader> m_pTestClothGS;
	boost::intrusive_ptr<ID3D11PixelShader> m_pTestClothPS;
	boost::intrusive_ptr<ID3D11RasterizerState> m_pRasterizerState;
	std::uint32_t m_iFrom = 0;
};

ObjectHandle CreateTestClothObject()
{
	auto ret = new TestClothObject();
	ret->Initialize();

	return ObjectHandle(ret);
}
