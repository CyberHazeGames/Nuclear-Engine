#include <Assets/Importer.h>
#include <Assets/AssetLibrary.h>
#include <Assets/Importers\TextureImporter.h>
#include <Utilities/Logger.h>
#include <Scripting\ScriptingModule.h>

#include <ft2build.h>
#include <freetype/freetype.h> 
#include <msdf-atlas-gen/msdf-atlas-gen/msdf-atlas-gen.h>

#include <Fallbacks/FallbacksModule.h>
#include <Audio\AudioModule.h>
#include <Audio\AudioBackend.h>

#include <Platform\FileSystem.h>
#include <Parsers/ShaderParser.h>

#include <Threading/Task.h>
#include <Threading/MainThreadTask.h>
#include <Threading/ThreadingModule.h>

#include <Assets/AssetManager.h>
#include <Assets\AssetLibrary.h>

#include <Assets/Tasks/TextureImportTask.h>
#include <Assets/Tasks/MaterialImportTask.h>
#include <Assets/Tasks/MeshImportTask.h>

#include <filesystem>
#include <memory>
#include <algorithm>
#include <Graphics/GraphicsModule.h>

#include <ThirdParty/sndfile.h>
#include <Assets/AudioClip.h>

#pragma comment(lib,"sndfile.lib")

#include "../Audio/AudioConverter.h"
namespace Nuclear
{
	namespace Assets
	{
		
		Importer::Importer()
		{
			FT_Handle = msdfgen::initializeFreetype();

			if (FT_Handle == nullptr)
			{
				NUCLEAR_ERROR("[Importer] Failed To Initialize FreeType library");
			}
		}
		Importer& Importer::Get()
		{
			static Importer instance;

			return instance;
		}
	
		std::string AssetNameFromDirectory(const std::string& str)
		{
			std::size_t found = str.find_last_of("/\\");
			std::string directory_path = str.substr(0, found);

			std::size_t found2 = directory_path.find_last_of("/\\");
			std::string directory_path_name = directory_path.substr(found2 + 1);

			std::string filename = str.substr(found + 1);

			filename = filename.substr(0, filename.find_last_of('.'));


			return directory_path_name + "_" + filename;
		}

		Texture* Importer::ImportTexture(const Core::Path& Path, const TextureImportingDesc& Desc)
		{
			if (Desc.mCommonOptions.mAsyncImport)
			{
				//Add to queue			
				auto result = &AssetLibrary::Get().mImportedTextures.AddAsset();
				if (Desc.mCommonOptions.mAssetName == "")
				{
					result->SetName(AssetNameFromDirectory(Path.GetRealPath()));
				}

				mQueuedAssets.push_back(result);
				result->SetState(IAsset::State::Queued);
				Threading::ThreadingModule::Get().GetThreadPool().AddTask(new TextureImportTask(result, Path , Desc));

				result->SetTextureView(Fallbacks::FallbacksModule::Get().GetDefaultBlackImage()->GetTextureView());
				return result;
			}
			else //singlethreaded loading
			{
				//TODO: Check if we on main thread?

				//Load
				TextureDesc desc;
				TextureData data;

				if (!Importers::TextureImporter::Get().Load(Path.GetRealPath(), &desc, Desc))
				{
					NUCLEAR_ERROR("[Importer] Failed To Decode Texture: '{0}'", Path.GetInputPath());
					return Fallbacks::FallbacksModule::Get().GetDefaultBlackImage();
				}

				auto result = &AssetLibrary::Get().mImportedTextures.AddAsset();
				if (Desc.mCommonOptions.mAssetName == "")
				{
					result->SetName(AssetNameFromDirectory(Path.GetRealPath()));
				}

				Graphics::GraphicsModule::Get().CreateImageData(&data, desc);

				if (!Graphics::GraphicsModule::Get().CreateImage(result, &data))
				{
					NUCLEAR_ERROR("[Importer] Failed To Load Texture: '{0}'", Path.GetInputPath());
					return Fallbacks::FallbacksModule::Get().GetDefaultBlackImage();
				}


				result->mState = IAsset::State::Loaded;
				NUCLEAR_TRACE("[Assets] Imported: {0} ", Path.GetInputPath());

				//TODO:EXPORT

				return result;
			}
		}

