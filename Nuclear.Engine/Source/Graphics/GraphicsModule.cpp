#include "Graphics\GraphicsModule.h"
#include <Platform\Window.h>
#include <Diligent/Primitives/interface/Errors.hpp>
#include <Diligent/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Diligent/Graphics/GraphicsEngine/interface/EngineFactory.h>
#include "Diligent/Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Diligent/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Diligent/Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Diligent/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include <SDL\include\SDL.h>
#include <SDL\include\SDL_syswm.h>

#include <Assets\AssetManager.h>
#include "..\Graphics\ImGUI\imgui_impl_sdl.h"
#include "..\Graphics\ImGUI\imgui_impl.h"
#include <Utilities/Logger.h>
#include <Platform\FileSystem.h>
#include <Assets/DefaultMeshes.h>
#include <Diligent\Graphics\GraphicsAccessories\interface\GraphicsAccessories.hpp>
#include <Diligent\Graphics\GraphicsTools\interface\GraphicsUtilities.h>
#include <Assets\Texture.h>
#include <Fallbacks/FallbacksModule.h>

namespace Nuclear
{
	namespace Graphics
	{
		using namespace Diligent;
		void DiligentMassageCallback(DEBUG_MESSAGE_SEVERITY Severity, const Char* Message, const char* Function, const char* File, int Line)
		{
			switch (Severity)
			{
			case DEBUG_MESSAGE_SEVERITY::DEBUG_MESSAGE_SEVERITY_INFO:
				NUCLEAR_TRACE("[Diligent] {0}", Message);
				return;
			case DEBUG_MESSAGE_SEVERITY::DEBUG_MESSAGE_SEVERITY_WARNING:
				NUCLEAR_WARN("[Diligent] {0}", Message);

				return;
			case DEBUG_MESSAGE_SEVERITY::DEBUG_MESSAGE_SEVERITY_ERROR:
				NUCLEAR_ERROR("[Diligent] {0}", Message);

				return;
			case DEBUG_MESSAGE_SEVERITY::DEBUG_MESSAGE_SEVERITY_FATAL_ERROR:
				NUCLEAR_FATAL("[Diligent] {0}", Message);
				return;
			default:
				return;
			};

		}
		GraphicsModule& GraphicsModule::Get() { static GraphicsModule instance; return instance; }
		bool GraphicsModule::OnInitialize() { return Initialize(mStartupDesc); }
		bool GraphicsModule::Initialize(const GraphicsModuleDesc& GraphicsDesc)
		{
			//Initialize Context

			SwapChainDesc SCDesc(GraphicsDesc.SCDesc);

			if (GraphicsDesc.GammaCorrect)
			{
				SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
			}
			else
			{
				SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
			}
			bool Result = InitializeDiligentEngine(GraphicsDesc.pWindowHandle, GraphicsDesc.mRenderAPI, SCDesc);

			if (!Result)
			{
				NUCLEAR_ERROR("[GraphicsModule] Failed to initialize DiligentCore!");
				return false;
			}

			NUCLEAR_INFO("[GraphicsModule] Diligent Graphics API Initialized On: {0}", mAdapterAttribs.Description);

			GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(Core::Path("@NuclearAssets@/Shaders/").GetRealPath().c_str(), &pShaderSourceFactory);
			Assets::DefaultMeshes::Initialize();

			if (GraphicsDesc.InitImGui)
			{
				ImGui::CreateContext();
				ImGuiIO& io = ImGui::GetIO();
				io.Fonts->AddFontFromFileTTF(Core::Path("@NuclearAssets@/Fonts/Roboto-Medium.ttf").GetRealPath().c_str(), 15);
				io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
				io.ConfigDockingWithShift = true;
				ImGui_ImplSDL2_InitForDiligent(GraphicsDesc.pWindowHandle);
				ImGui_Impl_Init();
				ImGui_Impl_CreateDeviceObjects();
				//io.Fonts->GetTexDataAsAlpha8()

				NUCLEAR_TRACE("[GraphicsModule] ImGUI Initalized.");
			}

			mDefaultStaticSamplers["NEMat_"] = SamLinearWrapDesc;
			mDefaultStaticSamplers["NE_ShadowMap"] = ShadowMapSamplerDesc;
			mDefaultStaticSamplers["NE_RT_"] = SamLinearClampDesc;
			mDefaultStaticSamplers["NEIBL_"] = SamLinearClampDesc;

			NUCLEAR_INFO("[GraphicsModule] GraphicsModule has been initialized successfully!");
			return true;
		}
		void GraphicsModule::Shutdown()
		{
			NUCLEAR_INFO("[GraphicsModule] Shutting down...");
			pDevice->Release();
			pContext->Release();
			pSwapChain->Release();
		}

