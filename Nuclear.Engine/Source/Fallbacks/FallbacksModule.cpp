#include <Fallbacks/FallbacksModule.h>
#include <Assets/AssetManager.h>
#include <Assets/TextureDesc.h>

namespace Nuclear
{
	namespace Fallbacks
	{
		FallbacksModule::FallbacksModule()
		{
			DefaultBlackImage = nullptr;

			DefaultGreyImage = nullptr;

			DefaultWhiteImage = nullptr;
		}

		bool FallbacksModule::Initialize()
		{
			// Fallbacks must be available even when external texture files are missing.
			auto CreateSolidTexture = [](const char* name, Byte red, Byte green, Byte blue)
			{
				Byte pixel[] = { red, green, blue, 255 };
				Assets::TextureDesc desc;
				desc.mPath = name;
				desc.mType = Diligent::RESOURCE_DIM_TEX_2D;
				desc.mWidth = desc.mHeight = 1;
				desc.mBitsPerPixel = 32;
				desc.mNumComponents = 4;
				desc.mRowStride = 4;
				desc.mComponentType = Diligent::VT_UINT8;
				desc.mMipLevels = 1;
				desc.mData = pixel;
				// This overload creates the GPU texture synchronously.
				return Assets::Importer::Get().ImportTexture(desc);
			};

			DefaultBlackImage = CreateSolidTexture("DefaultBlack", 0, 0, 0);
			DefaultGreyImage = CreateSolidTexture("DefaultGrey", 128, 128, 128);
			DefaultWhiteImage = CreateSolidTexture("DefaultWhite", 255, 255, 255);
			DefaultNormalTex.pTexture = CreateSolidTexture("DefaultNormal", 128, 128, 255);
			DefaultNormalTex.mUsageType = Assets::TextureUsageType::Normal;

			DefaultDiffuseTex.pTexture = DefaultGreyImage;
			DefaultDiffuseTex.mUsageType = Assets::TextureUsageType::Diffuse;

			DefaultSpecularTex.pTexture = DefaultWhiteImage;
			DefaultSpecularTex.mUsageType = Assets::TextureUsageType::Specular;

			return true;
		}
		void FallbacksModule::Shutdown()
		{
		}
		Assets::Texture* FallbacksModule::GetDefaultBlackImage()
		{
			return DefaultBlackImage;
		}
		Assets::Texture* FallbacksModule::GetDefaultGreyImage()
		{
			return DefaultGreyImage;
		}
		Assets::Texture* FallbacksModule::GetDefaultWhiteImage()
		{
			return nullptr;
		}
		const Assets::MaterialTexture& FallbacksModule::GetDefaultDiffuseTex() const
		{
			return DefaultDiffuseTex;
		}
		const Assets::MaterialTexture& FallbacksModule::GetDefaultSpecularTex() const
		{
			return DefaultSpecularTex;
		}
		const Assets::MaterialTexture& FallbacksModule::GetDefaultNormalTex() const
		{
			return DefaultNormalTex;
		}
	}
}