		Texture* Importer::ImportTexture(const TextureDesc& imagedesc, const TextureImportingDesc& importingdesc)
		{
			//Create
			auto result = &AssetLibrary::Get().mImportedTextures.AddAsset();

			TextureData imagedata;
			Graphics::GraphicsModule::Get().CreateImageData(&imagedata, imagedesc);
			
			if (!Graphics::GraphicsModule::Get().CreateImage(result, &imagedata))
			{
				NUCLEAR_ERROR("[Importer] Failed To Create Texture : '");
				return Fallbacks::FallbacksModule::Get().GetDefaultBlackImage();
			}


			result->mState = IAsset::State::Loaded;
			NUCLEAR_TRACE("[Assets] Imported: {0} ", imagedesc.mPath);
			return result;
		}

		AudioClip* Importer::ImportAudioClip(const Core::Path& Path, const AudioClipImportingDesc& Desc)
		{
			auto result = &AssetLibrary::Get().mImportedAudioClips.AddAsset();
			SNDFILE* sndfile;
			SF_INFO sfinfo;
			sf_count_t num_frames;

			sndfile = sf_open(Path.GetRealPath().c_str(), SFM_READ, &sfinfo);
			if (!sndfile)
			{
				NUCLEAR_ERROR("[Importer] Failed To Import AudioClip: '{0}' Error: {1}", Path.GetInputPath(), sf_strerror(sndfile));
				return result;
			}
			if (sfinfo.frames < 1 || sfinfo.frames >(sf_count_t)(INT_MAX / sizeof(short)) / sfinfo.channels)
			{
				NUCLEAR_ERROR("[Importer] Failed To Import AudioClip: '{0}' Error: Bad sample count!", Path.GetInputPath(), sf_strerror(sndfile));
				sf_close(sndfile);
				return result;
			}

			if (!(sfinfo.channels == 1 || sfinfo.channels == 2))
			{
				NUCLEAR_ERROR("[Importer] Failed To Import AudioClip: '{0}' Error: Unsupported channel count: {1}!", Path.GetInputPath(), sfinfo.channels);
				sf_close(sndfile);
				return result;

			}
			
			auto byterate = sf_current_byterate(sndfile);

			Audio::AudioFile file;

			file.mInfo.mSamples = sfinfo.frames;
			file.mInfo.mSampleRate = sfinfo.samplerate;
			file.mInfo.mChannels = sfinfo.channels;
			file.mInfo.mBitsPerSample = byterate;
			file.mInfo.mFormat = sfinfo.format;
			file.mInfo.mSections = sfinfo.sections;
			file.mInfo.mSeekable = sfinfo.seekable;
			file.mInfo.mBitDepth = Desc.mBitDepth;

			//else if (sfinfo.channels == 3)
			//{
			//	if (sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
			//		format = AL_FORMAT_BFORMAT2D_16;
			//}
			//else if (sfinfo.channels == 4)
			//{
			//	if (sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
			//		format = AL_FORMAT_BFORMAT3D_16;
			//}

			/* Decode the whole audio file to a buffer. */

			auto size = (size_t)(sfinfo.frames * sfinfo.channels);

			switch (Desc.mBitDepth)
			{
			case 8:
			{
				size *= sizeof(Byte);
				file.mData.resize(size);

				num_frames = sf_read_raw(sndfile, file.mData.data(), sfinfo.frames);
				break;
			}
			case 16:
			{
				size *= sizeof(short);
				file.mData.resize(size);
				num_frames = sf_readf_short(sndfile, (short*)file.mData.data(), sfinfo.frames);
				break;
			}
			case 24:
			{
				size *= sizeof(int);
				file.mData.resize(size);
				num_frames = sf_readf_int(sndfile, (int*)file.mData.data(), sfinfo.frames);
				break;
			}
			case 32:
			{
				size *= sizeof(float);
				file.mData.resize(size);
				num_frames = sf_readf_float(sndfile, (float*)file.mData.data(), sfinfo.frames);
				break;
			}
			default:
			{
				size *= sizeof(short);
				file.mData.resize(size);
				num_frames = sf_readf_short(sndfile, (short*)file.mData.data(), sfinfo.frames);
				file.mInfo.mBitDepth = 16;
				break;
			}
			}

			if (num_frames < 1)
			{
				sf_close(sndfile);
				NUCLEAR_ERROR("[Importer] Failed To Import AudioClip: '{0}' Error: Failed to read samples!", Path.GetInputPath());
				return 0;
			}

			//convert to mono for 3d sounds
			if (Desc.mIs3D && sfinfo.channels > 1)
			{
				Uint32 samples_per_channel = sfinfo.frames / sfinfo.channels;

				std::vector<Byte> result;
				result.resize(samples_per_channel * 2);

				Audio::ConvertToMono(file.mData.data(), result.data(), 16, samples_per_channel, sfinfo.channels);
				file.mData.swap(result);
				
				file.mInfo.mSamples = samples_per_channel;
				file.mInfo.mChannels = 1;
			}


			sf_close(sndfile);

			result->m3D = Desc.mIs3D;
			result->mLoop = Desc.mLoop;


			Audio::AudioModule::Get().GetBackend()->CreateAudioClip(result, file);


			result->mState = IAsset::State::Loaded;
			NUCLEAR_TRACE("[Assets] Imported: {0} ", Path.GetInputPath());
			return result;
		}