		void GraphicsModule::PresentFrame()
		{
			pSwapChain->Present();
		}

		bool GraphicsModule::CreateMesh(Assets::Mesh* mesh)
		{
			if (mesh)
			{
				bool result = true;

				for (unsigned int i = 0; i < mesh->mSubMeshes.size(); i++)
				{
					if (!mesh->mSubMeshes.at(i).Create())
					{
						result = false;
					}
				}

				mesh->SetState(Assets::IAsset::State::Created);
				return result;
			}
			return false;
		}

		bool GraphicsModule::CreateImage(Assets::Texture* result, Assets::TextureData* data)
		{
			//CREATE IMAGE
			Diligent::TextureData TexData;
			TexData.pSubResources = data->mSubresources.data();
			TexData.NumSubresources = data->mSubresources.size();
			Diligent::RefCntAutoPtr<Diligent::ITexture> mTexture;
			data->mTexDesc.MiscFlags = MISC_TEXTURE_FLAG_GENERATE_MIPS;
			GetDevice()->CreateTexture(data->mTexDesc, &TexData, &mTexture);
			
			if (mTexture.RawPtr() != nullptr)
			{
				result->mTextureView = nullptr;
				result->mWidth = data->mTexDesc.Width;
				result->mHeight = data->mTexDesc.Height;

				result->mTextureView = mTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
				GetContext()->GenerateMips(result->mTextureView);

				result->SetState(Assets::IAsset::State::Created);
				return true;
			}

			return false;
		}

		template <typename ChannelType>
		void ModifyComponentCount(const void* pSrcData,
			Uint32      SrcStride,
			Uint32      SrcCompCount,
			void* pDstData,
			Uint32      DstStride,
			Uint32      Width,
			Uint32      Height,
			Uint32      DstCompCount)
		{
			auto CompToCopy = std::min(SrcCompCount, DstCompCount);
			for (size_t row = 0; row < size_t{ Height }; ++row)
			{
				for (size_t col = 0; col < size_t{ Width }; ++col)
				{
					// clang-format off
					auto* pDst = reinterpret_cast<ChannelType*>((reinterpret_cast<Uint8*>(pDstData) + size_t{ DstStride } *row)) + col * DstCompCount;
					const auto* pSrc = reinterpret_cast<const ChannelType*>((reinterpret_cast<const Uint8*>(pSrcData) + size_t{ SrcStride } *row)) + col * SrcCompCount;
					// clang-format on

					for (size_t c = 0; c < CompToCopy; ++c)
						pDst[c] = pSrc[c];

					for (size_t c = CompToCopy; c < DstCompCount; ++c)
					{
						pDst[c] = c < 3 ?
							(SrcCompCount == 1 ? pSrc[0] : 0) :      // For single-channel source textures, propagate r to other channels
							std::numeric_limits<ChannelType>::max(); // Use 1.0 as default value for alpha
					}
				}
			}
		}

