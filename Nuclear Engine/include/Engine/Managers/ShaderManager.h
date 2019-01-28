#pragma once
#include <Base\NE_Common.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Shader.h>
#include <Diligent/Graphics/GraphicsEngine/interface/InputLayout.h>
#include <vector>

namespace NuclearEngine 
{
	struct NuclearShaderVariableDesc
	{
		std::string Name;

		/// Shader variable type. See Diligent::SHADER_VARIABLE_TYPE for a list of allowed types
		SHADER_VARIABLE_TYPE Type;
		NuclearShaderVariableDesc(std::string _Name, SHADER_VARIABLE_TYPE _Type = SHADER_VARIABLE_TYPE_STATIC) :
			Name(_Name),
			Type(_Type)
		{}
	};

	namespace Managers
	{
		struct AutoVertexShaderDesc
		{
			bool Use_Camera = true;

			bool InTexCoords = true;
			bool InNormals = true;
			bool InTangents = false;

			bool OutFragPos = true;
		};
		struct AutoPixelShaderDesc
		{
			bool OutputTexture = true;
		};



		//This is very useful class, it automates the shader creation easily, control all their Input & output, may even store them in the future.
		class NEAPI ShaderManager
		{
		public:
			ShaderManager();
			~ShaderManager();


			static IShader* CreateShader(const std::string& source, SHADER_TYPE type, SHADER_VARIABLE_TYPE DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC);

			static IShader* CreateAutoVertexShader(const AutoVertexShaderDesc& desc, std::vector<LayoutElement>* Layout);
			static IShader* CreateAutoPixelShader(const AutoPixelShaderDesc& desc);
		};
	}
}