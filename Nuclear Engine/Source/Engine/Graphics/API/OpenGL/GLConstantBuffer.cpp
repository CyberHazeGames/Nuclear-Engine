#include <Engine/Graphics/API/OpenGL\GLConstantBuffer.h>

#ifdef NE_COMPILE_CORE_OPENGL
static int ubosBindingindex = 0;


namespace NuclearEngine
{
	namespace Graphics
	{
		namespace API
		{
			namespace OpenGL
			{
				GLConstantBuffer::GLConstantBuffer()
				{
					this->buffer = 0;
					this->name = "NewCB";
					this->BindingIndex = 0;
				}
				GLConstantBuffer::~GLConstantBuffer()
				{
					this->buffer = 0;
					this->name = "OldCB";
					this->BindingIndex = 0;
				}
				void GLConstantBuffer::Create(GLConstantBuffer* result, const char * Nameinshader, unsigned int size)
				{
					int remainder = size % 16;
					if (remainder != 0)
					{
						Log.Warning("[GLConstantBuffer] The size of buffer isn't a multiple of 16 which can cause many unexpected problems! \n");
					}

					result->name = Nameinshader;
					result->BindingIndex = ubosBindingindex;

					glGenBuffers(1, &result->buffer);
					glBindBuffer(GL_UNIFORM_BUFFER, result->buffer);
					glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_DYNAMIC_DRAW);

					glBindBufferRange(GL_UNIFORM_BUFFER, result->BindingIndex, result->buffer, 0, size);

					//Increment the binding index
					ubosBindingindex++;
					glBindBuffer(GL_UNIFORM_BUFFER, 0);
				}

				void GLConstantBuffer::Delete(GLConstantBuffer * cbuffer)
				{
					glDeleteBuffers(1, &cbuffer->buffer);					
					cbuffer->buffer = 0;
				}

				void GLConstantBuffer::Update(const void* data, unsigned int size)
				{
					glBindBuffer(GL_UNIFORM_BUFFER, buffer);
					glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
					glBindBuffer(GL_UNIFORM_BUFFER, 0);
				}

				unsigned int GLConstantBuffer::GetBindingIndex()
				{
					return BindingIndex;
				}

				const char * GLConstantBuffer::GetName()
				{
					return name;
				}

			}
		}
	}
}

#endif