		void GraphicsModule::CreateImageData(Assets::TextureData* result, const Assets::TextureDesc& desc)
		{
			using namespace Diligent;
			result->mTexDesc.Type = desc.mType;
			result->mTexDesc.Usage = desc.mUsage;
			result->mTexDesc.BindFlags = desc.mBindFlags;
			result->mTexDesc.Format = desc.mFormat;
			result->mTexDesc.CPUAccessFlags = desc.mCPUAccessFlags;
			result->mTexDesc.Width = desc.mWidth;
			result->mTexDesc.Height = desc.mHeight;
			result->mTexDesc.MipLevels = desc.mMipLevels;

			Uint32 AvailMips = ComputeMipLevelsCount(result->mTexDesc.Width, result->mTexDesc.Height);
			Uint32 MipsToUse = 1;

			if (desc.mMipLevels > 0)
				MipsToUse = std::min(AvailMips, desc.mMipLevels);
			else
				MipsToUse = AvailMips;

			result->mSubresources = std::vector<TextureSubResData>(MipsToUse);
			result->mMips = std::vector< std::vector<Uint8> >(MipsToUse);

			ModifyImageComponents(result, desc);

			auto& pSubResources = result->mSubresources;
			auto& Mips = result->mMips;

			//Allocate memory for the mipmaps
			for (Uint32 m = 1; m < MipsToUse; ++m)
			{
				auto MipLevelProps = GetMipLevelProperties(result->mTexDesc, m);
				Mips[m].resize(StaticCast<size_t>(MipLevelProps.MipSize));
				pSubResources[m].pData = Mips[m].data();
				pSubResources[m].Stride = MipLevelProps.RowSize;
			}

			if (desc.mGenerateMipsOnCPU)
			{
				for (Uint32 m = 1; m < MipsToUse; ++m)
				{
					auto FinerMipProps = GetMipLevelProperties(result->mTexDesc, m - 1);
					ComputeMipLevelAttribs Attribs;
					Attribs.Format = result->mTexDesc.Format;
					Attribs.FineMipWidth = FinerMipProps.LogicalWidth;
					Attribs.FineMipHeight = FinerMipProps.LogicalHeight;
					Attribs.pFineMipData = pSubResources[m - 1].pData;
					Attribs.FineMipStride = StaticCast<size_t>(pSubResources[m - 1].Stride);
					Attribs.pCoarseMipData = Mips[m].data();
					Attribs.CoarseMipStride = StaticCast<size_t>(pSubResources[m].Stride);
					Attribs.AlphaCutoff = desc.mMipMapsAlphaCutoff;
					Attribs.FilterType = desc.mMipMapsFilter;
					ComputeMipLevel(Attribs);
				}
			}
		}

		void GraphicsModule::ModifyImageComponents(Assets::TextureData* result, const Assets::TextureDesc& desc)
		{
			Uint32 NumComponents = 0;
			const auto ChannelDepth = GetValueSize(desc.mComponentType) * 8;

			bool IsSRGB = (desc.mNumComponents >= 3 && ChannelDepth == 8) ? desc.mIsSRGB : false;
			if (result->mTexDesc.Format == TEX_FORMAT_UNKNOWN)
			{
				NumComponents = desc.mNumComponents == 3 ? 4 : desc.mNumComponents;

				if (ChannelDepth == 8)
				{
					switch (NumComponents)
					{
					case 1: result->mTexDesc.Format = TEX_FORMAT_R8_UNORM; break;
					case 2: result->mTexDesc.Format = TEX_FORMAT_RG8_UNORM; break;
					case 4: result->mTexDesc.Format = IsSRGB ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM; break;
					default: LOG_ERROR_AND_THROW("Unexpected number of color channels (", desc.mNumComponents, ")");
					}
				}
				else if (ChannelDepth == 16)
				{
					switch (NumComponents)
					{
					case 1: result->mTexDesc.Format = TEX_FORMAT_R16_UNORM; break;
					case 2: result->mTexDesc.Format = TEX_FORMAT_RG16_UNORM; break;
					case 4: result->mTexDesc.Format = TEX_FORMAT_RGBA16_UNORM; break;
					default: LOG_ERROR_AND_THROW("Unexpected number of color channels (", desc.mNumComponents, ")");
					}
				}
				else if (ChannelDepth == 32)
				{
					switch (NumComponents)
					{
					case 1: result->mTexDesc.Format = TEX_FORMAT_R16_FLOAT; break;
					case 2: result->mTexDesc.Format = TEX_FORMAT_RG16_FLOAT; break;
					case 4: result->mTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT; break;
					default: LOG_ERROR_AND_THROW("Unexpected number of color channels (", desc.mNumComponents, ")");
					}
				}
				else
					LOG_ERROR_AND_THROW("Unsupported color channel depth (", ChannelDepth, ")");
			}
			else
			{
				const auto& TexFmtDesc = GetTextureFormatAttribs(result->mTexDesc.Format);

				NumComponents = TexFmtDesc.NumComponents;
				if (TexFmtDesc.ComponentSize != ChannelDepth / 8)
					LOG_ERROR_AND_THROW("Image channel size ", ChannelDepth, " is not compatible with texture format ", TexFmtDesc.Name);
			}

			auto& pSubResources = result->mSubresources;
			auto& Mips = result->mMips;

			if (desc.mNumComponents != NumComponents)
			{
				auto DstStride = desc.mWidth * NumComponents * ChannelDepth / 8;
				DstStride = AlignUp(DstStride, Uint32{ 4 });
				Mips[0].resize(size_t{ DstStride } *size_t{ desc.mHeight });
				pSubResources[0].pData = Mips[0].data();
				pSubResources[0].Stride = DstStride;
				if (ChannelDepth == 8)
				{
					ModifyComponentCount<Uint8>(desc.mData, desc.mRowStride, desc.mNumComponents,
						Mips[0].data(), DstStride,
						desc.mWidth, desc.mHeight, NumComponents);
				}
				else if (ChannelDepth == 16)
				{
					ModifyComponentCount<Uint16>(desc.mData, desc.mRowStride, desc.mNumComponents,
						Mips[0].data(), DstStride,
						desc.mWidth, desc.mHeight, NumComponents);
				}
				else if (ChannelDepth == 32)
				{
					ModifyComponentCount<float>(desc.mData, desc.mRowStride, desc.mNumComponents,
						Mips[0].data(), DstStride,
						desc.mWidth, desc.mHeight, NumComponents);
				}
			}
			else
			{
				auto MipLevelProps = GetMipLevelProperties(result->mTexDesc, 0);

				pSubResources[0].pData = desc.mData;
				pSubResources[0].Stride = MipLevelProps.RowSize;
			}
		}

