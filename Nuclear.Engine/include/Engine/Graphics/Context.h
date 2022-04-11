#pragma once
#include <Core\NE_Common.h>
#include <Core\Window.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Diligent/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Diligent/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Diligent/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Diligent/Graphics/GraphicsEngine/interface/EngineFactory.h>
#include "Engine\Graphics\GraphicsEngine.h"

namespace Nuclear
{
	namespace Graphics 
	{
		class NEAPI Context
		{
		public:
			static bool Initialize(RENDER_DEVICE_TYPE renderapi, const Graphics::GraphicsEngineDesc& GraphicsDesc);

			static void ShutDown();

			static void PresentFrame();

			static bool IsOpenGL();			// Returns ture if OpenGL is used as rendering API.

			static bool IsVulkan();			// Returns ture if Vulkan is used as rendering API.
			
			static bool IsDirect3D();		// Returns ture if Direct3D is used as rendering API.
			
			static void ResizeSwapChain(Uint32 Width, Uint32 Height);


			static IRenderDevice* GetDevice();
			static IDeviceContext* GetContext();
			static ISwapChain* GetSwapChain();
			static IEngineFactory* GetEngineFactory();

		};
	}
}