		Mesh* Importer::ImportMesh(const Core::Path& Path, const MeshImportingDesc& desc)
		{
			Mesh* mesh = nullptr;
			Material* material = nullptr;
			Animations* animations = nullptr;

			std::string assetname = desc.mCommonOptions.mAssetName;

			if (desc.mCommonOptions.mAssetName == "")
			{
				assetname = AssetNameFromDirectory(Path.GetRealPath());
			}

			if (desc.mImportMaterial)
			{
				material = &AssetLibrary::Get().mImportedMaterials.AddAsset();
				material->SetName(assetname);
				material->SetState(IAsset::State::Queued);
				mQueuedAssets.push_back(material);
			}
			if (desc.mImportAnimations)
			{
				animations = &AssetLibrary::Get().mImportedAnimations.AddAsset();
				animations->SetName(assetname);
				animations->SetState(IAsset::State::Queued);
				mQueuedAssets.push_back(animations);
			}

			if (desc.mImportMesh)
			{
				mesh = &AssetLibrary::Get().mImportedMeshes.AddAsset();
				mesh->SetName(assetname);
				mesh->pImportedMaterial = material;
				mesh->pImportedAnimations = animations;
				mesh->SetState(IAsset::State::Queued);
				mQueuedAssets.push_back(mesh);
			}
		
			if (desc.mCommonOptions.mAsyncImport)
			{
				Threading::ThreadingModule::Get().GetThreadPool().AddTask(new MeshImportTask({ assetname ,mesh, material, animations }, Path, desc));
			}
			else
			{			
				MeshImportTask* task = new MeshImportTask({ assetname ,mesh, material, animations }, Path, desc);
				task->Execute();
			}

			return mesh;
		}

		Material* Importer::ImportMaterial(const Core::Path& Path, const MaterialImportingDesc& Desc)
		{
			if (Desc.mCommonOptions.mAsyncImport)
			{
				//Add to queue			
				auto result = &AssetLibrary::Get().mImportedMaterials.AddAsset();
				if (Desc.mCommonOptions.mAssetName == "")
				{
					result->SetName(AssetNameFromDirectory(Path.GetRealPath()));
				}

				mQueuedAssets.push_back(result);
				result->SetState(IAsset::State::Queued);
				Threading::ThreadingModule::Get().GetThreadPool().AddTask(new MaterialImportTask(result, Path, Desc));

				//TODO: Fallback material?
				//result->SetTextureView(Fallbacks::FallbacksModule::Get().GetDefaultBlackImage()->GetTextureView());
				return result;
			}

			//TODO Singlethread material loading
			assert(0);
			return nullptr;

		}

		Font* Importer::ImportFont(const Core::Path& path, const FontImportingDesc& desc)
		{
			if (!FT_Handle || !Graphics::GraphicsModule::Get().GetDevice())
			{
				return nullptr;
			}

			using namespace msdf_atlas;
			std::unique_ptr<msdfgen::FontHandle, decltype(&msdfgen::destroyFont)> font(
				msdfgen::loadFont(FT_Handle, path.GetRealPath().c_str()), msdfgen::destroyFont);
			if (!font)
			{
				NUCLEAR_ERROR("[Importer] Failed to import font: '{0}'", path.GetInputPath());
				return nullptr;
			}

			std::vector<GlyphGeometry> glyphs;
			FontGeometry geometry(&glyphs);
			if (geometry.loadCharset(font.get(), 1.0, Charset::ASCII) <= 0)
			{
				return nullptr;
			}

			TightAtlasPacker packer;
			packer.setDimensionsConstraint(TightAtlasPacker::DimensionsConstraint::SQUARE);
			packer.setMinimumScale(std::clamp<double>(desc.Height, 8.0, 256.0));
			packer.setPixelRange(2.0);
			packer.setMiterLimit(1.0);
			if (packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) != 0)
			{
				return nullptr;
			}