		bool GraphicsModule::isGammaCorrect()
		{
			if (GetSwapChain()->GetDesc().ColorBufferFormat == Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB)
				return true;
			else
				return false;
		}

		Assets::TextureUsageType GraphicsModule::ParseTexUsageFromName(std::string& name)
		{
			if (name.find("Diffuse") == 0)
				return Assets::TextureUsageType::Diffuse;
			else if (name.find("Albedo") == 0)
				return Assets::TextureUsageType::Albedo;
			else if (name.find("Specular") == 0)
				return Assets::TextureUsageType::Specular;
			else if (name.find("Metallic") == 0)
				return Assets::TextureUsageType::Metallic;
			else if (name.find("Normal") == 0)
				return Assets::TextureUsageType::Normal;
			else if (name.find("Roughness") == 0)
				return Assets::TextureUsageType::Roughness;
			else if (name.find("AO") == 0)
				return Assets::TextureUsageType::AO;

			else if (name.find("SSAO") == 0 || name.find("AmbientOcclusion") == 0)
				return Assets::TextureUsageType::AmbientOcclusion;

			//IBL
			else if (name.find("IrradianceMap") == 0)
				return Assets::TextureUsageType::IrradianceMap;
			else if (name.find("PrefilterMap") == 0)
				return Assets::TextureUsageType::PreFilterMap;
			else if (name.find("BRDF_LUT") == 0)
				return Assets::TextureUsageType::BRDF_LUT;

			//Shadows
			else if (name.find("ShadowMap") == 0)
				return Assets::TextureUsageType::ShadowTex;

			return Assets::TextureUsageType::Unknown;
		}

