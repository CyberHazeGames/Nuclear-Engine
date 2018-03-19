#pragma once

#include <Graphics/API/ShaderCompiler.h>

#ifdef NE_COMPILE_XSHADERCOMPILER
#include <Graphics/API/Shader_Types.h>

namespace NuclearEngine
{
	namespace Graphics 
	{

		namespace API
		{
			namespace XShaderCompiler
			{
				void CompileHLSL2GLSL(BinaryShaderBlob *result, std::string SourceCode, Graphics::API::ShaderType type);
			}
		}
	}
}
#endif