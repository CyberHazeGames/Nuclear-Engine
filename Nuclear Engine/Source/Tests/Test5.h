#pragma once
#include "TestCommon.h"
#include <iostream>

class Test5 : public Core::Game
{
protected:
	API::VertexShader VShader;
	API::PixelShader PShader;

	API::VertexShader ScreenVShader;
	API::PixelShader ScreenPShader;

	API::VertexBuffer CubeVB;
	API::VertexBuffer PlaneVB;

	XAsset::ModelAsset ScreenQuad;

	API::Sampler LinearSampler;
	API::Texture PlaneTex;
	API::Texture CubeTex;

	API::Sampler ScreenSampler;
	API::Texture ScreenTex;

	API::RenderTarget RT;

	Components::FlyCamera Camera;

	//Default states
	API::CommonStates states;

	float lastX = _Width_ / 2.0f;
	float lastY = _Height_ / 2.0f;
	bool firstMouse = true;

	std::string VertexShader = R"(struct VertexInputType
{
    float4 position : POSITION;
	float2 tex : TEXCOORD;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
	float2 tex : TEXCOORD;
};

cbuffer NE_Camera : register(b0)
{
    matrix Model;
    matrix ModelInvTranspose;
    matrix ModelViewProjection;
    matrix View;
    matrix Projection;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
	
	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(ModelViewProjection, input.position);
	
	// Store the input texture for the pixel shader to use.
    output.tex = input.tex;
    return output;
})";

	std::string PixelShader = R"(
struct PixelInputType
{
    float4 position : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D NE_Tex_Diffuse : register(t0);
SamplerState NE_Tex_Diffuse_Sampler : register(s0);

float4 main(PixelInputType input) : SV_TARGET
{
    return NE_Tex_Diffuse.Sample(NE_Tex_Diffuse_Sampler, input.tex);
}
)";
	std::string ScreenVertexShader = R"(struct VertexInputType
{
    float4 position : POSITION;
	float2 tex : TEXCOORD;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
	float2 tex : TEXCOORD;
};


PixelInputType main(VertexInputType input)
{
    PixelInputType output;
	
	// CalcSulate the position of the vertex against the world, view, and projection matrices.
	output.position = float4(input.position.x,input.position.y,0.0f,1.0f);
	// Store the input texture for the pixel shader to use.
    output.tex = input.tex;
    return output;
})";

	std::string ScreenPixelShader = R"(
struct PixelInputType
{
    float4 position : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D ScreenTexture : register(t0);
SamplerState ScreenTexture_Sampler : register(s0);

float4 main(PixelInputType input) : SV_TARGET
{
    return float4(ScreenTexture.Sample(ScreenTexture_Sampler, input.tex).rgb, 1.0f);
}
)";

	API::RasterizerState R_State;
