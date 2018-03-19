#pragma once
#include <Graphics/API/DirectX\DX11Common.h>

#ifdef NE_COMPILE_DIRECTX11
#include <Graphics/API/Shader_Types.h>
namespace NuclearEngine
{
	namespace Graphics
	{
		namespace API
		{
			namespace DirectX
			{
				class DX11ConstantBuffer;
				class DX11VertexBuffer;

				class NEAPI DX11VertexShader
				{
					friend class DX11VertexBuffer;
				public:
					DX11VertexShader();
					~DX11VertexShader();

					static void Create(DX11VertexShader *shader, BinaryShaderBlob* desc);
					static void Delete(DX11VertexShader *shader);

					void SetConstantBuffer(DX11ConstantBuffer* ubuffer);

					void Bind();
				private:

					ID3D11VertexShader * VertexShader;
					ShaderReflection Reflection;

					/*Used for InputLayouts*/
					void* VS_Buffer;
					size_t VS_Size;
				};
			}
		}
	}
}

#endif