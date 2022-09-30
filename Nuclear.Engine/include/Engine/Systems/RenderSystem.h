#pragma once
#include <Engine\ECS\System.h>
#include <Engine\Rendering\ShadingModel.h>
#include <Engine\Rendering\RenderingPipeline.h>
#include <Engine\Components\MeshComponent.h>
#include <Engine/Assets/Mesh.h>
#include <Engine/Assets/Material.h>
#include <Engine\Rendering\Background.h>
#include <vector>
#include <unordered_map>
#include <Engine/Rendering/RenderPass/RenderPass.h>
#include <Core/Logger.h>

namespace Nuclear
{
	namespace Systems
	{
		class CameraSystem;
		class LightingSystem;

		struct RenderSystemBakeStatus
		{
			bool BakeLighting = true;
			bool BakePipelines = true;
		};

		//RenderPass WIP
		struct NEAPI DrawCommand
		{
			DrawCommand(Assets::Mesh* mesh, const Math::Matrix4& transform)
				: mMeshPtr(mesh), mTransform(transform) {}

			const Math::Matrix4& GetTransform() const { return mTransform; }
			Assets::Mesh* GetMeshComponent() const { return mMeshPtr; }
		private:
			Assets::Mesh* mMeshPtr;
			Math::Matrix4 mTransform;
		};


		class NEAPI RenderSystem : public ECS::System<RenderSystem>
		{
		public:
			RenderSystem();
			~RenderSystem();

			void AddRenderingPipeline(Rendering::RenderingPipeline* Pipeline);
			void SetActiveRenderingPipeline(Uint32 PipelineID);

			void CreateMaterialForAllPipelines(Assets::Material* material);

			void CreateMaterial(Assets::Material* material, Uint32 PipelineID);

			bool NeedsBaking();

			void Bake(Uint32 RTWidth, Uint32 RTHeight, bool AllPipelines = true);

			void ResizeRenderTargets(Uint32 RTWidth, Uint32 RTHeight);

			Rendering::RenderingPipeline* GetActivePipeline();

			//////////////////////////////////////////////////////////////////////////////////////////////
			////////////////////////////////////WIP - RENDER PASS/////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////////
			void AddRenderPass(Rendering::RenderPass* pass);

			template <typename T>
			T* GetRenderPass()
			{
				static_assert(std::is_base_of<Rendering::RenderPass, T>::value, "GetRenderPass<T> class must derive from IRenderPass!");

				for (Rendering::RenderPass* pass : mRenderPasses)
				{
					T* result = dynamic_cast<T*>(pass);
					if (result)
					{
						return result;
					}
				}
				NUCLEAR_FATAL("[RenderSystem] GetRenderPass called with undefined RenderPass!");
				return nullptr;
			}
			//////////////////////////////////////////////////////////////////////////////////////////////

			//Update Functions
			void Update(ECS::TimeDelta dt) override;

			IBuffer* GetAnimationCB();

			CameraSystem* GetCameraSystem();
			LightingSystem* GetLightingSystem();

			Rendering::Background& GetBackground();

		private:
			Rendering::Background mBackground;
			std::vector<Rendering::RenderPass*> mRenderPasses;

			std::shared_ptr<CameraSystem> mCameraSystemPtr;
			std::shared_ptr<LightingSystem> mLightingSystemPtr;

			RenderSystemBakeStatus mStatus;
			RefCntAutoPtr<IBuffer> mAnimationCB;

			Rendering::RenderingPipeline* mActiveRenderingPipeline;
			std::unordered_map<Uint32, Rendering::RenderingPipeline*> mRenderingPipelines;
		};

	}
}