#include "stdafx.h"
#include "TestClothObject.h"
#include "Globals.h"
#include <memory>
#include <amp.h>
#include <amp_math.h>
#include <amp_graphics.h>
#include <boost/intrusive_ptr.hpp>

namespace cc = concurrency;
namespace ccm = cc::fast_math;
namespace ccg = cc::graphics;

namespace
{
	const int NDIM_HORIZONTAL = 128;
	const int NDIM_VERTICAL = 128;

	// for determining max value at compile time
	template <int A, int B, bool C = (A < B)> struct CT_MAX
	{
		enum { value = B };
	};

	// for determining max value at compile time
	template <int A, int B> struct CT_MAX < A, B, false >
	{
		enum { value = A };
	};

	// for determining min value at compile time
	template <int A, int B, bool C = (A < B)> struct CT_MIN
	{
		enum { value = A };
	};

	// for determining min value at compile time
	template <int A, int B> struct CT_MIN < A, B, false >
	{
		enum { value = B };
	};

	const int X_TILE = CT_MIN<256, NDIM_HORIZONTAL>::value;
	const int Y_TILE = CT_MAX<256 / NDIM_HORIZONTAL, 1>::value;

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
		boost::intrusive_ptr<ID3D11ShaderResourceView> ClothPositionSRV;
		boost::intrusive_ptr<ID3D11Buffer> ClothPositionBuffer;
	} m_SimBuffers[2];

	struct CB_TEST_CLOTH
	{
		DirectX::XMMATRIX WorldView;
		DirectX::XMMATRIX Projection;
		DirectX::XMUINT2 ClothResolution;
		DirectX::XMUINT2 dummy;
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
		cc::parallel_for_each(positions.extent.tile<X_TILE, Y_TILE>(),
			[&positions](cc::tiled_index<X_TILE, Y_TILE> idx) restrict(amp)
		{
			const float pfx = idx.global[0] * (1.0f / (NDIM_HORIZONTAL - 1));
			const float pfy = idx.global[1] * (1.0f / (NDIM_VERTICAL - 1));
			const float nfx = 1.0f - pfx;
			const float nfy = 1.0f - pfy;
			const float SQRT_2 = ccm::sqrtf(2.0f);
			const ccg::float_4 CLOTH_POS00(-1.0f, 1.0f, 0.0f, 1.0f);
			const ccg::float_4 CLOTH_POS01(1.0f, 1.0f, 0.0f, 1.0f);
			const ccg::float_4 CLOTH_POS10(-1.0f, 1.0f - SQRT_2, SQRT_2, 1.0f);
			const ccg::float_4 CLOTH_POS11(1.0f, 1.0f - SQRT_2, SQRT_2, 1.0f);

			positions[idx.global] =
				nfx * nfy * CLOTH_POS00 +
				pfx * nfy * CLOTH_POS01 +
				nfx * pfy * CLOTH_POS10 +
				pfx * pfy * CLOTH_POS11;
		});
	}

	static void InitializeVelocities(cc::array<ccg::float_4, 2>& velocities)
	{
		cc::parallel_for_each(velocities.extent.tile<X_TILE, Y_TILE>(),
			[&velocities](cc::tiled_index<X_TILE, Y_TILE> idx) restrict(amp)
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
		auto dp = positions[idx0] - positions[idx1];
		auto dv = velocities[idx0] - velocities[idx1];

		float dpLenSq = dp.x * dp.x + dp.y * dp.y + dp.z * dp.z;
		float dpLen = ccm::sqrtf(dpLenSq);

		return (
			spring * (restLen / dpLen - 1.0f) +
			- damping * (dp.x * dv.x + dp.y * dv.y + dp.z * dv.z) / dpLenSq) * dp;
	}

	static ccg::float_4 CalcNormal(cc::index<2> idx0,
		cc::index<2> idx1,
		cc::index<2> idx2,
		const cc::array<ccg::float_4, 2>& positions) restrict(amp)
	{
		auto d0 = positions[idx1] - positions[idx0];
		auto d1 = positions[idx2] - positions[idx0];

		return ccg::float_4(
			d0.y * d1.z - d0.z * d1.y,
			d0.z * d1.x - d0.x * d1.z,
			d0.x * d1.y - d0.y * d1.x,
			0.0f);
	}

	static void UpdateBuffer(const SimulationBuffers& buffersFrom,
		SimulationBuffers& buffersTo,
		cc::array<ccg::float_4, 2>& normals)
	{
		const auto& posFrom = *buffersFrom.ClothPositions;
		const auto& velFrom = *buffersFrom.ClothVelocities;
		auto& posTo = *buffersTo.ClothPositions;
		auto& velTo = *buffersTo.ClothVelocities;
		const float dt = 0.001f;// DXUTGetElapsedTime();

		cc::parallel_for_each(velFrom.extent.tile<X_TILE, Y_TILE>(),
			[&, dt](cc::tiled_index<X_TILE, Y_TILE> idx) restrict(amp)
		{
			const float SPRING_N = 100.0f / dt;
			const float DAMPING_N = 3.1f;
			const float SPRING_D = 100.0f / dt;
			const float DAMPING_D = 3.1f;
			const float SPRING_B = 100.0f / dt;
			const float DAMPING_B = 3.1f;
			const float REST_LEN_X = 2.0f / (NDIM_HORIZONTAL - 1);
			const float REST_LEN_Y = 2.0f / (NDIM_VERTICAL - 1);
			const float REST_LEN_XY = ccm::sqrtf(REST_LEN_X * REST_LEN_X
				+ REST_LEN_Y * REST_LEN_Y);

			ccg::float_4 accel(0.0f, 0.0f, 0.0f, 0.0f);

			const int X_MIN = idx.global[0] == 0;
			const int X_MAX = idx.global[0] == NDIM_HORIZONTAL - 1;
			const int Y_MIN = idx.global[1] == 0;
			const int Y_MAX = idx.global[1] == NDIM_VERTICAL - 1;
			const int X_MIN2 = idx.global[0] < 2;
			const int X_MAX2 = idx.global[0] >= NDIM_HORIZONTAL - 2;
			const int Y_MIN2 = idx.global[1] < 2;
			const int Y_MAX2 = idx.global[1] >= NDIM_VERTICAL - 2;

			// if (!(Y_MIN && (X_MIN || X_MAX)))
			if (!Y_MIN)
			{
				if (!X_MIN)
				{
					auto idx2 = idx.global;
					idx2[0]--;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (!Y_MIN)
				{
					auto idx2 = idx.global;
					idx2[1]--;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (!X_MAX)
				{
					auto idx2 = idx.global;
					idx2[0]++;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (!Y_MAX)
				{
					auto idx2 = idx.global;
					idx2[1]++;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X,
						SPRING_N,
						DAMPING_N);
				}

				if (!X_MIN && !Y_MIN)
				{
					auto idx2 = idx.global;
					idx2[0]--;
					idx2[1]--;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_XY,
						SPRING_D,
						DAMPING_D);
				}

				if (!X_MAX && !Y_MIN)
				{
					auto idx2 = idx.global;
					idx2[0]++;
					idx2[1]--;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_XY,
						SPRING_D,
						DAMPING_D);
				}

				if (!X_MIN && !Y_MAX)
				{
					auto idx2 = idx.global;
					idx2[0]--;
					idx2[1]++;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_XY,
						SPRING_D,
						DAMPING_D);
				}

				if (!X_MAX && !Y_MAX)
				{
					auto idx2 = idx.global;
					idx2[0]++;
					idx2[1]++;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_XY,
						SPRING_D,
						DAMPING_D);
				}

				if (!X_MIN2)
				{
					auto idx2 = idx.global;
					idx2[0] -= 2;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X * 2.0f,
						SPRING_B,
						DAMPING_B);
				}

				if (!X_MAX2)
				{
					auto idx2 = idx.global;
					idx2[0] += 2;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_X * 2.0f,
						SPRING_B,
						DAMPING_B);
				}

				if (!Y_MIN2)
				{
					auto idx2 = idx.global;
					idx2[1] -= 2;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_Y * 2.0f,
						SPRING_B,
						DAMPING_B);
				}

				if (!Y_MAX2)
				{
					auto idx2 = idx.global;
					idx2[1] += 2;
					accel += CalcAccel(idx.global, idx2,
						posFrom, velFrom,
						REST_LEN_Y * 2.0f,
						SPRING_B,
						DAMPING_B);
				}

				accel.y -= 9.8f;
			}

			ccg::float_4 normal(0.0f, 0.0f, 0.0f, 0.0f);

			velTo[idx.global] = velFrom[idx.global]
				+ accel * dt;
			posTo[idx.global] = posFrom[idx.global]
				+ velTo[idx.global] * dt;

			if (!X_MIN && !Y_MIN)
			{
				auto idx1 = idx.global;
				auto idx2 = idx.global;
				idx1[0]--;
				idx2[1]--;
				normal += CalcNormal(idx.global, idx1, idx2,
					posTo);
			}

			if (!X_MAX && !Y_MIN)
			{
				auto idx1 = idx.global;
				auto idx2 = idx.global;
				idx1[1]--;
				idx2[0]++;
				normal += CalcNormal(idx.global, idx1, idx2,
					posTo);
			}

			if (!X_MIN && !Y_MAX)
			{
				auto idx1 = idx.global;
				auto idx2 = idx.global;
				idx2[0]--;
				idx1[1]++;
				normal += CalcNormal(idx.global, idx1, idx2,
					posTo);
			}

			if (!X_MAX && !Y_MAX)
			{
				auto idx1 = idx.global;
				auto idx2 = idx.global;
				idx1[0]++;
				idx2[1]++;
				normal += CalcNormal(idx.global, idx1, idx2,
					posTo);
			}

			float n = ccm::sqrtf(normal.x * normal.x +
				normal.y * normal.y + normal.z * normal.z);

			normals[idx.global] = normal / n;
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
		D3D11_BUFFER_DESC constBufDesc;
		ZeroMemory(&constBufDesc, sizeof(constBufDesc));
		constBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constBufDesc.ByteWidth = sizeof(CB_TEST_CLOTH);
		constBufDesc.Usage = D3D11_USAGE_DYNAMIC;

		ID3D11Buffer* pConstBuffer;
		assert(SUCCEEDED(DXUTGetD3D11Device()->CreateBuffer(&constBufDesc,
			nullptr, &pConstBuffer)));
		boost::intrusive_ptr<ID3D11Buffer>(pConstBuffer, false)
			.swap(m_pTestClothConstants);
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

		boost::intrusive_ptr<ID3D11RasterizerState>(pRasterizerState, false)
			.swap(m_pRasterizerState);
	}

	void InitializeNormals()
	{
		m_ClothNormals.reset(
			new cc::array<ccg::float_4, 2>
			(NDIM_HORIZONTAL, NDIM_VERTICAL, *m_AccelView));

		IUnknown* pBufferUnknown;
		ID3D11Buffer* pBuffer;
		pBufferUnknown = cc::direct3d::get_buffer(*m_ClothNormals);
		if (SUCCEEDED(pBufferUnknown->QueryInterface<ID3D11Buffer>(&pBuffer)))
		{
			boost::intrusive_ptr<ID3D11Buffer>(pBuffer, false)
				.swap(m_pClothNormalBuffer);
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
			.swap(m_pClothNormalSRV);
	}

public:
	void Initialize()
	{
		m_AccelView.reset(new cc::accelerator_view(
			cc::direct3d::create_accelerator_view(DXUTGetD3D11Device())));

		// initialize normals
		InitializeNormals();

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
		UpdateBuffer(m_SimBuffers[m_iFrom], m_SimBuffers[m_iFrom ^ 1],
			*m_ClothNormals);
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
	std::unique_ptr<cc::accelerator_view> m_AccelView;
	std::unique_ptr<cc::array<ccg::float_4, 2>> m_ClothNormals;
	boost::intrusive_ptr<ID3D11Buffer> m_pClothNormalBuffer;
	boost::intrusive_ptr<ID3D11ShaderResourceView> m_pClothNormalSRV;
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
