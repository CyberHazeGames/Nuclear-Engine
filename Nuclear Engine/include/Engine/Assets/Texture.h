#pragma once
#include <Base\Math\Math.h>
#include <Engine/Graphics/API/Texture.h>
#include <Engine/Graphics/API/Sampler.h>
#include <Engine/Assets/Asset.h>
#include <vector>

namespace NuclearEngine {
	namespace Assets {
		enum TextureUsageType : Uint8
		{
			Diffuse = 1,
			Specular = 2,
			Normal = 3,

			Unknown = 255
		};

		class NEAPI Texture : public Asset
		{
		public:
			Texture();
			~Texture();

			Uint8 GetUsageType();
			void SetUsageType(Uint8 type);
			void SetUsageType(TextureUsageType type);

			inline void VSBind(Uint32 slot, Graphics::API::Sampler& sampler)
			{
				sampler.VSBind(slot);
				mTexture.VSBind(slot);
			}
			inline void PSBind(Uint32 slot, Graphics::API::Sampler& sampler)
			{
				sampler.PSBind(slot);
				mTexture.PSBind(slot);
			}
			inline void VSBind(Uint32 slot)
			{
				return mTexture.VSBind(slot);
			}
			inline void PSBind(Uint32 slot)
			{
				return mTexture.PSBind(slot);
			}

			Graphics::API::Texture mTexture;

		private:
			Uint8 mUsageType;
		};
	}
}