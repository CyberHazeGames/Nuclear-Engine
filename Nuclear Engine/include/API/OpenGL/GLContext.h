#pragma once
#include <API\OpenGL\GLCommon.h>

#ifdef NE_COMPILE_CORE_OPENGL
#include <API\Color.h>
#include <API\API_Types.h>

namespace NuclearEngine
{
	namespace API 
	{
		namespace OpenGL 
		{
			
			class NEAPI GLContext
			{
			public:
				static bool Initialize();
				static void SetPrimitiveType(int primitivetype);

				static void Clear(API::Color color, uint flags, float depth = 1.0f, unsigned int stencil = 0);

				static void EnableDepthBuffer(bool state);

				static void SwapBuffers();
				static void Shutdown();

				static void Draw(unsigned int count);
				static void DrawIndexed(unsigned int vertexCount);

				static void SetViewPort(int x, int y, int width, int height);
			};
		}
	}
}

#endif