#include <MaterialPreviewRenderer.h>
#include <Assets/Texture.h>
#include <Graphics/GraphicsModule.h>
#include <Fallbacks/FallbacksModule.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <array>
#include <algorithm>

namespace Nuclear::Editor
{
	bool MaterialPreviewRenderer::Initialize(Assets::Shader& source, Components::CameraComponent& camera)
	{
		using namespace Diligent;
		if (source.GetShaderBuildDesc().mType != Graphics::ShaderType::Rendering3D ||
			!source.GetShaderBuildDesc().mPipelineDesc.mForwardPSOCreateInfo.mValid) return false;
		mBuildDesc = source.GetShaderBuildDesc();
		mReflection = source.GetReflection();
		mBuildDesc.mPipelineDesc.Switches.clear();
		mBuildDesc.mSupportedTechniques = Graphics::SupportedRenderingTechnique::ForwardOnly;
		mBuildDesc.mPipelineDesc.mName += " / Material preview";
		auto& pipeline = mBuildDesc.mPipelineDesc.mForwardPSOCreateInfo.GraphicsPipeline;
		pipeline.NumRenderTargets = 1;
		pipeline.RTVFormats[0] = camera.GetColorRT().GetRTV()->GetTexture()->GetDesc().Format;
		pipeline.DSVFormat = camera.GetDepthRT().GetRTV()->GetTexture()->GetDesc().Format;

		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		BufferDesc buffer;
		buffer.Name = "Material preview camera";
		buffer.Size = sizeof(Components::CameraBuffer);
		buffer.Usage = USAGE_DYNAMIC;
		buffer.BindFlags = BIND_UNIFORM_BUFFER;
		buffer.CPUAccessFlags = CPU_ACCESS_WRITE;
		device->CreateBuffer(buffer, nullptr, &mCameraBuffer);
		buffer.Name = "Material preview lights";
		buffer.Size = 5 * sizeof(Math::Vector4);
		device->CreateBuffer(buffer, nullptr, &mLightBuffer);
		if (!mCameraBuffer || !mLightBuffer) return false;

		Graphics::Rendering3DShaderBakingDesc lighting;
		lighting.DirLights = 2;
		Graphics::ShaderPipelineBakingDesc bake;
		bake.pRendering3DBakingDesc = &lighting;
		bake.pVariantsFactory = &Graphics::GraphicsModule::Get().GetDefaultShaderPipelineVariantFactory();
		bake.mDefines.insert("NE_DIR_LIGHTS_NUM 2");
		bake.mStaticVariablesBindings = {{SHADER_TYPE_VERTEX, "NEStatic_Camera", mCameraBuffer},
			{SHADER_TYPE_PIXEL, "NEStatic_Lights", mLightBuffer}};
		mPipeline.BuildVariants();
		if (!mPipeline.Bake(bake)) return false;
		pSourceShader = &source;
		return mPipeline.GetVariant(0) && mPipeline.GetVariant(0)->mPipeline;
	}

	void MaterialPreviewRenderer::PrepareMaterial(Assets::Material& source, Uint32 textureSet)
	{
		pMaterial = std::make_unique<Assets::Material>();
		Assets::MaterialTextureSet set;
		if (!source.GetTextures().empty())
			set = source.GetTextures()[std::min<size_t>(textureSet, source.GetTextures().size() - 1)];
		auto& fallbacks = Fallbacks::FallbacksModule::Get();
		const std::array<Assets::MaterialTexture, 5> defaults = {{
			{fallbacks.GetDefaultWhiteImage(), Assets::TextureUsageType::Albedo},
			{fallbacks.GetDefaultBlackImage(), Assets::TextureUsageType::Metallic},
			fallbacks.GetDefaultNormalTex(),
			{fallbacks.GetDefaultGreyImage(), Assets::TextureUsageType::Roughness},
			{fallbacks.GetDefaultWhiteImage(), Assets::TextureUsageType::AO}}};
		for (const auto& fallback : defaults)
		{
			auto found = std::find_if(set.mData.begin(), set.mData.end(), [&](const auto& map) {
				return map.mUsageType == fallback.mUsageType;
			});
			if (found == set.mData.end()) set.mData.push_back(fallback);
			else if (!found->pTexture || !found->pTexture->GetTextureView()) *found = fallback;
		}
		pMaterial->GetTextures().push_back(std::move(set));
		pMaterial->Create(this);
		pSourceMaterial = &source;
		mTextureSet = textureSet;
	}

	bool MaterialPreviewRenderer::Render(Assets::Mesh& mesh, Components::CameraComponent& camera,
		Assets::Material& material, Uint32 textureSet)
	{
		using namespace Diligent;
		auto* shader = material.GetShader();
		if (!shader || material.GetState() != Assets::IAsset::State::Created) return false;
		if (!pSourceShader && !Initialize(*shader, camera)) return false;
		// Each document/thumbnail renderer has one source shader for its lifetime.
		if (pSourceShader != shader) return false;
		if (!pMaterial || pSourceMaterial != &material || mTextureSet != textureSet)
			PrepareMaterial(material, textureSet);
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		camera.SetModelMatrix(Math::Matrix4(1));
		{
			MapHelper<Components::CameraBuffer> constants(context, mCameraBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			*constants = camera.mCameraData;
		}
		{
			MapHelper<Math::Vector4> lights(context, mLightBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			lights[0] = Math::Vector4(camera.GetPosition(), 1);
			lights[1] = Math::Vector4(Math::normalize(Math::Vector3(-.6f, -1, -.8f)), 0);
			lights[2] = {1, .96f, .9f, 3};
			lights[3] = Math::Vector4(Math::normalize(Math::Vector3(.8f, -.2f, .5f)), 0);
			lights[4] = {.65f, .78f, 1, 1.5f};
		}
		auto* variant = mPipeline.GetVariant(0);
		context->SetPipelineState(variant->mPipeline);
		pMaterial->BindTexSet(variant, 0);
		for (auto& part : mesh.mSubMeshes)
		{
			if (!part.mVB || !part.mIB || !part.mIndicesCount) continue;
			IBuffer* vertex = part.mVB;
			const Uint64 offset = 0;
			context->SetVertexBuffers(0, 1, &vertex, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
			context->SetIndexBuffer(part.mIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			DrawIndexedAttribs draw;
			draw.IndexType = VT_UINT32;
			draw.NumIndices = part.mIndicesCount;
			draw.Flags = DRAW_FLAG_VERIFY_ALL;
			context->DrawIndexed(draw);
		}
		return true;
	}
}
