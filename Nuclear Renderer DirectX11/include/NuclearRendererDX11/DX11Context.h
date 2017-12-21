#include <NuclearRendererBase\NRBContext.h>
#include <NuclearRendererDX11\DX11Common.h>

namespace NuclearRenderer
{
	class NRDX11API DX11Context : public NRBContext
	{
	public:
		bool Initialize() override;
		void SetPrimitiveType(int primitivetype) override;
		void ClearColor(float Red, float Green, float Blue, float Alpha) override;
		void ClearDepthBuffer() override;
		void ClearStencilBuffer() override;

		void EnableDepthBuffer(bool state) override;
		
		void SwapBuffers() override;
		void Shutdown() override;

		void Draw(unsigned int count) override;
		void DrawIndexed(unsigned int vertexCount) override;

		void SetViewPort(int x, int y, int width, int height) override;

		NRBVertexBuffer* ConstructVertexBuffer(NRBVertexBuffer* param) override;
		NRBIndexBuffer* ConstructIndexBuffer(NRBIndexBuffer* param) override;
		NRBConstantBuffer* ConstructConstantBuffer(NRBConstantBuffer* param) override;
		NRBRenderTarget* ConstructRenderTarget(NRBRenderTarget* param) override;
		NRBDepthStencilState* ConstructDepthStencilState(NRBDepthStencilState* param) override;
		NRBTexture* ConstructTexture(NRBTexture* param) override;
		NRBShader* ConstructShader(NRBShader* param) override;

		// Implementation Specific
		static ID3D11Device1* GetDevice();
		static ID3D11DeviceContext1* GetContext();
	};
}