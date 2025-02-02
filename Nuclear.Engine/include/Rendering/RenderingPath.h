#pragma once
#include <NE_Common.h>
#include <Graphics/BakeStatus.h> 
#include <Components\MeshComponent.h>

namespace Nuclear
{
	namespace Systems {
		class RenderSystem;
	}
	namespace Graphics {
		struct ShaderPipelineVariant;
	}
	namespace Rendering
	{
		struct FrameRenderData;
		class ShaderPipeline;

		//Defines how to render stuff
		class NEAPI RenderingPath
		{
		public:
			RenderingPath();

			virtual void UpdateAnimationCB(Animation::Animator* mAnimator);

			virtual void Reset();

			//Static meshes
			virtual void StartRendering(FrameRenderData* framedata, Graphics::ShaderPipelineVariant* pipeline) = 0;

			virtual void Render(Components::MeshComponent& mesh, const Math::Matrix4& modelmatrix) = 0;

		protected:
			void DrawMesh(Assets::Mesh* mesh, Assets::Material* material);
			Graphics::ShaderPipelineVariant* pActivePipeline = nullptr;
			FrameRenderData* pCurrentFrame = nullptr;
		};
	}
}