		bool GraphicsModule::ReflectShader(const ShaderBuildDesc& desc, ShaderReflection& out)
		{
			using namespace Diligent;

			//create pixel shader
			RefCntAutoPtr<IShader> PShader;
			ShaderObjectCreationDesc pixelshader;

			if ((desc.mSupportedTechniques == SupportedRenderingTechnique::ForwardDeffered
				|| desc.mSupportedTechniques == SupportedRenderingTechnique::ForwardOnly)
				&& desc.mPipelineDesc.mForwardPSOCreateInfo.mValid)
				pixelshader = desc.mPipelineDesc.mForwardPSOCreateInfo.mPixelShader;
			else if (desc.mSupportedTechniques == SupportedRenderingTechnique::DefferedOnly && desc.mPipelineDesc.mGBufferPSOCreateInfo.mValid)
				pixelshader = desc.mPipelineDesc.mGBufferPSOCreateInfo.mPixelShader;
			else if (desc.mSupportedTechniques == SupportedRenderingTechnique::DefferedOnly && desc.mPipelineDesc.mDefferedPSOCreateInfo.mValid)
				pixelshader = desc.mPipelineDesc.mDefferedPSOCreateInfo.mPixelShader;
			else
			{
				NUCLEAR_ERROR("[GraphicsModule] ReflectShader Error: All Pipelines are invalid!");
				return false;
			}

			//just for reflection sake
			pixelshader.mDefines.insert("NE_DIR_LIGHTS_NUM 1");
			pixelshader.mDefines.insert("NE_SPOT_LIGHTS_NUM 1");
			pixelshader.mDefines.insert("NE_POINT_LIGHTS_NUM 1");
			pixelshader.mDefines.insert("NE_MAX_DIR_CASTERS 1");
			pixelshader.mDefines.insert("NE_MAX_SPOT_CASTERS 1");
			pixelshader.mDefines.insert("NE_MAX_OMNIDIR_CASTERS 1");

			pixelshader.mName = pixelshader.mName + "_ShaderReflection_TempObject";
			CreateShader(PShader.RawDblPtr(), pixelshader);
			if (!PShader)
			{
				return false;
			}
			int slot = 0;
			for (Uint32 i = 0; i < PShader->GetResourceCount(); i++)
			{
				Diligent::ShaderResourceDesc RsrcDesc;
				PShader->GetResourceDesc(i, RsrcDesc);

				if (RsrcDesc.Type == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
				{
					std::string name = RsrcDesc.Name;
					if (name.find("NEMat_") != std::string::npos)
					{
						Assets::ShaderTexture ReflectedTex;
						ReflectedTex.mTex.pTexture = Fallbacks::FallbacksModule::Get().GetDefaultBlackImage();
						ReflectedTex.mTex.mUsageType = ParseTexUsageFromName(std::string(name).erase(0, 6));
						ReflectedTex.mSlot = slot;
						ReflectedTex.mName = name;
						ReflectedTex.mType = Assets::ShaderTextureType::MaterialTex;
						out.mMaterialTexturesInfo.push_back(ReflectedTex);
						slot++;
					}
				}
			}
			return true;
		}

		void GraphicsModule::CreateShader(Diligent::IShader** result, const Graphics::ShaderObjectCreationDesc& desc)
		{
			Diligent::ShaderCreateInfo CreationAttribs;

			CreationAttribs.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
			CreationAttribs.Desc.UseCombinedTextureSamplers = desc.mUseCombinedTextureSamplers;
			CreationAttribs.Desc.ShaderType = desc.mType;
			CreationAttribs.EntryPoint = desc.mEntrypoint.c_str();
			CreationAttribs.Desc.Name = desc.mName.c_str();
			CreationAttribs.pShaderSourceStreamFactory = pShaderSourceFactory;

			std::string Source = desc.mSource;
			if (Source.empty())
			{
				Source = Platform::FileSystem::Get().LoadShader(desc.mPath.GetRealPath(), desc.mDefines, std::set<std::string>(), true);
			}
			else
			{
				//add defines
				if (desc.mDefines.size() > 0)
				{
					std::vector<std::string> MergedCode;

					std::string firstLine = Source.substr(0, Source.find("\n"));

					for (auto& i : desc.mDefines)
					{
						std::string define = "#define " + i + "\n";
						MergedCode.push_back(define);
					}

					MergedCode.push_back(Source);
					std::string str;
					for (unsigned int i = 0; i < MergedCode.size(); ++i)
						str = str + MergedCode[i].c_str();

					Source = str;
				}

			}

			CreationAttribs.Source = Source.c_str();

			GetDevice()->CreateShader(CreationAttribs, result);
		}

		Diligent::IShaderSourceInputStreamFactory* GraphicsModule::GetDefaultShaderSourceFactory()
		{
			return pShaderSourceFactory;
		}

		void GraphicsModule::InitPSOResources(Diligent::GraphicsPipelineStateCreateInfo& PSOCreateInfo, PSOResourcesInitInfo& desc)
		{
			if (desc.mResources.size() == 0)
			{
				ReflectShaderResources(PSOCreateInfo.pVS, desc);
				ReflectShaderResources(PSOCreateInfo.pPS, desc);
				ReflectShaderResources(PSOCreateInfo.pDS, desc);
				ReflectShaderResources(PSOCreateInfo.pHS, desc);
				ReflectShaderResources(PSOCreateInfo.pGS, desc);
				ReflectShaderResources(PSOCreateInfo.pAS, desc);
				ReflectShaderResources(PSOCreateInfo.pMS, desc);
			}

			if (desc.mStaticSamplers.size() == 0)
			{
				for (auto& i : desc.mTexturesSamplers)
				{
					bool match = false;
					for (auto& j : mDefaultStaticSamplers)
					{
						if (i.first.find(j.first) != std::string::npos)
						{
							Diligent::ImmutableSamplerDesc samdesc(i.second.ShaderStages, i.first.c_str() , j.second);
							desc.mStaticSamplers.push_back(samdesc);
							match = true;
						}
					}

					if (!match)
					{
						Diligent::ImmutableSamplerDesc samdesc(i.second.ShaderStages, i.first.c_str(), SamLinearWrapDesc);
						desc.mStaticSamplers.push_back(samdesc);
					}
				}
			}

			PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = static_cast<Uint32>(desc.mResources.size());
			PSOCreateInfo.PSODesc.ResourceLayout.Variables = desc.mResources.data();

			PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Uint32>(desc.mStaticSamplers.size());
			PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = desc.mStaticSamplers.data();
		}


		void GraphicsModule::CreateShader(const std::string& source, Diligent::IShader** result, Diligent::SHADER_TYPE type)
		{
			Diligent::ShaderCreateInfo CreationAttribs;
			CreationAttribs.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
			CreationAttribs.Desc.UseCombinedTextureSamplers = true;
			CreationAttribs.Desc.ShaderType = type;
			CreationAttribs.EntryPoint = "main";
			CreationAttribs.Source = source.c_str();

			GetDevice()->CreateShader(CreationAttribs, result);
		}

		void GraphicsModule::ReflectShaderResources(Diligent::IShader* Shader, PSOResourcesInitInfo& result)
		{
			if (Shader)
			{
				for (Uint32 i = 0; i < Shader->GetResourceCount(); i++)
				{
					Diligent::ShaderResourceDesc RsrcDesc;
					Shader->GetResourceDesc(i, RsrcDesc);

					Diligent::ShaderResourceVariableDesc Desc;
					Desc.Name = RsrcDesc.Name;
					Desc.Type = ParseNameToGetType(RsrcDesc.Name);
					Desc.ShaderStages = Shader->GetDesc().ShaderType;

					std::string name(RsrcDesc.Name);
					if (name.find("_sampler") != std::string::npos)
						result.mTexturesSamplers[name.erase(name.length() - 8)] = Desc;
					else					
						result.mResources.push_back(Desc);
					
				}
			}
		}

		Diligent::SHADER_RESOURCE_VARIABLE_TYPE GraphicsModule::ParseNameToGetType(const std::string& name)
		{
			using namespace Diligent;
			if (name.find("NEStatic") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
			else if (name.find("NEMutable") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
			else if (name.find("NEDynamic") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
			else if (name.find("NEMat") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
			else if (name.find("NE_RT") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
			else if (name.find("NE_FX_") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
			else if (name.find("NEIBL") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
			else if (name.find("NE_ShadowMap") == 0)
				return SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

			return SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
		}

		ShaderPipelineVariantFactory& GraphicsModule::GetDefaultShaderPipelineVariantFactory()
		{
			return mDefaultVariantFactory;
		}


		GraphicsModule::GraphicsModule()
		{
			pDevice = nullptr;
			pContext = nullptr;
			pSwapChain = nullptr;
			pEngineFactory = nullptr;

			{
				mRenderToTextureInputLayout.clear();
				mRenderToTextureInputLayout.push_back(LayoutElement(0, 0, 3, VT_FLOAT32, false));  //POS
				mRenderToTextureInputLayout.push_back(LayoutElement(1, 0, 2, VT_FLOAT32, false));  //UV
			}

			{
				mRendering3D_InputLayout.clear();
				mRendering3D_InputLayout.push_back(LayoutElement(0, 0, 3, VT_FLOAT32, false));  //POS
				mRendering3D_InputLayout.push_back(LayoutElement(1, 0, 2, VT_FLOAT32, false));  //UV
				mRendering3D_InputLayout.push_back(LayoutElement(2, 0, 3, VT_FLOAT32, false));  //NORMAL
				mRendering3D_InputLayout.push_back(LayoutElement(3, 0, 3, VT_FLOAT32, false));  //Tangents
				mRendering3D_InputLayout.push_back(LayoutElement(4, 0, 3, VT_FLOAT32, false));  //Bitangents
				mRendering3D_InputLayout.push_back(LayoutElement(5, 0, 4, VT_INT32, false));    //BONE ID
				mRendering3D_InputLayout.push_back(LayoutElement(6, 0, 4, VT_FLOAT32, false));  //WEIGHT
			}

			SamLinearWrapDesc = SamplerDesc(FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
				TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP);

			SamPointBorderDesc = SamplerDesc(FILTER_TYPE_POINT, FILTER_TYPE_POINT, FILTER_TYPE_POINT,
				TEXTURE_ADDRESS_BORDER, TEXTURE_ADDRESS_BORDER, TEXTURE_ADDRESS_BORDER);

			SamLinearClampDesc = SamplerDesc(FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
				TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);

			ShadowMapSamplerDesc = SamPointBorderDesc;
			ShadowMapSamplerDesc.BorderColor[0] = 1.0f;
			ShadowMapSamplerDesc.BorderColor[1] = 1.0f;
			ShadowMapSamplerDesc.BorderColor[2] = 1.0f;
			ShadowMapSamplerDesc.BorderColor[3] = 1.0f;
		}

		void GraphicsModule::ResizeSwapChain(Uint32 Width, Uint32 Height)
		{
			if (pSwapChain)
				pSwapChain->Resize(Width, Height);
		}

		bool GraphicsModule::InitializeDiligentEngine(SDL_Window* window, const RENDER_DEVICE_TYPE& type, const SwapChainDesc& SCDesc)
		{
			Uint32 NumDeferredCtx = 0;

			SDL_SysWMinfo wmInfo;
			SDL_VERSION(&wmInfo.version);
			SDL_GetWindowWMInfo(window, &wmInfo);

			FullScreenModeDesc FSDesc;
			NativeWindow DLWindow;
			DLWindow.hWnd = wmInfo.info.win.window;


			auto FindAdapter = [this](auto* pFactory, Version GraphicsAPIVersion, GraphicsAdapterInfo& AdapterAttribs) {
				Uint32 NumAdapters = 0;
				pFactory->EnumerateAdapters(GraphicsAPIVersion, NumAdapters, nullptr);
				std::vector<GraphicsAdapterInfo> Adapters(NumAdapters);
				if (NumAdapters > 0)
					pFactory->EnumerateAdapters(GraphicsAPIVersion, NumAdapters, Adapters.data());
				else
					LOG_ERROR_AND_THROW("Failed to find compatible hardware adapters");

				NUCLEAR_TRACE("[GraphicsModule] Available GPU Adapters: ");
				for (Uint32 i = 0; i < Adapters.size(); ++i)
				{
					const auto& AdapterInfo = Adapters[i];

					NUCLEAR_TRACE("[ID: {0}] {1} - Type: {2} - Dedicated Memory: {3}, Total Memory: {4}",
						i, AdapterInfo.Description, GetAdapterTypeString(AdapterInfo.Type), AdapterInfo.Memory.LocalMemory / 1024 / 1024, (AdapterInfo.Memory.LocalMemory + AdapterInfo.Memory.HostVisibleMemory + AdapterInfo.Memory.UnifiedMemory) / 1024 / 1024);
				}

				auto AdapterId = mAdapterId;
				if (AdapterId != DEFAULT_ADAPTER_ID)
				{
					if (AdapterId < Adapters.size())
					{
						mAdapterType = Adapters[AdapterId].Type;
					}
					else
					{
						LOG_ERROR_MESSAGE("Adapter ID (", AdapterId, ") is invalid. Only ", Adapters.size(), " compatible ", (Adapters.size() == 1 ? "adapter" : "adapters"), " present in the system");
						AdapterId = DEFAULT_ADAPTER_ID;
					}
				}

				if (AdapterId == DEFAULT_ADAPTER_ID && mAdapterType != ADAPTER_TYPE_UNKNOWN)
				{
					for (Uint32 i = 0; i < Adapters.size(); ++i)
					{
						if (Adapters[i].Type == mAdapterType)
						{
							AdapterId = i;
							break;
						}
					}
					if (AdapterId == DEFAULT_ADAPTER_ID)
						LOG_WARNING_MESSAGE("Unable to find the requested adapter type. Using default adapter.");
				}

				if (AdapterId == DEFAULT_ADAPTER_ID)
				{
					mAdapterType = ADAPTER_TYPE_UNKNOWN;
					for (Uint32 i = 0; i < Adapters.size(); ++i)
					{
						const auto& AdapterInfo = Adapters[i];
						const auto  AdapterType = AdapterInfo.Type;
						static_assert((ADAPTER_TYPE_DISCRETE > ADAPTER_TYPE_INTEGRATED &&
							ADAPTER_TYPE_INTEGRATED > ADAPTER_TYPE_SOFTWARE &&
							ADAPTER_TYPE_SOFTWARE > ADAPTER_TYPE_UNKNOWN),
							"Unexpected ADAPTER_TYPE enum ordering");
						if (AdapterType > mAdapterType)
						{
							// Prefer Discrete over Integrated over Software
							mAdapterType = AdapterType;
							AdapterId = i;
						}
						else if (AdapterType == mAdapterType)
						{
							// Select adapter with more memory
							const auto& NewAdapterMem = AdapterInfo.Memory;
							const auto  NewTotalMemory = NewAdapterMem.LocalMemory + NewAdapterMem.HostVisibleMemory + NewAdapterMem.UnifiedMemory;
							const auto& CurrAdapterMem = Adapters[AdapterId].Memory;
							const auto  CurrTotalMemory = CurrAdapterMem.LocalMemory + CurrAdapterMem.HostVisibleMemory + CurrAdapterMem.UnifiedMemory;
							if (NewTotalMemory > CurrTotalMemory)
							{
								AdapterId = i;
							}
						}
					}
				}

				if (AdapterId != DEFAULT_ADAPTER_ID)
				{
					AdapterAttribs = Adapters[AdapterId];
				}

				return AdapterId;
			};

			switch (type)
			{
			case RENDER_DEVICE_TYPE_D3D11:
			{
				EngineD3D11CreateInfo EngineCI;
				auto* pFactoryD3D11 = LoadAndGetEngineFactoryD3D11();
				pFactoryD3D11->SetMessageCallback(DiligentMassageCallback);

				EngineCI.GraphicsAPIVersion = { 11, 0 };
				pEngineFactory = pFactoryD3D11;

				EngineCI.AdapterId = FindAdapter(pFactoryD3D11, EngineCI.GraphicsAPIVersion, mAdapterAttribs);

				pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &pDevice,
					&pContext);
				pFactoryD3D11->CreateSwapChainD3D11(pDevice, pContext,
					SCDesc, FSDesc, DLWindow, &pSwapChain);

			}
			break;

			case RENDER_DEVICE_TYPE_D3D12:
			{
				EngineD3D12CreateInfo EngineCI;
				EngineCI.GraphicsAPIVersion = { 11, 0 };

				auto* pFactoryD3D12 = LoadAndGetEngineFactoryD3D12();
				pFactoryD3D12->SetMessageCallback(DiligentMassageCallback);

				pEngineFactory = pFactoryD3D12;
				EngineCI.AdapterId = FindAdapter(pFactoryD3D12, EngineCI.GraphicsAPIVersion, mAdapterAttribs);

				pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &pDevice,
					&pContext);
				pFactoryD3D12->CreateSwapChainD3D12(pDevice, pContext,
					SCDesc, FSDesc, DLWindow, &pSwapChain);
			}
			break;

			case RENDER_DEVICE_TYPE_GL:
			{

				auto* pFactoryOpenGL = LoadAndGetEngineFactoryOpenGL();
				pFactoryOpenGL->SetMessageCallback(DiligentMassageCallback);

				pEngineFactory = pFactoryOpenGL;

				EngineGLCreateInfo CreationCreateInfo;
				CreationCreateInfo.Window = DLWindow;
				pFactoryOpenGL->CreateDeviceAndSwapChainGL(
					CreationCreateInfo, &pDevice, &pContext, SCDesc, &pSwapChain);

			}
			break;

			case RENDER_DEVICE_TYPE_VULKAN:
			{
				EngineVkCreateInfo EngVkCreateInfo;

				auto* pFactoryVk = LoadAndGetEngineFactoryVk();
				pFactoryVk->SetMessageCallback(DiligentMassageCallback);

				pEngineFactory = pFactoryVk;
				EngVkCreateInfo.AdapterId = FindAdapter(pFactoryVk, EngVkCreateInfo.GraphicsAPIVersion, mAdapterAttribs);

				pFactoryVk->CreateDeviceAndContextsVk(EngVkCreateInfo, &pDevice,
					&pContext);
				pFactoryVk->CreateSwapChainVk(pDevice, pContext,
					SCDesc, DLWindow, &pSwapChain);

			}
			break;

			default:
				NUCLEAR_FATAL("[InitializeDiligentEngineWin32] Unknown Device type!");
				return false;
			}

			return true;
		}
	}
}
