#pragma once
#include <Core/EngineModule.h>
#include <Diligent/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Diligent/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/ShaderPipelineVariantFactory.h>
#include <Assets/TextureDesc.h>
#include <Diligent/Graphics/GraphicsEngine/interface/InputLayout.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Shader.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Graphics/ShaderTypes.h>
#include <Graphics/ShaderReflection.h>
#include <unordered_map>
#include <Graphics\GraphicsModuleDesc.h>


namespace Diligent
{
	struct ISwapChain;
	struct IEngineFactory;
}

namespace Nuclear
{
	namespace Assets
	{
		class Texture;
		class Mesh;
	}
	namespace Graphics
	{
		class NEAPI GraphicsModule : public Core::EngineModule
		{
		public:
			static GraphicsModule& Get();
			void SetStartupDesc(const GraphicsModuleDesc& desc) { mStartupDesc = desc; }
			bool OnInitialize() override;
			bool Initialize(const GraphicsModuleDesc& desc);
			void Shutdown() override;

			void PresentFrame();

			FORCE_INLINE Diligent::IRenderDevice* GetDevice() const
			{
				return pDevice;
			}

			FORCE_INLINE Diligent::IDeviceContext* GetContext() const
			{
				return pContext;
			}

			FORCE_INLINE Diligent::ISwapChain* GetSwapChain() const
			{
				return pSwapChain;
			}

			FORCE_INLINE Diligent::IEngineFactory* GetEngineFactory() const
			{
				return pEngineFactory;
			}
			FORCE_INLINE const Diligent::GraphicsAdapterInfo GetAdapterInfo() const
			{
				return mAdapterAttribs;
			}
			void ResizeSwapChain(Uint32 Width, Uint32 Height);

			bool CreateMesh(Assets::Mesh* result);

			bool CreateImage(Assets::Texture* result, Assets::TextureData* desc);

			void CreateImageData(Assets::TextureData* result, const Assets::TextureDesc& desc);

			void ModifyImageComponents(Assets::TextureData* result, const Assets::TextureDesc& desc);
			void GenerateMipMapsCPU(Assets::TextureData* result, const Assets::TextureDesc& desc);

			Assets::TextureUsageType ParseTexUsageFromName(std::string& name);

			bool isGammaCorrect();

			bool ReflectShader(const ShaderBuildDesc& desc, ShaderReflection& out);

			FORCE_INLINE const std::vector<Diligent::LayoutElement>& GetRendering3DInputLayout() const
			{
				return mRendering3D_InputLayout;
			}

			FORCE_INLINE const std::vector<Diligent::LayoutElement>& GetRenderToTextureInputLayout() const
			{
				return mRenderToTextureInputLayout;
			}

			void CreateShader(Diligent::IShader** result, const Graphics::ShaderObjectCreationDesc& desc);
			Diligent::IShaderSourceInputStreamFactory* GetDefaultShaderSourceFactory();

			void InitPSOResources(Diligent::GraphicsPipelineStateCreateInfo& result, PSOResourcesInitInfo& desc);

			void CreateShader(const std::string& source, Diligent::IShader** shader, Diligent::SHADER_TYPE type);

			Diligent::SHADER_RESOURCE_VARIABLE_TYPE ParseNameToGetType(const std::string& name);

			ShaderPipelineVariantFactory& GetDefaultShaderPipelineVariantFactory();

		protected:
			void ReflectShaderResources(Diligent::IShader* Shader, PSOResourcesInitInfo& result);
			Diligent::RefCntAutoPtr<Diligent::IShaderSourceInputStreamFactory> pShaderSourceFactory;
			ShaderPipelineVariantFactory mDefaultVariantFactory;

			std::vector<Diligent::LayoutElement> mRendering3D_InputLayout;
			std::vector<Diligent::LayoutElement> mRenderToTextureInputLayout;

			std::unordered_map<std::string, Diligent::SamplerDesc> mDefaultStaticSamplers;

			Diligent::SamplerDesc SamLinearWrapDesc;
			Diligent::SamplerDesc SamLinearClampDesc;
			Diligent::SamplerDesc SamPointBorderDesc;
			Diligent::SamplerDesc ShadowMapSamplerDesc;

			GraphicsModule();

		private:
			GraphicsModuleDesc mStartupDesc{};
			Diligent::IRenderDevice* pDevice;
			Diligent::IDeviceContext* pContext;
			Diligent::ISwapChain* pSwapChain;
			Diligent::IEngineFactory* pEngineFactory;

			Diligent::GraphicsAdapterInfo mAdapterAttribs;
			Uint32       mAdapterId = Diligent::DEFAULT_ADAPTER_ID;
			Diligent::ADAPTER_TYPE mAdapterType = Diligent::ADAPTER_TYPE_UNKNOWN;
			std::string  mAdapterDetailsString;

			bool InitializeDiligentEngine(SDL_Window* window, const Diligent::RENDER_DEVICE_TYPE& type, const Diligent::SwapChainDesc& SCDesc);
		};
	}
}
