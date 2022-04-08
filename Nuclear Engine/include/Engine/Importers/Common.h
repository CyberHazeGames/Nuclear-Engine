#pragma once
#include <Engine/Assets/Mesh.h>
#include <Engine/Assets/Material.h>
#include <Engine/Assets/Image.h>
#include <Engine/Assets/Animations.h>
#include <Base\Utilities\Delegate.h>
#include <string>
#include <tuple>

namespace NuclearEngine
{
	namespace Managers { class AssetManager; }

	namespace Importers
	{
		enum MeshTextureType
		{
			NONE = 0x0,

			ALBEDO = 0x1,

			SPECULAR = 0x2,

			AMBIENT = 0x3,

			EMISSIVE = 0x4,

			HEIGHT = 0x5,

			NORMALS = 0x6,

			SHININESS = 0x7,

			OPACITY = 0x8,

			DISPLACEMENT = 0x9,

			LIGHTMAP = 0xA,

			REFLECTION = 0xB,

			UNKNOWN = 0xC,

		};


		struct MeshLoadingDesc
		{
			bool SaveMaterialNames = true;
			bool LoadAnimation = true;
		};
		struct ImageLoadingDesc
		{
			std::string mPath;

			USAGE mUsage;

			BIND_FLAGS mBindFlags;

			Uint32 mMipLevels;

			CPU_ACCESS_FLAGS mCPUAccessFlags;

			bool mIsSRGB;

			bool mGenerateMips;

			bool mFlipY_Axis;

			bool mLoadFromMemory;

			void* mMemData;

			Uint32 mMemSize;

			TEXTURE_FORMAT mFormat;

			ImageLoadingDesc() :
				mPath(""),
				mUsage(USAGE_IMMUTABLE),
				mBindFlags(BIND_SHADER_RESOURCE),
				mMipLevels(0),
				mCPUAccessFlags(CPU_ACCESS_NONE),
				mIsSRGB(false),
				mGenerateMips(true),
				mFlipY_Axis(true),
				mFormat(TEX_FORMAT_UNKNOWN),
				mLoadFromMemory(false),
				mMemData(nullptr),
				mMemSize(0)
			{}
		};

		struct MeshImporterDesc
		{
			const char* mPath;
			MeshLoadingDesc mMeshDesc = MeshLoadingDesc();
			Managers::AssetManager* mManager = nullptr;
		};

		typedef Utilities::Delegate<bool(const MeshImporterDesc& desc, Assets::Mesh*, Assets::Material*, Assets::Animations*)> MeshImporterDelegate;
		typedef Utilities::Delegate<bool(const std::string& Path, const ImageLoadingDesc& Desc, Assets::Image*)> ImageImporterDelegate;
	}
}
