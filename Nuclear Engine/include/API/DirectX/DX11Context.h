#include <API\DirectX\DX11Common.h>

#ifdef NE_COMPILE_DIRECTX11
#include <API\Color.h>
#include <Core\Application.h>

namespace NuclearEngine
{
	namespace API
	{
		namespace DirectX
		{
			class DX11RenderTarget;
			class NEAPI DX11Context
			{
				friend Core::Application;
			public:

				static void SetPrimitiveType(int primitivetype);

				static void Clear(API::Color color, uint flags, float depth = 1.0f, unsigned int stencil = 0);

				static void Draw(unsigned int count);
				static void DrawIndexed(unsigned int vertexCount);

				static void SetViewPort(int x, int y, int width, int height);

				//DX11 Specific
				static ID3D11Device* GetDevice();
				static ID3D11DeviceContext* GetContext();

				static void Bind_RenderTarget(DX11RenderTarget* rt);
				static void Bind_Default_Rasterizer_State();
				static void Bind_Default_RenderTarget();
			private:
				static bool Initialize(GLFWwindow* window);
				static void Shutdown();
				static void SwapBuffers();
			};
		}
	}
}

#endif