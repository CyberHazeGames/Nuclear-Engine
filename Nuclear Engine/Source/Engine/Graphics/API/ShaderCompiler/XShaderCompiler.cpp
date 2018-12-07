#include "XShaderCompiler.h"

#ifdef NE_COMPILE_XSHADERCOMPILER
#include <..\Nuclear Shader Compiler\inc\Xsc\Xsc.h>
#include <sstream>
#include <iostream>
#include <vector>
#pragma comment(lib,"NuclearShaderCompiler.lib")

namespace NuclearEngine
{
	namespace Graphics
	{
		namespace API
		{
			namespace XShaderCompiler
			{
				ShaderVariableType Reflect_FieldType(Xsc::Reflection::Field field)
				{
					switch (field.type)
					{
					case Xsc::Reflection::FieldType::Bool:
						return ShaderVariableType::Bool;

					case Xsc::Reflection::FieldType::Int:
						//TODO Support Integer Matrices
						if (field.dimensions[0] == 1)
						{
							if (field.dimensions[1] == 1)
								return ShaderVariableType::Int1;
							else if (field.dimensions[1] == 2)
								return ShaderVariableType::Int2;
							else if (field.dimensions[1] == 3)
								return ShaderVariableType::Int3;
							else if (field.dimensions[1] == 4)
								return ShaderVariableType::Int4;
							else
								return ShaderVariableType::Unknown;
						}
						else
							return ShaderVariableType::Unknown;

					case Xsc::Reflection::FieldType::Float:

						if (field.dimensions[0] == 1 && field.dimensions[1] == 1)
							return ShaderVariableType::Float1;
						else if (field.dimensions[0] == 2 && field.dimensions[1] == 1)
							return ShaderVariableType::Float2;
						else if (field.dimensions[0] == 3 && field.dimensions[1] == 1)
							return ShaderVariableType::Float3;
						else if (field.dimensions[0] == 4 && field.dimensions[1] == 1)
							return ShaderVariableType::Float4;
						else if (field.dimensions[0] == 2 && field.dimensions[1] == 2)
							return ShaderVariableType::Matrix2x2;
						else if (field.dimensions[0] == 3 && field.dimensions[1] == 3)
							return ShaderVariableType::Matrix3x3;
						else if (field.dimensions[0] == 4 && field.dimensions[1] == 4)
							return ShaderVariableType::Matrix4x4;
						

					default:
						//Log.Warning("[XShaderCompiler] [Reflection] Using Unsupported Variable type: " + std::to_string(Xsc::Reflection::FieldType::Bool) + " \n");
						return ShaderVariableType::Unknown;
					}
					return ShaderVariableType::Unknown;
				}
				void ParseConstantBuffers(const std::vector<Xsc::Reflection::ConstantBuffer>& cbuffers, BinaryShaderBlob * result)
				{
					for (auto cb : cbuffers)
					{
						Reflected_Constantbuffer refl_cb;
						refl_cb.BindingSlot = cb.slot;

						for (auto member : cb.fields)
						{
							ShaderVariable variable;
							//special case
							if (member.type == Xsc::Reflection::FieldType::Record)
								variable.Type = ShaderVariableType::Struct;
							else
								variable.Type = Reflect_FieldType(member);

							refl_cb.Variables[member.name] = variable;
						}
						refl_cb.Size = cb.size;
						result->Reflection.ConstantBuffers[cb.name] = refl_cb;
					}
				}
				void ParseResource(const std::vector<Xsc::Reflection::Resource>& resources, BinaryShaderBlob * result)
				{
					for (Xsc::Reflection::Resource mRes : resources)
					{
						if (mRes.type == Xsc::Reflection::ResourceType::Texture2D)
						{
							Reflected_Texture refl_tex;
							refl_tex.BindingSlot = mRes.slot;

							result->Reflection.Textures[mRes.name] = refl_tex;
						}
					}
				}
				void Reflect(Xsc::Reflection::ReflectionData* reflection, BinaryShaderBlob * result)
				{
					ParseResource(reflection->resources, result);
					ParseConstantBuffers(reflection->constantBuffers, result);
				}

				class XSC_ERROR_LOG : public Xsc::Log
				{
				public:
					void SubmitReport(const Xsc::Report& report) override
					{
						switch (report.Type())
						{
						case Xsc::ReportTypes::Info:
							Infos.push_back(report);
							break;
						case Xsc::ReportTypes::Warning:
							Warnings.push_back(report);
							break;
						case Xsc::ReportTypes::Error:
							Errors.push_back(report);
							break;
						}
					}
					std::string Get_Whole_Log()
					{
						std::string result;
						for (Uint32 i = 0; i < Infos.size(); i++)
						{
							result += std::string("[XShaderCompiler] " + Infos.at(i).Message() + '\n');
						}
						for (Uint32 i = 0; i < Warnings.size(); i++)
						{
							result += std::string("[XShaderCompiler] " + Warnings.at(i).Message() + '\n');
						}
						for (Uint32 i = 0; i < Errors.size(); i++)
						{
							result += std::string("[XShaderCompiler] " + Errors.at(i).Message() + '\n');
						}
						return result;
					}

				private:
					std::vector<Xsc::Report> Infos;
					std::vector<Xsc::Report> Warnings;
					std::vector<Xsc::Report> Errors;
				};


				void CompileHLSL2GLSL(BinaryShaderBlob * result, std::string SourceCode, LLGL::ShaderType type)
				{
					// Initialize shader input descriptor structure
					auto input = std::make_shared<std::stringstream>();

					*input << SourceCode;

					Xsc::ShaderInput inputDesc;
					inputDesc.sourceCode = input;
					inputDesc.shaderVersion = Xsc::InputShaderVersion::HLSL5;
					inputDesc.entryPoint = "main";
					switch (type)
					{
					case ShaderType::Vertex:
						inputDesc.shaderTarget = Xsc::ShaderTarget::VertexShader;
						break;
					case ShaderType::Geometry:
						inputDesc.shaderTarget = Xsc::ShaderTarget::GeometryShader;
						break;
					case ShaderType::Pixel:
						inputDesc.shaderTarget = Xsc::ShaderTarget::FragmentShader;
						break;
					default:
						break;
					}

					// Initialize shader output descriptor structure
					Xsc::ShaderOutput outputDesc;
					std::ostringstream stream;
					outputDesc.sourceCode = &stream;
					outputDesc.shaderVersion = Xsc::OutputShaderVersion::GLSL450;

					Xsc::Reflection::ReflectionData reflection;
					if (result->convertedshaderrowmajor == true)
					{
						outputDesc.options.rowMajorAlignment = true;
					}

					//Seperate shaders requirements
					outputDesc.options.autoBinding = true;
					outputDesc.options.separateShaders = true;

					// Compile HLSL code into GLSL
					XSC_ERROR_LOG log;

					if (!Xsc::CompileShader(inputDesc, outputDesc, &log, &reflection))
					{
						Log.Error("[ShaderCompiler] CompileDXBC2GLSL Failed! Info: \n" + log.Get_Whole_Log());

					}
					result->GLSL_SourceCode = stream.str();
					Reflect(&reflection, result);

					result->Converted = true;
				}
			}
		}
	}
}
#endif