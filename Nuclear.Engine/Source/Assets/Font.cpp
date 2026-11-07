#include <Assets/Font.h>

#include <Graphics/GraphicsModule.h>

namespace Nuclear
{
	namespace Assets
	{
		Font::Font()
			: IAsset(AssetType::Font)
		{
		}

		Font::~Font()
		{
		}
		void Font::Create(const FontCreationDesc& desc)
		{
			mTextureView.Release();
			SetState(State::Unknown);
			if (!desc.Data || !desc.AtlasWidth || !desc.AtlasHeight || !Graphics::GraphicsModule::Get().GetDevice())
			{
				return;
			}
			Diligent::TextureDesc TexDesc;
			TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
			TexDesc.Width = desc.AtlasWidth;
			TexDesc.Height = desc.AtlasHeight;
			TexDesc.MipLevels = 1;
			TexDesc.ArraySize = 1;
			TexDesc.Usage = Diligent::USAGE_DEFAULT;
			TexDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
			TexDesc.Format = Diligent::TEX_FORMAT_R8_UNORM;
			TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

			Diligent::TextureSubResData pSubResource;
			pSubResource.pData = desc.Data;
			pSubResource.Stride = desc.AtlasWidth;

			Diligent::TextureData TexData;
			TexData.pSubResources = &pSubResource;
			TexData.NumSubresources = 1;
			Diligent::RefCntAutoPtr<Diligent::ITexture> mTexture;
			Graphics::GraphicsModule::Get().GetDevice()->CreateTexture(TexDesc, &TexData, &mTexture);

			if (mTexture)
			{
				mTextureView = mTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
				SetState(State::Created);
			}
		
		}
		const FontCharacter* Font::FindCharacter(Uint32 codepoint) const
		{
			auto found = Characters.find(codepoint);
			if (found == Characters.end()) found = Characters.find('?');
			return found == Characters.end() ? nullptr : &found->second;
		}

		float Font::GetKerning(Uint32 previous, Uint32 current) const
		{
			auto found = Kerning.find((Uint64(previous) << 32) | current);
			return found == Kerning.end() ? 0 : found->second;
		}
	}
}
