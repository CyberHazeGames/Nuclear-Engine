#include <Engine/Graphics/API/DirectX\DX11IndexBuffer.h>

#ifdef NE_COMPILE_DIRECTX11
#include <Engine/Graphics/API/DirectX\DX11Context.h>
#include <Engine/Graphics/API/Buffer_Types.h>

namespace NuclearEngine
{
	namespace Graphics 
	{
		namespace API
		{
			namespace DirectX
			{

				DX11IndexBuffer::DX11IndexBuffer()
				{
					buffer = nullptr;
				}
				DX11IndexBuffer::~DX11IndexBuffer()
				{
					buffer = nullptr;
				}

				void DX11IndexBuffer::Create(DX11IndexBuffer* result, const IndexBufferDesc& desc)
				{
					D3D11_BUFFER_DESC bufferDesc;
					ZeroMemory(&bufferDesc, sizeof(bufferDesc));

					if (!desc.UsePreciseSize)
						bufferDesc.ByteWidth = desc.Size * sizeof(unsigned int);
					else
						bufferDesc.ByteWidth = desc.Size;				

					if (desc.UsageType == BufferUsage::Static) {
						bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
						bufferDesc.CPUAccessFlags = 0;
					}
					else if (desc.UsageType == BufferUsage::Dynamic)
					{
						bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
						bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
					}
					else
					{
						bufferDesc.Usage = D3D11_USAGE_DEFAULT;
						bufferDesc.CPUAccessFlags = 0;
					}

					bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
					bufferDesc.MiscFlags = 0;

					if (desc.Data != NULL)
					{
						D3D11_SUBRESOURCE_DATA subData;
						ZeroMemory(&subData, sizeof(subData));

						subData.pSysMem = desc.Data;
						subData.SysMemPitch = 0;
						subData.SysMemSlicePitch = 0;

						DX11Context::GetDevice()->CreateBuffer(&bufferDesc, &subData, &result->buffer);
					}
					else {
						DX11Context::GetDevice()->CreateBuffer(&bufferDesc,NULL, &result->buffer);
					}
				}

				void DX11IndexBuffer::Delete(DX11IndexBuffer * ibuffer)
				{
					if (ibuffer->buffer != nullptr)
					{
						ibuffer->buffer->Release();
					}

					ibuffer->buffer = nullptr;
				}

				void DX11IndexBuffer::Bind()
				{
					DX11Context::GetContext()->IASetIndexBuffer(buffer, DXGI_FORMAT_R32_UINT, 0);
				}
				void DX11IndexBuffer::Update(const void * data, unsigned int size)
				{
					DX11Context::GetContext()->UpdateSubresource(buffer, 0, 0, data, 0, 0);
				}
			}
		}
	}
}

#endif