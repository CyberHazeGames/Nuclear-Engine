#include <MeshPreviewScene.h>
#include <MaterialPreviewRenderer.h>
#include <Graphics/GraphicsModule.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace Nuclear::Editor
{
	using namespace Diligent;

	MeshPreviewScene::MeshPreviewScene() : mCamera([]() {
		Components::CameraComponentDesc desc;
		desc.mInitRTs = false;
		desc.mIsActiveCamera = false;
		desc.mProjection = Math::Matrix4(1);
		desc.mPitch = -10;
		return desc;
	}()) {}

	MeshPreviewScene::~MeshPreviewScene() = default;

	void MeshPreviewScene::FrameMesh()
	{
		// Reconstruct direction from the current camera, placing it outside the bounds.
		mCamera.UpdateBuffer();
		const auto size = mCamera.GetColorRT().GetDimensions();
		const float aspectFit = size.x ? std::max(1.f, float(size.y) / size.x) : 1.f;
		mCamera.SetPosition(mCenter - mCamera.GetFrontView() * mRadius * 3.5f * aspectFit);
		mCamera.MovementSpeed = mRadius * 2;
		mCamera.UpdateBuffer();
	}

	void MeshPreviewScene::MeasureMesh(Assets::Mesh& mesh)
	{
		Math::Vector3 minimum(std::numeric_limits<float>::max());
		Math::Vector3 maximum(std::numeric_limits<float>::lowest());
		bool found = false;
		auto vertex = [&](const Assets::Mesh::Vertex& value) {
			const auto p = value.Position;
			if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return;
			minimum = Math::min(minimum, p);
			maximum = Math::max(maximum, p);
			found = true;
		};
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		for (auto& part : mesh.mSubMeshes)
		{
			if (!part.data.Vertices.empty())
			{
				for (const auto& value : part.data.Vertices) vertex(value);
				continue;
			}
			if (!part.mVB) continue;
			auto desc = part.mVB->GetDesc();
			desc.Name = "Mesh preview bounds readback";
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			desc.Mode = BUFFER_MODE_UNDEFINED;
			desc.ElementByteStride = 0;
			RefCntAutoPtr<IBuffer> staging;
			Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(desc, nullptr, &staging);
			if (!staging) continue;
			context->CopyBuffer(part.mVB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
				staging, 0, desc.Size, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			context->WaitForIdle();
			MapHelper<Assets::Mesh::Vertex> mapped(context, staging, MAP_READ, MAP_FLAG_NONE);
			if (mapped)
				for (Uint64 i = 0; i < desc.Size / sizeof(Assets::Mesh::Vertex); ++i) vertex(mapped[i]);
		}
		if (found)
		{
			mCenter = minimum * .5f + maximum * .5f;
			const float radius = Math::length(maximum - mCenter);
			if (std::isfinite(radius)) mRadius = std::max(.01f, radius);
		}
		mMeasured = true;
		FrameMesh();
	}

	bool MeshPreviewScene::Render(Assets::Mesh& mesh, Uint32 width, Uint32 height, Assets::Material* material, Uint32 textureSet)
	{
		if (!width || !height || mesh.GetState() != Assets::IAsset::State::Created) return false;
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		if (!mInitialized)
		{
			Graphics::RenderTargetDesc color;
			color.mName = "Mesh document color";
			color.mDimensions = {width, height};
			if (material) color.ColorTexFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
			Graphics::RenderTargetDesc depth = color;
			depth.mName = "Mesh document depth";
			depth.DepthTexFormat = TEX_FORMAT_D32_FLOAT;
			mCamera.CreateRenderTargets(color, depth);
			if (!mCamera.GetColorRT().GetRTV() || !mCamera.GetDepthRT().GetRTV()) return false;
			if ((!material && !InitializePipeline()) || !mSkybox.Initialize(mCamera)) return false;
			if (material) pMaterialRenderer = std::make_unique<MaterialPreviewRenderer>();
			mInitialized = true;
		}
		if (!mMeasured) MeasureMesh(mesh);
		if (mCamera.GetColorRT().GetDimensions() != Math::Vector2ui(width, height))
			mCamera.ResizeRTs(width, height);
		mCamera.SetProjectionMatrix(Math::perspective(Math::radians(45.f), float(width) / height,
			mRadius * .001f, mRadius * 1000.f));
		mCamera.UpdateBuffer();
		mTexture.SetTextureView(mCamera.GetColorRT().GetSRV());
		mTexture.SetState(Assets::IAsset::State::Created);
		auto* rtv = mCamera.GetColorRT().GetRTV();
		auto* dsv = mCamera.GetDepthRT().GetRTV();
		context->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clear[] = {.08f, .1f, .13f, 1};
		context->ClearRenderTarget(rtv, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		context->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		Viewport viewport;
		viewport.Width = float(width);
		viewport.Height = float(height);
		context->SetViewports(1, &viewport, width, height);
		if (material)
		{
			if (!pMaterialRenderer->Render(mesh, mCamera, *material, textureSet))
			{
				context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
				return false;
			}
		}
		else
		{
			{
				MapHelper<Constants> mapped(context, mConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
				mapped->ModelViewProjection = mCamera.mCameraData.Projection * mCamera.mCameraData.View;
				mapped->LightDirection = Math::Vector4(Math::normalize(Math::Vector3(.6f, 1, .8f)), 0);
				mapped->BaseColor = {.72f, .78f, .86f, 1};
			}
			context->SetPipelineState(mPipeline);
			context->CommitShaderResources(mBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			for (auto& part : mesh.mSubMeshes)
			{
				if (!part.mVB || !part.mIB || !part.mIndicesCount) continue;
				IBuffer* buffer = part.mVB;
				const Uint64 offset = 0;
				context->SetVertexBuffers(0, 1, &buffer, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
					SET_VERTEX_BUFFERS_FLAG_RESET);
				context->SetIndexBuffer(part.mIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				DrawIndexedAttribs draw;
				draw.IndexType = VT_UINT32;
				draw.NumIndices = part.mIndicesCount;
				draw.Flags = DRAW_FLAG_VERIFY_ALL;
				context->DrawIndexed(draw);
			}
		}
		mSkybox.Render(mCamera);
		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
		return true;
	}
	static const char* PreviewVS = R"(
cbuffer PreviewConstants
{
    float4x4 ModelViewProjection;
    float4 LightDirection;
    float4 BaseColor;
};

struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Position = mul(ModelViewProjection, float4(input.Position, 1.0));
    output.Normal = input.Normal;
    return output;
}
)";

	static const char* PreviewPS = R"(
cbuffer PreviewConstants
{
    float4x4 ModelViewProjection;
    float4 LightDirection;
    float4 BaseColor;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 N = length(input.Normal) > 0.001 ? normalize(input.Normal) : float3(0.0, 1.0, 0.0);
    float3 L = normalize(LightDirection.xyz);
    float NdotL = max(dot(N, L), 0.0);
    float3 ambient = BaseColor.rgb * 0.35;
    float3 diffuse = BaseColor.rgb * NdotL * 0.65;
    return float4(ambient + diffuse, 1.0);
}
)";

	bool MeshPreviewScene::InitializePipeline()
	{
		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		if (!device)
			return false;

		BufferDesc bufferDesc;
		bufferDesc.Name = "Mesh preview constants";
		bufferDesc.Size = sizeof(Constants);
		bufferDesc.Usage = USAGE_DYNAMIC;
		bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
		bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		device->CreateBuffer(bufferDesc, nullptr, &mConstantBuffer);
		if (!mConstantBuffer)
			return false;

		RefCntAutoPtr<IShader> vs;
		RefCntAutoPtr<IShader> ps;
		Graphics::GraphicsModule::Get().CreateShader(PreviewVS, &vs, SHADER_TYPE_VERTEX);
		Graphics::GraphicsModule::Get().CreateShader(PreviewPS, &ps, SHADER_TYPE_PIXEL);
		if (!vs || !ps)
			return false;

		GraphicsPipelineStateCreateInfo psoCI;
		psoCI.PSODesc.Name = "Mesh document preview pipeline";
		psoCI.GraphicsPipeline.NumRenderTargets = 1;
		psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
		psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
		psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
		psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
		psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
		psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

		const LayoutElement layout[] = {
			{0, 0, 3, VT_FLOAT32, false, offsetof(Assets::Mesh::Vertex, Position), sizeof(Assets::Mesh::Vertex)},
			{1, 0, 3, VT_FLOAT32, false, offsetof(Assets::Mesh::Vertex, Normal), sizeof(Assets::Mesh::Vertex)}};
		psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
		psoCI.GraphicsPipeline.InputLayout.NumElements = 2;
		psoCI.pVS = vs;
		psoCI.pPS = ps;

		psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
		device->CreateGraphicsPipelineState(psoCI, &mPipeline);
		if (!mPipeline)
			return false;

		auto* cbVarVS = mPipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "PreviewConstants");
		if (cbVarVS)
			cbVarVS->Set(mConstantBuffer);
		auto* cbVarPS = mPipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PreviewConstants");
		if (cbVarPS)
			cbVarPS->Set(mConstantBuffer);

		mPipeline->CreateShaderResourceBinding(&mBinding, true);
		return mBinding != nullptr;
	}

}
