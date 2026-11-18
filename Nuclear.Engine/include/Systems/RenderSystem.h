#pragma once
#include <ECS\System.h>
#include <vector>
#include <unordered_map>
#include <Rendering/RenderPass.h>
#include <Rendering/FrameRenderData.h>

namespace Diligent {
	struct IBuffer;
}
namespace Nuclear
{
	namespace Rendering
	{
		class ImageBasedLighting;
	}
	namespace Systems
	{
		struct RenderSystemBakeStatus
		{
			bool BakeLighting = true;
			bool BakePipelines = true;
		};

		struct RenderSystemBakingDesc
		{
			Uint32 RTWidth;
			Uint32 RTHeight;

			bool mRenderToScreen = true;

			//requests deffered pipeline for all meshes (if exists)
			bool mIsDefferedByDefault = false;
		};

		class NEAPI RenderSystem : public ECS::System<RenderSystem>
		{
		public:
			RenderSystem();
			~RenderSystem();

			bool NeedsBaking();

			void Bake(const RenderSystemBakingDesc& desc);

			void ResizeRTs(Uint32 RTWidth, Uint32 RTHeight) override;

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
				return nullptr;
			}

			//Update Functions
			void Update(ECS::TimeDelta dt) override;

			void SetIBLContext(Rendering::ImageBasedLighting* IBLContext);

			void RegisterShader(Assets::Shader* shader);

			/////////////////////////////////////////////////////////////////////////////////////////
			Diligent::IBuffer* GetLightCB();

			Rendering::FrameRenderData mRenderData;
			size_t GetDirLightsNum();
			size_t GetPointLightsNum();
			size_t GetSpotLightsNum();
		private:
			std::vector<Rendering::RenderPass*> mRenderPasses;
			std::vector<Assets::Shader*> mRegisteredShaders;

			Rendering::ImageBasedLighting* pIBLContext = nullptr;

			RenderSystemBakeStatus mStatus;
			RenderSystemBakingDesc mLastBake{}; ///< Configuration to reuse after the live light set changes.
			bool mLightsChanged = false; ///< Rebuild before the next dereference of cached light pointers.
			void InvalidateLights(); ///< Called on construction/removal in the active registry.

			size_t Baked_DirLights_Size = 0;
			size_t Baked_PointLights_Size = 0;
			size_t Baked_SpotLights_Size = 0;
			size_t NE_Light_CB_Size;
			size_t NUM_OF_LIGHT_VECS;

			bool HasbeenBakedBefore = false;
			bool mRenderToScreen = false;
			Diligent::RefCntAutoPtr<Diligent::IBuffer> mPSLightCB;

			void BakeLights();
			void BakeLightsBuffer();
			void UpdateLights();
			bool LightRequiresBaking();
			void UpdateLightsBuffer(const Math::Vector4& CameraPos);
		};

	}
}