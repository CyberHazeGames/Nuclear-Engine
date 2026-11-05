#pragma once
#include <Diligent/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

namespace Nuclear::Platform { class Window; }

namespace Nuclear
{
	namespace Graphics
	{
		/// Device, presentation, and UI settings used when the graphics module starts.
		struct GraphicsModuleDesc
		{
			/// Rendering backend to create; Windows currently defaults to D3D11.
			Diligent::RENDER_DEVICE_TYPE mRenderAPI = Diligent::RENDER_DEVICE_TYPE::RENDER_DEVICE_TYPE_D3D11;

			/// Creates an ImGui context and attaches it to the supplied platform window.
			bool InitImGui = true;
			/// Borrowed window; must remain alive until the graphics module shuts down.
			Platform::Window* pWindowHandle = nullptr;

			/// Initial swap-chain settings, including dimensions and buffer formats.
			Diligent::SwapChainDesc SCDesc;

			/// Selects an sRGB color buffer, overriding SCDesc.ColorBufferFormat.
			bool GammaCorrect = false;
		};

	}
}
