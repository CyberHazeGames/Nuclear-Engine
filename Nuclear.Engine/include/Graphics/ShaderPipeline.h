#pragma once
#include <NE_Common.h>
#include <Diligent/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Diligent/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <unordered_map>
#include <Core\Path.h>
#include <Graphics/ShaderPipelineVariant.h>
#include <Graphics/ShaderPipelineSwitch.h>
#include "Rendering/GBuffer.h"
#include <Serialization/Access.h>

namespace Nuclear
{
	namespace Assets
	{
		class Shader;
	}
	namespace Rendering
	{
		class ImageBasedLighting;
	}
	namespace Graphics
	{			
		class NEAPI ShaderPipeline
		{
		public:
			ShaderPipeline(Assets::Shader* parent);
			ShaderPipeline(ShaderPipelineDesc& desc);

			~ShaderPipeline();

			void BuildVariants();

			/// Recreates GPU objects and reflection for the supplied scene bindings on the render thread.
			/// Variant addresses remain stable; callers must refresh mesh/material bindings after rebaking.
			bool Bake(ShaderPipelineBakingDesc& bakingdesc);

			Uint32 GetHashedKey(const std::string& Key);
			ShaderPipelineVariant* GetVariant(Uint32 Key);

			FORCE_INLINE const std::vector<ShaderPipelineSwitch>& GetSwitches() const
			{
				return mDesc.Switches;
			}
			FORCE_INLINE const std::unordered_map<Uint32, ShaderPipelineVariant>& GetVariants() const
			{
				return mVariants;
			}

			FORCE_INLINE Assets::Shader* GetShaderAsset()
			{
				return pParentShader;
			}
			FORCE_INLINE Rendering::GBuffer* GetGBuffer()
			{
				return &mGBuffer;
			}

			constexpr static auto serialize(auto& archive, auto& self)
			{
				ISLOADING(archive)
				{
					Core::UUID shaderuuid;
					auto result = archive(shaderuuid, self.mVariantsInfo);
					self.pParentShader = static_cast<Assets::Shader*>(Serialization::SerializationModule::Get().DeserializeUUID(Assets::AssetType::Shader, shaderuuid));
					return result;
				}
				else
				{
					Core::UUID uuid;
					if (self.pParentShader)
					{
						uuid = self.pParentShader->GetUUID();
					}
					return archive(uuid, self.mVariantsInfo);
				}

			}
		protected:
			friend Serialization::Access;
			friend class ShaderPipelineVariantFactory;

			Assets::Shader* pParentShader;
			std::unordered_map<Uint32, ShaderPipelineVariant> mVariants;
			std::vector<ShaderPipelineVariantDesc> mVariantsInfo;

			ShaderPipelineDesc& mDesc;
			Rendering::GBuffer mGBuffer;
		};
	}
}
