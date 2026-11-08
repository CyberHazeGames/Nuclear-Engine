#include <MeshPreviewCache.h>
#include <Graphics/GraphicsModule.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cmath>

namespace Nuclear::Editor
{
	using namespace Diligent;

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
    output.Position = mul(float4(input.Position, 1.0), ModelViewProjection);
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

	MeshPreviewCache::MeshPreviewCache() = default;

	MeshPreviewCache::~MeshPreviewCache()
	{
		Clear();
	}

	void MeshPreviewCache::Clear()
	{
		mCachedTextures.clear();
		mPipeline.Release();
		mBinding.Release();
		mConstantBuffer.Release();
		mDepthTarget.Release();
		mInitialized = false;
	}

	bool MeshPreviewCache::InitializePipeline()
	{
		if (mInitialized)
			return mPipeline != nullptr;

		mInitialized = true;
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

		TextureDesc depthDesc;
		depthDesc.Name = "Mesh preview depth target";
		depthDesc.Type = RESOURCE_DIM_TEX_2D;
		depthDesc.Width = 128;
		depthDesc.Height = 128;
		depthDesc.Format = TEX_FORMAT_D32_FLOAT;
		depthDesc.Usage = USAGE_DEFAULT;
		depthDesc.BindFlags = BIND_DEPTH_STENCIL;
		device->CreateTexture(depthDesc, nullptr, &mDepthTarget);
		if (!mDepthTarget)
			return false;

		RefCntAutoPtr<IShader> vs;
		RefCntAutoPtr<IShader> ps;
		Graphics::GraphicsModule::Get().CreateShader(PreviewVS, &vs, SHADER_TYPE_VERTEX);
		Graphics::GraphicsModule::Get().CreateShader(PreviewPS, &ps, SHADER_TYPE_PIXEL);
		if (!vs || !ps)
			return false;

		GraphicsPipelineStateCreateInfo psoCI;
		psoCI.PSODesc.Name = "Mesh preview thumbnail pipeline";
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

	Assets::Texture* MeshPreviewCache::GetPreview(Assets::Mesh* mesh)
	{
		if (!mesh)
			return nullptr;

		auto it = mCachedTextures.find(mesh);
		if (it != mCachedTextures.end())
			return it->second.get();

		if (mesh->GetState() != Assets::IAsset::State::Created)
			return nullptr;

		if (!InitializePipeline())
			return nullptr;

		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		if (!device || !context)
			return nullptr;

		TextureDesc colorDesc;
		colorDesc.Name = "Mesh preview color target";
		colorDesc.Type = RESOURCE_DIM_TEX_2D;
		colorDesc.Width = 128;
		colorDesc.Height = 128;
		colorDesc.Format = TEX_FORMAT_RGBA8_UNORM;
		colorDesc.Usage = USAGE_DEFAULT;
		colorDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

		RefCntAutoPtr<ITexture> colorTarget;
		device->CreateTexture(colorDesc, nullptr, &colorTarget);
		if (!colorTarget)
			return nullptr;

		auto* rtv = colorTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
		auto* dsv = mDepthTarget->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

		context->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clearColor[] = {0.08f, 0.10f, 0.13f, 1.0f};
		context->ClearRenderTarget(rtv, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		context->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		Viewport vp;
		vp.Width = 128.0f;
		vp.Height = 128.0f;
		context->SetViewports(1, &vp, 128, 128);

		Math::Vector3 center(0.0f);
		float distance = 2.5f;
		size_t totalVerts = 0;
		Math::Vector3 minBounds(1e9f);
		Math::Vector3 maxBounds(-1e9f);
		for (const auto& part : mesh->mSubMeshes)
		{
			for (const auto& v : part.data.Vertices)
			{
				minBounds = glm::min(minBounds, v.Position);
				maxBounds = glm::max(maxBounds, v.Position);
				++totalVerts;
			}
		}
		if (totalVerts > 0 && minBounds.x <= maxBounds.x)
		{
			center = (minBounds + maxBounds) * 0.5f;
			const Math::Vector3 extent = maxBounds - minBounds;
			const float maxExtent = glm::max(extent.x, glm::max(extent.y, extent.z));
			if (maxExtent > 0.001f)
				distance = maxExtent * 1.8f;
		}

		Constants constants;
		const Math::Matrix4 proj = Math::perspective(Math::radians(45.0f), 1.0f, 0.05f, 500.0f);
		const Math::Vector3 eye = center + glm::normalize(Math::Vector3(1.2f, 1.0f, 1.5f)) * distance;
		const Math::Matrix4 view = Math::lookAt(eye, center, Math::Vector3(0.0f, 1.0f, 0.0f));
		constants.ModelViewProjection = proj * view;
		constants.LightDirection = Math::Vector4(glm::normalize(Math::Vector3(0.6f, 1.0f, 0.8f)), 0.0f);
		constants.BaseColor = Math::Vector4(0.72f, 0.78f, 0.86f, 1.0f);

		{
			MapHelper<Constants> mapped(context, mConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			*mapped = constants;
		}

		context->SetPipelineState(mPipeline);
		context->CommitShaderResources(mBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		for (auto& submesh : mesh->mSubMeshes)
		{
			if (!submesh.mVB || !submesh.mIB || !submesh.mIndicesCount)
				continue;
			IBuffer* vertex = submesh.mVB;
			const Uint64 offset = 0;
			context->SetVertexBuffers(0, 1, &vertex, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
			context->SetIndexBuffer(submesh.mIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			DrawIndexedAttribs draw;
			draw.IndexType = VT_UINT32;
			draw.NumIndices = submesh.mIndicesCount;
			draw.Flags = DRAW_FLAG_VERIFY_ALL;
			context->DrawIndexed(draw);
		}

		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

		auto textureAsset = std::make_unique<Assets::Texture>();
		textureAsset->SetTextureView(colorTarget->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
		textureAsset->SetState(Assets::IAsset::State::Created);

		auto* resultPtr = textureAsset.get();
		mCachedTextures[mesh] = std::move(textureAsset);
		return resultPtr;
	}
} // namespace Nuclear::Editor
