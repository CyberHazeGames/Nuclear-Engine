#pragma once

namespace NuclearEngine {
	enum class ShaderType;
}
namespace NuclearRenderer {

	class NRBUniformBuffer;

	class NRBShader
	{
	public:
		virtual bool Create(const char* VertexShaderCode, const char* PixelShaderCode, const char* GeometryShaderCode, NuclearEngine::ShaderType Input) = 0;

		virtual void SetUniformBuffer(NRBUniformBuffer* ubuffer) = 0;
		virtual void Delete() = 0;
		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		//OpenGL Specific
		virtual unsigned int GetGLShaderID() = 0;

		//DirectX 11 Specific
		virtual void * GetDXBufferPointer() = 0;
		virtual unsigned long GetDXBufferSize() =0;
	};
}