			int width = 0;
			int height = 0;
			packer.getDimensions(width, height);
			if (width <= 0 || height <= 0)
			{
				return nullptr;
			}

			// Retain the existing single-channel SDF generator. Its bitmap remains
			// alive until the synchronous graphics-thread atlas upload completes.
			ImmediateAtlasGenerator<float, 1, sdfGenerator, BitmapAtlasStorage<Byte, 1>> generator(width, height);
			generator.setAttributes(GeneratorAttributes{});
			generator.setThreadCount(4);
			generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));
			const msdfgen::BitmapConstRef<Byte, 1> atlas = generator.atlasStorage();

			auto* result = &AssetLibrary::Get().mImportedFonts.AddAsset();
			result->Ascender = static_cast<float>(geometry.getMetrics().ascenderY);
			result->LineHeight = static_cast<float>(geometry.getMetrics().lineHeight);
			for (const auto& source : glyphs)
			{
				FontCharacter glyph;
				glyph.CodePoint = source.getCodepoint();
				glyph.Advance = static_cast<float>(source.getAdvance());
				const auto box = source.getBoxRect();
				glyph.Size = { box.w, box.h };

				double left, bottom, right, top;
				source.getQuadPlaneBounds(left, bottom, right, top);
				glyph.PlaneBounds = { left, bottom, right, top };
				glyph.Bearing = { static_cast<int>(left * packer.getScale()), static_cast<int>(top * packer.getScale()) };
				source.getQuadAtlasBounds(left, bottom, right, top);
				glyph.AtlasBounds = { left / width, top / height, right / width, bottom / height };
				result->Characters[glyph.CodePoint] = glyph;
			}

			for (const auto& previous : glyphs)
			{
				for (const auto& current : glyphs)
				{
					double advance = previous.getAdvance();
					if (!geometry.getAdvance(advance, previous.getCodepoint(), current.getCodepoint()))
					{
						continue;
					}
					const float adjustment = static_cast<float>(advance - previous.getAdvance());
					if (adjustment != 0)
					{
						result->Kerning[(Uint64(previous.getCodepoint()) << 32) | current.getCodepoint()] = adjustment;
					}
				}
			}

			result->Create({ static_cast<Uint32>(width), static_cast<Uint32>(height), atlas.pixels });
			result->SetName(path.GetInputPath());
			if (result->GetState() != IAsset::State::Created)
			{
				return nullptr;
			}
			NUCLEAR_TRACE("[Importer] Imported font: '{0}'", path.GetInputPath());
			return result;
		}

		Shader* Importer::ImportShader(const Core::Path& Path, const ShaderImportingDesc& desc)
		{
			Shader* result = &AssetLibrary::Get().mImportedShaders.AddAsset();
			result->SetName(Path.GetFilename(true));

			auto source = Platform::FileSystem::Get().LoadFileToString(Path.GetRealPath());

			//Step1: Parse Source -> Build ShaderBuildDesc
			Graphics::ShaderBuildDesc& shaderbuilddesc = result->mBuildDesc;
			shaderbuilddesc.mDefines = desc.mDefines;
			shaderbuilddesc.mExcludedVariants = desc.mExcludedVariants;
			bool parsing_ = Parsers::ShaderParser::ParseSource(source, shaderbuilddesc);

			if (parsing_)
			{
				//Step2: Reflect Pixel shader for material reflection
				if (Graphics::GraphicsModule::Get().ReflectShader(shaderbuilddesc, result->mReflection))
				{
					//Step3: Create actual pipeline
					result->mPipeline.BuildVariants();

					//step 4: export shader info
					if (!desc.mCommonOptions.mLoadOnly)
						AssetManager::Get().Export(result, true, AssetLibrary::Get().GetPath() + "Shaders/");

				}
				else
				{
					NUCLEAR_WARN("[Importer] Reflecting Shader: {0} Failed!", result->GetName());
					result->mPipeline.BuildVariants();
				}
			}
			else
			{
				NUCLEAR_ERROR("[Importer] Importing Shader: {0} Failed!", result->GetName());
				result->mState = IAsset::State::Unknown;
				return result;
			}

			result->mState = IAsset::State::Loaded;
			NUCLEAR_TRACE("[Assets] Imported: {0} ", Path.GetInputPath());
			return result;
		}

		Script* Importer::ImportScript(const Core::Path& Path, const ScriptImportingDesc& desc)
		{
			Script* result = &AssetLibrary::Get().mImportedScripts.AddAsset();

			std::string fullname = "";
			if (desc.mClassNameFromPath)
			{
				fullname = Scripting::ScriptingModule::Get().GetClientNamespace() + '.' + Path.GetFilename(true);
			}
			else
			{
				fullname = desc.mScriptFullName;
			}
			if (!Scripting::ScriptingModule::Get().CreateScriptAsset(result, fullname))
			{
				NUCLEAR_ERROR("[Assets] Failed to import script: {0}", fullname);
				return nullptr;
			}

			result->mState = IAsset::State::Loaded;
			NUCLEAR_TRACE("[Assets] Imported: {0} ", Path.GetInputPath());
			return result;
		}

		std::array<Texture*, 6> Importer::ImportTextureCube(const std::array<Core::Path, 6>& Paths, const TextureImportingDesc& desc)
		{
			TextureImportingDesc Desc = desc;
			//Desc.FlipY_Axis = false;

			std::array<Texture*, 6> result = {
				TextureCube_Import(Paths.at(0), Desc),
				TextureCube_Import(Paths.at(1), Desc),
				TextureCube_Import(Paths.at(2), Desc),
				TextureCube_Import(Paths.at(3), Desc),
				TextureCube_Import(Paths.at(4), Desc),
				TextureCube_Import(Paths.at(5), Desc) };

			return result;
		}

		AssetType Importer::GetAssetType(const std::string& filename)
		{
			std::string extension = filename.substr(filename.find_last_of("."));


			if (Importers::TextureImporter::Get().IsExtensionSupported(extension))
			{
				return AssetType::Texture;
			}
			else if (Importers::AssimpManager::Get().IsExtensionSupported(extension))
			{
				return AssetType::Mesh;
			}
			else if (extension == ".NEShader")
			{
				return AssetType::Shader;
			}
			else if (extension == ".NEMaterial")
			{
				return AssetType::Material;
			}
			else if (extension == ".NEScene")
			{
				return AssetType::Scene;
			}
			else
			{
				NUCLEAR_ERROR("[Importer] GetAssetType(filename) Unknown Type: {0}", extension);
				return AssetType::Unknown;
			}


			return AssetType::Unknown;
		}

		std::vector<IAsset*>& Importer::GetQueuedAssets()
		{
			return mQueuedAssets;
		}

		Scene* Importer::ImportScene(const Core::Path& Path, const SceneImportingDesc& desc)
		{
			Scene* result = &AssetLibrary::Get().mImportedScenes.AddAsset();

			Platform::FileSystem::Get().LoadBinaryBuffer(result->mBinaryBuffer, Path);

			NUCLEAR_TRACE("[Importer] Imported Scene : '{0}'", Path.GetInputPath());
			return result;
		}

		Texture* Importer::TextureCube_Import(const Core::Path& Path, const TextureImportingDesc& importingdesc)
		{
			auto result = &AssetLibrary::Get().mImportedTextures.AddAsset();

			TextureDesc imagedesc;
			TextureData imagedata;

			if (!Importers::TextureImporter::Get().Load(Path.GetRealPath(), &imagedesc, importingdesc))
			{
				NUCLEAR_ERROR("[Importer] Failed To Load Texture2D (For CubeMap): '{0}'", Path.GetInputPath());
				return nullptr;
			}
			imagedesc.mMipLevels = 1;
			Graphics::GraphicsModule::Get().CreateImageData(&imagedata, imagedesc);
			Graphics::GraphicsModule::Get().CreateImage(result, &imagedata);

			//TODO::
			//result.mData = imagedata;


			NUCLEAR_TRACE("[Importer] Imported Texture2D (for CubeMap) : '{0}'", Path.GetInputPath());

			return result;
		}
	}
}
