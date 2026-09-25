#pragma once
#include <Core/EngineModule.h>
#include <Assets/MaterialTypes.h>

namespace Nuclear
{
	namespace Fallbacks
	{
		class NEAPI FallbacksModule : public Core::EngineModule
		{
		public:
			static FallbacksModule& Get();
			bool OnInitialize() override;
			bool Initialize();
			void Shutdown() override;

			Assets::Texture* GetDefaultBlackImage();
			Assets::Texture* GetDefaultGreyImage();
			Assets::Texture* GetDefaultWhiteImage();

			const Assets::MaterialTexture& GetDefaultDiffuseTex() const;
			const Assets::MaterialTexture& GetDefaultSpecularTex() const;
			const Assets::MaterialTexture& GetDefaultNormalTex() const;
		private:
			FallbacksModule();

			Assets::Texture* DefaultBlackImage;
			Assets::Texture* DefaultGreyImage;
			Assets::Texture* DefaultWhiteImage;

			Assets::MaterialTexture DefaultDiffuseTex;
			Assets::MaterialTexture DefaultSpecularTex;
			Assets::MaterialTexture DefaultNormalTex;
		};
	}
}
