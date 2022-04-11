#include <Engine\Graphics\Context.h>
#include <Engine.h>
#include <Core/Window.h>
#include "InitializeDiligentEngineWin32.h"
#include <Core\Logger.h>

namespace Nuclear
{
	namespace Graphics
	{
		static IRenderDevice* gDevice;
		static IDeviceContext* gContext;
		static ISwapChain* gSwapChain;
		static IEngineFactory* gEngineFactory;

		bool OpenGL = false;
		bool Vulkan = false;
		bool Direct3D = false;
		bool Metal = false;


		bool Context::Initialize(RENDER_DEVICE_TYPE renderapi, const Graphics::GraphicsEngineDesc& GraphicsDesc)
		{
			SwapChainDesc SCDesc(GraphicsDesc.SCDesc);

			if (GraphicsDesc.GammaCorrect)
			{
				SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
			}
			else
			{
				SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
			}
			bool Result = InitializeDiligentEngineWin32(Core::Engine::GetInstance()->GetMainWindow()->GetRawWindowPtr(),renderapi, &gDevice, &gContext, &gSwapChain,&gEngineFactory, SCDesc);

			if(Result)
				Log.Info("[Context] Diligent Graphics API Initialized.\n");

			return true;
		}

		void Context::ShutDown()
		{
			gDevice->Release();
			gContext->Release();
			gSwapChain->Release();
		}

		void Context::PresentFrame()
		{
			gSwapChain->Present();
		}

		bool Context::IsOpenGL()
		{
			return OpenGL;
		}

		bool Context::IsVulkan()
		{
			return Vulkan;
		}

		bool Context::IsDirect3D()
		{
			return Direct3D;
		}

		void Context::ResizeSwapChain(Uint32 Width, Uint32 Height)
		{
			if (gSwapChain)
				gSwapChain->Resize(Width, Height);
		}

		IRenderDevice * Context::GetDevice()
		{
			return gDevice;
		}

		IDeviceContext * Context::GetContext()
		{
			return gContext;
		}

		ISwapChain * Context::GetSwapChain()
		{
			return gSwapChain;
		}
		IEngineFactory* Context::GetEngineFactory()
		{
			return gEngineFactory;
		}
	}
}