#include <Engine\Assets\Material.h>
#include <Base\Utilities\Hash.h>
#include <Engine/Assets/DefaultTextures.h>
#include <Engine\Graphics\Context.h>

namespace NuclearEngine
{
	namespace Assets
	{	
		Material::Material()
			: mPixelShaderTextures(std::vector<TextureSet>()), mPixelShaderTS(std::vector<Texture>())
		{
		}
		Material::~Material()
		{
		}
		void Material::Create(const MaterialCreationDesc& desc)
		{
			if (!desc.mPipeline)
			{
				Log.Error("[Material] Creation requires a valid Pipeline object!\n");
				return;
			}
			mPipeline = desc.mPipeline;
			mPipeline->CreateShaderResourceBinding(&mSRB, true);

			mPixelShaderTextures;
			mPixelShaderTS;

			//Fix Slots
			for (size_t i = 0; i < mPixelShaderTextures.size(); i++)
			{
				for (size_t j = 0; j < mPixelShaderTextures.at(i).size(); j++)
				{
					for (auto TSinfo : mPixelShaderTS)
					{
						if (mPixelShaderTextures.at(i).at(j).mTex.GetUsageType() == TSinfo.GetUsageType())
						{
						//	mPixelShaderTextures.at(i).at(j).mSlot = TSinfo.mSlot;
						}
					}
				}
			}
		}


		void Material::BindTexSet(Uint32 index)
		{
			//TODO: Check if all Slots have been occupied and then bind the free ones to fix some glitches
			if (!mPixelShaderTextures.empty())
			{
				for (auto tex : mPixelShaderTextures.at(index))
				{	
					mSRB->GetVariable(SHADER_TYPE_PIXEL, tex.mSlot)->Set(tex.mTex.mTexture);
				}
			}
		}

		void Material::Bind()
		{	
			Graphics::Context::GetContext()->SetPipelineState(mPipeline);
		}
	
		TextureUsageType ParseTexUsageFromName(std::string& name)
		{
			if (name.find("Diffuse") == 0)
				return TextureUsageType::Diffuse;
			else if (name.find("Specular") == 0)
				return TextureUsageType::Specular;
			else if (name.find("Normal") == 0)
				return TextureUsageType::Normal;

			return TextureUsageType::Unknown;
		}
	}
}