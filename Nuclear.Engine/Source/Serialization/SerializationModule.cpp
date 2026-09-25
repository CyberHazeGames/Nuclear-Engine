#include "Serialization/SerializationModule.h"
#include <ThirdParty/zpp_bits.h>
#include <Parsers/INIParser.h>
#include <ThirdParty/magic_enum.hpp>
#include <Utilities/Hash.h>
#include <Assets/AssetLibrary.h>
#include <Fallbacks/FallbacksModule.h>

namespace Nuclear
{
	namespace Serialization
	{
		/*
		
		//Important for loading assets
			AssetType mType;
			Utilities::UUID mUUID;
			std::string mName;
			Uint32 mHashedName = 0;

			Uint32 mHashedPath = 0;
			AssetImportingDesc mImportingDesc;			
		*/

		//bool ReadValue(Parsers::INIStructure& meta, std::string section, std::string key, std::string value)
		//{

		//}

		const char* SetBool(bool val)
		{
			if (val)
				return "true";

			return "false";
		}
		SerializationModule& SerializationModule::Get() { static SerializationModule instance; return instance; }
		void SerializationModule::Shutdown()
		{
		}
		bool SerializationModule::Serialize(const Assets::AssetMetadata& metadata, const Core::Path& path)
		{
			Parsers::INIFile file(path.GetRealPath());

			Parsers::INIStructure meta;
			meta["General"]["Type"] = "AssetMetadata";

			meta["AssetMetadata"]["Name"] = metadata.mName;
			meta["AssetMetadata"]["UUID"] = metadata.mUUID.str();
			meta["AssetMetadata"]["AssetType"] = magic_enum::enum_name(metadata.mType);

			switch (metadata.mType)
			{
			case Assets::AssetType::Texture:
			{
				const auto imagedesc = static_cast<Assets::TextureLoadingDesc*>(metadata.pLoadingDesc);
				meta["TextureLoadingDesc"]["Extension"] = magic_enum::enum_name(imagedesc->mExtension);
				meta["TextureLoadingDesc"]["AsyncLoading"] = SetBool(imagedesc->mAsyncLoading);
				meta["TextureLoadingDesc"]["FlipYAxis"] = SetBool(imagedesc->mFlipYAxis);
			}
			case Assets::AssetType::Mesh:
			{
				const auto meshdesc = static_cast<Assets::MeshLoadingDesc*>(metadata.pLoadingDesc);
				meta["MeshLoadingDesc"]["MaterialUUID"] = meshdesc->mMaterialUUID.str();
				meta["MeshLoadingDesc"]["AnimationsUUID"] = meshdesc->mAnimationsUUID.str();
				meta["MeshLoadingDesc"]["SaveMaterialNames"] = SetBool(meshdesc->mSaveMaterialNames);
				meta["MeshLoadingDesc"]["ExternalMaterial"] = SetBool(meshdesc->mExternalMaterial);
			}


			default:
				break;
			}

			return file.generate(meta);
		}


		bool SerializationModule::Deserialize(Assets::AssetMetadata& metadata, const Core::Path& path)
		{
			Parsers::INIFile file(path.GetRealPath());

			Parsers::INIStructure meta;

			bool result = file.read(meta);

			if (meta["General"]["Type"] == "AssetMetadata")
			{
				metadata.mName = meta["AssetMetadata"]["Name"];
				metadata.mUUID = Core::UUID(meta["AssetMetadata"]["UUID"]);
				metadata.mType = magic_enum::enum_cast<Assets::AssetType>(meta["AssetMetadata"]["AssetType"]).value_or(Assets::AssetType::Unknown);

				switch (metadata.mType)
				{
				case Assets::AssetType::Texture:
				{
					auto imagedesc = static_cast<Assets::TextureLoadingDesc*>(metadata.pLoadingDesc = new Assets::TextureLoadingDesc);
					imagedesc->mExtension = magic_enum::enum_cast<Assets::IMAGE_EXTENSION>(meta["TextureLoadingDesc"]["Extension"]).value_or(Assets::IMAGE_EXTENSION::IMAGE_EXTENSION_UNKNOWN);
					imagedesc->mAsyncLoading = meta["TextureLoadingDesc"].GetBoolean("AsyncLoading", true);
					imagedesc->mFlipYAxis = meta["TextureLoadingDesc"].GetBoolean("FlipYAxis", true);
				}
				case Assets::AssetType::Mesh:
				{
					auto meshdesc = static_cast<Assets::MeshLoadingDesc*>(metadata.pLoadingDesc = new Assets::MeshLoadingDesc);
					meshdesc->mMaterialUUID = Core::UUID(meta["MeshLoadingDesc"]["MaterialUUID"]);
					meshdesc->mAnimationsUUID = Core::UUID(meta["MeshLoadingDesc"]["AnimationsUUID"]);
					meshdesc->mSaveMaterialNames = meta["MeshLoadingDesc"].GetBoolean("SaveMaterialNames", true);
					meshdesc->mExternalMaterial = meta["MeshLoadingDesc"].GetBoolean("ExternalMaterial", true);

				}
				default:
					break;
				}
			}	

			return result;
		}

		Assets::IAsset* SerializationModule::DeserializeUUID(Assets::AssetType type, const Core::UUID& uuid)
		{
			if (type == Assets::AssetType::Shader)
			{
				return Assets::AssetLibrary::Get().mImportedShaders.GetOrAddAsset(uuid);
			}
			else if (type == Assets::AssetType::Material)
			{
				return Assets::AssetLibrary::Get().mImportedMaterials.GetOrAddAsset(uuid);
			}
			else if (type == Assets::AssetType::Mesh)
			{
				return Assets::AssetLibrary::Get().mImportedMeshes.GetOrAddAsset(uuid);
			}
			else if (type == Assets::AssetType::Texture)
			{
				auto result = Assets::AssetLibrary::Get().mImportedTextures.GetOrAddAsset(uuid);
				if(result->GetTextureView() == nullptr)
				{
					result->SetTextureView(Fallbacks::FallbacksModule::Get().GetDefaultGreyImage()->GetTextureView());
				}
				return result;
			}


			return nullptr;
		}
		SerializationModule::SerializationModule()
		{
		}
	}
}