public:
	Test5()
	{
	}
	void Load()
	{
		API::VertexShader::Create(
			&VShader,
			&API::CompileShader(VertexShader,
				API::ShaderType::Vertex));

		API::PixelShader::Create(
			&PShader,
			&API::CompileShader(PixelShader,
				API::ShaderType::Pixel));

		API::VertexShader::Create(
			&ScreenVShader,
			&API::CompileShader(ScreenVertexShader,
				API::ShaderType::Vertex));

		API::PixelShader::Create(
			&ScreenPShader,
			&API::CompileShader(ScreenPixelShader,
				API::ShaderType::Pixel));
	
		float cubevertices[] = {
			-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
			0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
			-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

			-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
			0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
			0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

			-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
			-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
		};
		float planeVertices[] = {
			// positions          // texture Coords (note we set these higher than 1 (together with GL_REPEAT as texture wrapping mode). this will cause the floor texture to repeat)
			5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
			-5.0f, -0.5f,  5.0f,  0.0f, 0.0f,
			-5.0f, -0.5f, -5.0f,  0.0f, 2.0f,

			5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
			-5.0f, -0.5f, -5.0f,  0.0f, 2.0f,
			5.0f, -0.5f, -5.0f,  2.0f, 2.0f
		};	
		
		int windowwidth, windowheight;
		Core::Application::GetSize(windowwidth, windowheight);



		API::VertexBufferDesc vDesc;
		vDesc.data = cubevertices;
		vDesc.size = sizeof(cubevertices);
		vDesc.usage = API::BufferUsage::Static;
		API::VertexBuffer::Create(&CubeVB, vDesc);

		vDesc.data = planeVertices;
		vDesc.size = sizeof(planeVertices);
		API::VertexBuffer::Create(&PlaneVB, vDesc);



		API::InputLayout ShaderIL;
		ShaderIL.AppendAttribute("POSITION", 0, API::DataType::Float3);
		ShaderIL.AppendAttribute("TEXCOORD", 0, API::DataType::Float2);

		CubeVB.SetInputLayout(&ShaderIL, &VShader);
		PlaneVB.SetInputLayout(&ShaderIL, &VShader);

		XAsset::ModelAsset::CreateScreenQuad(&ScreenQuad);
		ScreenQuad.Initialize(&ScreenVShader);
		
		//Create sampler
		API::SamplerDesc Samplerdesc;
		Samplerdesc.Filter = API::TextureFilter::Point2D;
		API::Sampler::Create(&ScreenSampler, Samplerdesc);
		
		API::Texture_Desc ScreenTexDesc;
		ScreenTexDesc.Format = API::Format::R8G8B8_UNORM;
		ScreenTexDesc.Type = API::TextureType::Texture2D;
		ScreenTexDesc.GenerateMipMaps = false;

		API::Texture_Data Data;
		Data.Img_Data_Buf = NULL;
		Data.Width = windowwidth;
		Data.Height = windowheight;
		API::Texture::Create(&ScreenTex, Data, ScreenTexDesc);
		
		//RT
		API::RenderTarget::Create(&RT);
		RT.AttachTexture(&ScreenTex);
		RT.AttachDepthStencilBuffer(Math::Vector2ui(windowwidth, windowheight));

		Camera.Initialize(Math::Perspective(Math::radians(45.0f), Core::Application::GetAspectRatiof(), 0.1f, 100.0f));

		VShader.SetConstantBuffer(&Camera.GetCBuffer());

		API::Texture_Desc Desc;
		Desc.Format = API::Format::R8G8B8A8_UNORM;
		Desc.Type = API::TextureType::Texture2D;

		Managers::AssetManager::CreateTextureFromFile("Assets/Common/Textures/woodenbox.jpg", &PlaneTex, Desc);
		Managers::AssetManager::CreateTextureFromFile("Assets/Common/Textures/crate_diffuse.png", &CubeTex, Desc);

		//Create sampler
		Samplerdesc.Filter = API::TextureFilter::Trilinear;
		API::Sampler::Create(&LinearSampler, Samplerdesc);
	
		API::RasterizerStateDesc rasterizerdesc;
		rasterizerdesc.FillMode = API::FillMode::Wireframe;

		API::RasterizerState::Create(&R_State, rasterizerdesc);

		Core::Context::SetPrimitiveType(PrimitiveType::TriangleList);
		states.EnabledDepth_DisabledStencil.Bind();

		Core::Application::SetMouseInputMode(Core::MouseInputMode::Virtual);
		Core::Application::Display();
	}

	void OnMouseMovement(double xpos_a, double ypos_a) override
	{
		float xpos = static_cast<float>(xpos_a);
		float ypos = static_cast<float>(ypos_a);

		if (firstMouse)
		{
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		float xoffset = xpos - lastX;
		float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

		lastX = xpos;
		lastY = ypos;

		Camera.ProcessEye(xoffset, yoffset);
	}

	void Update(float deltatime) override
	{
		if (Platform::Input::Keyboard::IsKeyPressed(Platform::Input::Keyboard::Key::W))
			Camera.ProcessMovement(Components::Camera_Movement::FORWARD, deltatime);
		if (Platform::Input::Keyboard::IsKeyPressed(Platform::Input::Keyboard::Key::A))
			Camera.ProcessMovement(Components::Camera_Movement::LEFT, deltatime);
		if (Platform::Input::Keyboard::IsKeyPressed(Platform::Input::Keyboard::Key::S))
			Camera.ProcessMovement(Components::Camera_Movement::BACKWARD, deltatime);
		if (Platform::Input::Keyboard::IsKeyPressed(Platform::Input::Keyboard::Key::D))
			Camera.ProcessMovement(Components::Camera_Movement::RIGHT, deltatime);

		Camera.Update();

	}

	void mRenderscene()
	{
	
		VShader.Bind();
		PShader.Bind();
		LinearSampler.PSBind(0);
		CubeTex.PSBind(0);

		CubeVB.Bind();

		// cube 1
		Math::Matrix4 CubeModel(1.0f);
		CubeModel = Math::Translate(CubeModel, Math::Vector3(-1.0f, 0.0f, -1.0f));
		Camera.SetModelMatrix(CubeModel);
		Core::Context::Draw(36);
		// cube 2
		CubeModel = Math::Matrix4(1.0f);
		CubeModel = Math::Translate(CubeModel, Math::Vector3(2.0f, 0.0f, 0.0f));
		Camera.SetModelMatrix(CubeModel);
		Core::Context::Draw(36);

		// floor
		PlaneTex.PSBind(0);

		PlaneVB.Bind();
		Camera.SetModelMatrix(Math::Matrix4());
		Core::Context::Draw(6);
	}

	/*
	Controls:
	1 - Enable Rasterizer State
	2 - Restore Default Rasterizer State

	W - Move Camera Forward
	A - Move Camera Left
	S - Move Camera Backward
	D - Move Camera Right

	Mouse - Make Camera look around
	*/
	
	void Render(float) override
	{


		//Bind The RenderTarget
		RT.Bind();
		Core::Context::Clear(API::Color(0.0f, 0.0f, 1.0f, 1.0f), ClearColorBuffer | ClearDepthBuffer);

		//Enable Depth Test
		states.EnabledDepth_DisabledStencil.Bind();

		//Render to render-target
		mRenderscene();

		//Bind default RenderTarget
		RT.Bind_Default();

		Core::Context::Clear(API::Color(1.0f, 1.0f, 1.0f, 1.0f), ClearColorBuffer | ClearDepthBuffer);

		//Render RT Texture (color buffer) content
		states.DisabledDepthStencil.Bind();

		if (Platform::Input::Keyboard::IsKeyPressed(Platform::Input::Keyboard::Key::Num1))
		{
			R_State.Bind();
		}
		else if (Platform::Input::Keyboard::IsKeyPressed(Platform::Input::Keyboard::Key::Num2))
		{
			states.DefaultRasterizer.Bind();
		}

		ScreenVShader.Bind();
		ScreenPShader.Bind();
		ScreenSampler.PSBind(0);
		ScreenTex.PSBind(0);
		ScreenQuad.Meshes.at(0).VBO.Bind();
		ScreenQuad.Meshes.at(0).IBO.Bind();
		Core::Context::DrawIndexed(ScreenQuad.Meshes.at(0).IndicesCount);
		//ScreenQuad.Render();
		PlaneTex.PSBind(0);

		Core::Context::PresentFrame();

	}
	void Shutdown() override
	{
		API::VertexShader::Delete(&VShader);
		API::PixelShader::Delete(&PShader);
		API::VertexShader::Delete(&ScreenVShader);
		API::PixelShader::Delete(&ScreenPShader);
		API::VertexBuffer::Delete(&CubeVB);
		API::VertexBuffer::Delete(&PlaneVB);
		API::Sampler::Delete(&ScreenSampler);
		API::RenderTarget::Delete(&RT);
		API::Texture::Delete(&CubeTex);
	}
};
