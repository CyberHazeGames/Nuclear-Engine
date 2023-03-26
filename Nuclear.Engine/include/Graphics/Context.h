#pragma once
#include <NE_Common.h>
#include <Platform\Window.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Diligent/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Diligent/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Diligent/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Diligent/Graphics/GraphicsEngine/interface/EngineFactory.h>
#include "Graphics\GraphicsEngine.h"

namespace Nuclear
{
	namespace Graphics 
	{
		class NEAPI Context
		{
		public:
			Context(Context const&) = delete;
			void operator=(Context const&) = delete;

			static Context& GetInstance();

			bool Initialize(const Graphics::GraphicsEngineDesc& GraphicsDesc);

			void Shutdown();

			void PresentFrame();

			bool IsOpenGL();			// Returns ture if OpenGL is used as rendering API.

			bool IsVulkan();			// Returns ture if Vulkan is used as rendering API.
			
			bool IsDirect3D();		// Returns ture if Direct3D is used as rendering API.
			
			void ResizeSwapChain(Uint32 Width, Uint32 Height);

			Diligent::IRenderDevice* GetDevice();
			Diligent::IDeviceContext* GetContext();
			Diligent::ISwapChain* GetSwapChain();
			Diligent::IEngineFactory* GetEngineFactory();

		protected:
			Context();
			Diligent::IRenderDevice* gDevice;
			Diligent::IDeviceContext* gContext;
			Diligent::ISwapChain* gSwapChain;
			Diligent::IEngineFactory* gEngineFactory;
			Diligent::GraphicsAdapterInfo mAdapterAttribs;
			Uint32       mAdapterId = Diligent::DEFAULT_ADAPTER_ID;
			Diligent::ADAPTER_TYPE mAdapterType = Diligent::ADAPTER_TYPE_UNKNOWN;
			std::string  mAdapterDetailsString;

			bool OpenGL = false;
			bool Vulkan = false;
			bool Direct3D = false;
			bool Metal = false;

			bool InitializeDiligentEngine(SDL_Window* window, const Diligent::RENDER_DEVICE_TYPE& type, const Diligent::SwapChainDesc& SCDesc);
		};
	}
}