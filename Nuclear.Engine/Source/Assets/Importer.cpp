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

		template <int N>
		void fillfontdesc(const msdfgen::BitmapConstRef<Byte, N>& bitmap, FontCreationDesc& desc)
		{
			desc.AtlasWidth = bitmap.width;
			desc.AtlasHeight = bitmap.height;
			desc.Data = bitmap.pixels;
		}
		Font* Importer::ImportFont(const Core::Path& Path, const FontImportingDesc& desc)
		{
			auto hashedpath = Utilities::Hash(Path.GetInputPath());

			using namespace msdf_atlas;

			if (msdfgen::FontHandle* font = msdfgen::loadFont(FT_Handle, Path.GetRealPath().c_str()))
			{
				// Storage for glyph geometry and their coordinates in the atlas
				std::vector<GlyphGeometry> glyphs;
				// FontGeometry is a helper class that loads a set of glyphs from a single font.
				// It can also be used to get additional font metrics, kerning information, etc.
				FontGeometry fontGeometry(&glyphs);
				// Load a set of character glyphs:
				// The second argument can be ignored unless you mix different font sizes in one atlas.
				// In the last argument, you can specify a charset other than ASCII.
				// To load specific glyph indices, use loadGlyphs instead.
				fontGeometry.loadCharset(font, 1.0, Charset::ASCII);


				// Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
				//const double maxCornerAngle = 3.0;
				//for (GlyphGeometry& glyph : glyphs)
				//	glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);


				// TightAtlasPacker class computes the layout of the atlas.
				TightAtlasPacker packer;
				// Set atlas parameters:
				// setDimensions or setDimensionsConstraint to find the best value
				packer.setDimensionsConstraint(TightAtlasPacker::DimensionsConstraint::SQUARE);
				// setScale for a fixed size or setMinimumScale to use the largest that fits
				packer.setMinimumScale(24.0);
				// setPixelRange or setUnitRange
				packer.setPixelRange(2.0);
				packer.setMiterLimit(1.0);
				// Compute atlas layout - pack glyphs
				packer.pack(glyphs.data(), glyphs.size());
				// Get final atlas dimensions
				int width = 0, height = 0;
				packer.getDimensions(width, height);
				// The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
				ImmediateAtlasGenerator<
					float, // pixel type of buffer for individual glyphs depends on generator function
					1, // number of atlas color channels
					sdfGenerator, // function to generate bitmaps for individual glyphs
					BitmapAtlasStorage<Byte, 1> // class that stores the atlas bitmap
					// For example, a custom atlas storage class that stores it in VRAM can be used.
				> generator(width, height);
				// GeneratorAttributes can be modified to change the generator's default settings.
				GeneratorAttributes attributes;
				generator.setAttributes(attributes);
				generator.setThreadCount(4);
				// Generate atlas bitmap
				generator.generate(glyphs.data(), glyphs.size());
				// The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
				// The glyphs array (or fontGeometry) contains positioning data for typesetting text.


				//	success = myProject::submitAtlasBitmapAndLayout(generator.atlasStorage(), glyphs);
				auto& atlas = generator.atlasStorage();

				Font* result = &AssetLibrary::Get().mImportedFonts.AddAsset();

				FontCreationDesc desc;
				fillfontdesc<1>(atlas, desc);

				Uint32 x = 'f';


				for (auto& i : glyphs)
				{
					FontCharacter glyph;
					glyph.CodePoint = i.getCodepoint();
					glyph.Advance = i.getAdvance();
					//		i.getQuadAtlasBounds
					auto x = i.getBoxRect();
					double l, b, r, t;
					i.getQuadAtlasBounds(l, b, r, t);
					result->Characters[i.getCodepoint()] = glyph;

				}
				result->Create(desc);

				NUCLEAR_TRACE("[Importer] Imported Font : '{0}' : '{1}'", Path.GetInputPath(), Utilities::int_to_hex<Uint32>(hashedpath));

				// Cleanup
				msdfgen::destroyFont(font);
				return result;
			}
			else
			{
				NUCLEAR_ERROR("[Importer] Failed to import font : '{0}' : '{1}'", Path.GetInputPath(), Utilities::int_to_hex<Uint32>(hashedpath));
			}

			//TODO

			return nullptr;
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
