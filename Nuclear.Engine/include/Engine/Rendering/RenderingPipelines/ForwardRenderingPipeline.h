#pragma once
#include <Engine/Rendering/RenderingPipeline.h>

namespace Nuclear
{
	namespace Rendering
	{
		class NEAPI ForwardRenderingPipeline : public RenderingPipeline
		{
		public:
			void BeginFrame(FrameRenderData* frame) override;

			void StartStaticShaderModelRendering(ShadingModel* shadingmodel) override;

			void Render(Components::MeshComponent& mesh, const Math::Matrix4& modelmatrix) override;

			void FinishStaticShaderModelRendering() override;

			void StartSkinnedShaderModelRendering(ShadingModel* shadingmodel) override;

			void Render(Components::SkinnedMeshComponent& mesh, const Math::Matrix4& modelmatrix) override;

		private:
			FrameRenderData* pCurrentFrame;
			ShadingModel* pActiveShadingModel;
		};
	}
}