#pragma once
#include <NE_Common.h>
#include <Math\Math.h>
#include <Graphics\ShaderPipelineSwitch.h>
#include <Serialization/Access.h>
#include <Serialization/SerializationModule.h>
#include <Core\UUID.h>
#include <Assets/AssetType.h>
#include <Serialization/IsLoading.h>
#include <vector>
#include <memory>

namespace Nuclear {
	namespace Assets 
	{
		class Mesh;
		class Material;
	}

	namespace Animation {
		class Animator;
	}

	namespace Graphics {
		struct ShaderPipelineVariant;
	}
	namespace Components 
	{

		class NEAPI MeshComponent
		{
		public:			
			MeshComponent();
            Math::Vector3 Tint{1.f}; ///< Linear RGB multiplier applied by the forward game material.
            float Metallic = -1.f; ///< [0,1] override, or -1 to use the PBR map.
            float Roughness = -1.f; ///< [0,1] override, or -1 to use the PBR map.
			MeshComponent(Assets::Mesh* mesh, Assets::Material* material = nullptr, Animation::Animator* Animator = nullptr);
			~MeshComponent();

			void Update(bool forcedirty = false);

			void SetVariantSwitch(Uint32 VariantID, bool val);

			void SetEnableRendering(bool val);
			void SetCastShadow(bool val);
			void SetReceiveShadows(bool val);

			void SetMesh(Assets::Mesh* mesh);
			void SetMaterial(Assets::Material* material);

			void SetRenderSystemFlags(bool hasdefferedpass, bool hasshadowpass);

			FORCE_INLINE Uint32 GetRenderQueue() const
			{
				return RenderQueue;
			}
			FORCE_INLINE Graphics::ShaderPipelineVariant* GetRenderingVariant()
			{
				return mPipelineCntrl.GetActiveVariant();
			}

			FORCE_INLINE bool GetEnableRendering() const
			{
				return mEnableRendering;
			}
			FORCE_INLINE bool GetCastShadow() const
			{
				return mCastShadow;
			}
			FORCE_INLINE bool GetReceiveShadows() const
			{
				return mReceiveShadows;
			}

			FORCE_INLINE Assets::Mesh* GetMesh()
			{
				return pMesh;
			}
			FORCE_INLINE Assets::Material* GetMaterial()
			{
				return pMaterial;
			}
			/// Selects an imported clip and crossfades in seconds. Negative index disables authored animation.
            bool PlayClip(int index, float blendSeconds);
            /// Updates only the component-owned animator, preserving manual sample animators.
            void AdvanceClip(float seconds);
            int ClipIndex = -1; ///< Authored/current clip index; negative means static mesh.
            float BlendSeconds = .2f; ///< Duration used when the author selects a new clip.
            FORCE_INLINE Animation::Animator* GetAnimator()
			{
				return pAnimator;
			}

			constexpr static auto serialize(auto& archive, auto& self)
			{
				ISLOADING(archive)
				{
					Core::UUID mesh;
					Core::UUID material;

					auto result = archive(mesh, material, self.mEnableRendering, self.mCastShadow, self.mReceiveShadows);

					self.pMesh = static_cast<Assets::Mesh*>(Serialization::SerializationModule::Get().DeserializeUUID(Assets::AssetType::Mesh, mesh));
					self.pMaterial = static_cast<Assets::Material*>(Serialization::SerializationModule::Get().DeserializeUUID(Assets::AssetType::Material, material));

					return result;
				}
				else
				{
					Core::UUID mesh;
					Core::UUID material;

					if (self.pMesh != nullptr)					
						mesh = self.pMesh->GetUUID();

					if (self.pMaterial != nullptr)
						material = self.pMaterial->GetUUID();


					return archive(mesh, material, self.mEnableRendering, self.mCastShadow, self.mReceiveShadows);
				}
			}
		protected:
			friend Serialization::Access;

			Assets::Mesh* pMesh;
			Assets::Material* pMaterial;
			std::shared_ptr<Animation::Animator> mOwnedAnimator; ///< Per-instance playback for editor-authored clips.
			Animation::Animator* pAnimator;
			Graphics::ShaderPipelineSwitchController mPipelineCntrl;

			std::unordered_map<Uint32, bool> mCustomSwitches;

			Uint32 RenderQueue = 1;

			//Switches
			bool mEnableRendering = true;
			bool mCastShadow = true;
			bool mReceiveShadows = true;

			//Internal
			bool mRequestDeffered = false;
			bool mMaterialDirty = true;
			bool mDirty = true;
			bool mRenderSystemHasDefferedPass = false;
			bool mRenderSystemHasShadowPass = false;
		};

	}
}