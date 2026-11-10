#pragma once
#include <Assets/LoadingDescs.h>
#include <Diligent/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <string>
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <vector>
namespace Nuclear
{
	namespace Assets
	{
		class Shader;
		struct CommonImportingOptions
		{
			///if this string left empty the engine will assign a default name as "Directoryname::AssetFilenamewithextension"
			///Example:
			///c:/assets/textures/rustediron/albedotex.png -> rustediron_albedotex.png
			std::string mAssetName = "";

			///if this path is invalid the engine will import to default asset folder in assetlibrary path
			///Path to directory only
			Core::Path mExportPath;


			bool mChangeExportedName = true;

			//No exporting
			bool mLoadOnly = false;
						
			bool mAsyncImport = true;
		};
		struct AudioClipImportingDesc
		{
			CommonImportingOptions mCommonOptions;

			bool mLoop = false;
			bool mIs3D = false;
			Uint32 mBitDepth = 16; 
		};
		struct MeshImportingDesc
		{
			CommonImportingOptions mCommonOptions;

			Shader* pMaterialShader = nullptr;

			bool mSaveMaterialNames = true;
			bool mImportMesh = true;
			bool mImportAnimations = true;
			bool mImportMaterial = true;
			/// When material import finds a referenced *_A image, also import existing sibling _N, _M, _R and _AO maps.
			/// Explicit material slots take precedence; discovery stays in the referenced image's directory.
			bool mDiscoverCompanionTextures = true;

			bool mExportMaterial = true;
		};

		struct MaterialImportingDesc
		{
			CommonImportingOptions mCommonOptions;
		};

		struct TextureImportingDesc
		{
			CommonImportingOptions mCommonOptions;

			Diligent::USAGE mUsage;

			Diligent::BIND_FLAGS mBindFlags;

			//0-> Create full mip map chain
			Uint32 mMipLevels;

			Diligent::CPU_ACCESS_FLAGS mCPUAccessFlags;

			bool mIsSRGB;

			bool mGenerateMips;

			TextureLoadingData mData;

			bool mLoadFromMemory;

			bool mGenerateMipsOnCPU;

			bool mFlipY_Axis;

			Diligent::RESOURCE_DIMENSION mType;

			IMAGE_EXTENSION mExportExtension;

			TextureImportingDesc() :
				mUsage(Diligent::USAGE_IMMUTABLE),
				mBindFlags(Diligent::BIND_SHADER_RESOURCE),
				mMipLevels(0),
				mCPUAccessFlags(Diligent::CPU_ACCESS_NONE),
				mIsSRGB(false),
				mGenerateMips(true),
				mLoadFromMemory(false),
				mData(TextureLoadingData()),
				mGenerateMipsOnCPU(false),
				mType(Diligent::RESOURCE_DIM_TEX_2D),
				mFlipY_Axis(true),
				mExportExtension(IMAGE_EXTENSION_UNKNOWN)
			{
			}
		};

		/// Settings for synchronous ASCII single-channel SDF font import on the graphics thread.
		struct FontImportingDesc
		{
			Uint32 Width = 0; ///< Reserved for compatibility; the atlas packer selects its width.
			Uint32 Height = 48; ///< Minimum atlas pixels per em, clamped to [8, 256].

		};

		struct ShaderImportingDesc
		{
			CommonImportingOptions mCommonOptions;

			std::vector<std::string> mDefines;
			std::vector<std::string> mExcludedVariants;
		};

		struct ScriptImportingDesc
		{

			std::string mScriptFullName;
			bool mClassNameFromPath = true;
		};
		struct SceneImportingDesc
		{
			std::string mName;
		};
	}
}
