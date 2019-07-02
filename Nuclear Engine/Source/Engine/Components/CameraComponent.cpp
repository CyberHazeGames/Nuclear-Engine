#include "Engine/Components\CameraComponent.h"
#include "Base/Math/gtc/matrix_inverse.hpp"
#include <Core\Engine.h>
#include <Engine/ECS\Scene.h>
#include <Engine\Graphics\Context.h>
#include "Diligent\Graphics\GraphicsEngine\interface\MapHelper.h"
#include <Diligent/Graphics/GraphicsTools/include/CommonlyUsedStates.h>
#include <Engine\Managers\ShaderManager.h>
#include <Core\FileSystem.h>

namespace NuclearEngine
{
	using namespace Math;

	namespace Components
	{
		CameraComponent::CameraComponent()
			: CameraComponent(Math::Vector3(0.0f, 0.0f, 0.0f))
		{
		}
		CameraComponent::CameraComponent(Math::Vector3 __position, Math::Vector3 _Worldup, float yaw, float pitch, float speed, float sensitivity, float _Zoom)
			: Front(Math::Vector3(0.0f, 0.0f, -1.0f)), MovementSpeed(speed), MouseSensitivity(sensitivity), Yaw(yaw), Pitch(pitch), WorldUp(_Worldup), Zoom(_Zoom)
		{
			
			position = __position;

			BufferDesc CBDesc;
			CBDesc.Name = "CameraCB";
			CBDesc.uiSizeInBytes = sizeof(_CameraBuffer);
			CBDesc.Usage = USAGE_DYNAMIC;
			CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
			CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
			Graphics::Context::GetDevice()->CreateBuffer(CBDesc, &BufferData(), &ConstantBuffer);
		}
	
		CameraComponent::~CameraComponent()
		{
		}
	
		void CameraComponent::Initialize(Math::Matrix4 projectionMatrix)
		{
			_CameraBuffer.Projection = projectionMatrix;
			Update();
		}

		void CameraComponent::Bake(const CameraBakingOptions& Opt)
		{
			BakeRenderTarget(Opt);
			BakePipeline(Opt);
		}

		void CameraComponent::Update()
		{
			Vector3 front;
			front.x = cos(radians(Yaw)) * cos(radians(Pitch));
			front.y = sin(radians(Pitch));
			front.z = sin(radians(Yaw)) * cos(radians(Pitch));
			Front = normalize(front);
			// Also re-calculate the Right and Up vector
			Right = normalize(Math::cross(Front, WorldUp));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
			Up = normalize(Math::cross(Right, Front));

			_CameraBuffer.View = Math::lookAt(position, position + Front, Up);

			UpdateMatricesOnly();

			Diligent::MapHelper<CameraBuffer> CBConstants(Graphics::Context::GetContext(), ConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			*CBConstants = _CameraBuffer;
		}
		void CameraComponent::UpdateMatricesOnly()
		{
			_CameraBuffer.ModelViewProjection = _CameraBuffer.Projection * _CameraBuffer.View * _CameraBuffer.Model;
			_CameraBuffer.ModelInvTranspose = Math::inverseTranspose(_CameraBuffer.Model);
		}
		void CameraComponent::SetModelMatrix(Math::Matrix4 modelMatrix)
		{
			_CameraBuffer.Model = modelMatrix;
			Update();
		}
		void CameraComponent::SetViewMatrix(Math::Matrix4 viewMatrix)
		{
			_CameraBuffer.View = viewMatrix;
			Update();

		}
		void CameraComponent::SetProjectionMatrix(Math::Matrix4 projectionMatrix)
		{
			_CameraBuffer.Projection = projectionMatrix;
			Update();

		}
		void CameraComponent::SetPosition(Math::Vector3 cameraposition)
		{
			this->position = cameraposition;
		}
		void CameraComponent::SetActive()
		{
			//Ugly hack prone to errors dont look
			Core::Engine::GetGame()->GetActiveScene()->SetActiveCamera(this);
		}
		Math::Matrix4 CameraComponent::GetModelMatrix()
		{
			return _CameraBuffer.Model;
		}
		Math::Matrix4 CameraComponent::GetViewMatrix()
		{
			return this->_CameraBuffer.Model;
		}
		Math::Matrix4 CameraComponent::GetProjectionMatrix()
		{
			return _CameraBuffer.Projection;
		}
		Math::Vector3 CameraComponent::GetPosition()
		{
			return position;
		}
		IBuffer* CameraComponent::GetCBuffer()
		{
			return ConstantBuffer;
		}

		Graphics::RenderTarget* CameraComponent::GetCameraRT()
		{
			return &CameraRT;
		}

		IPipelineState* CameraComponent::GetPipeline()
		{
			return mPSO.RawPtr();
		}

		void CameraComponent::BakeRenderTarget(const CameraBakingOptions& Desc)
		{
			Graphics::RenderTargetDesc RTDesc;
			RTDesc.Width = Desc.RTWidth;
			RTDesc.Height = Desc.RTHeight;

			if (Desc.HDR == true)
				RTDesc.ColorTexFormat = TEX_FORMAT_RGBA16_FLOAT;
			else
				RTDesc.ColorTexFormat = TEX_FORMAT_RGBA8_UNORM;


			CameraRT.Create(RTDesc);
		}

		void CameraComponent::BakePipeline(const CameraBakingOptions& Desc)
		{
			Managers::AutoVertexShaderDesc VertShaderDesc;
			VertShaderDesc.Use_Camera = false;
			VertShaderDesc.InNormals = false;
			VertShaderDesc.OutFragPos = false;
			VertShaderDesc.Name = "CameraPP_VS";

			std::vector<LayoutElement> LayoutElems;
			RefCntAutoPtr<IShader> VShader;
			RefCntAutoPtr<IShader> PShader;


			VShader = Graphics::GraphicsEngine::GetShaderManager()->CreateAutoVertexShader(VertShaderDesc, &LayoutElems);

			ShaderCreateInfo CreationAttribs;
			CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
			CreationAttribs.UseCombinedTextureSamplers = true;
			CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
			CreationAttribs.Desc.Name = "CameraPP_PS";

			std::vector<std::string> defines;

			if (Desc.GammaCorrection == true) { defines.push_back("NE_GAMMA_CORRECTION_ENABLED"); }
			if (Desc.HDR == true) { defines.push_back("NE_HDR_ENABLED"); }

			CreationAttribs.Source = Core::FileSystem::LoadShader("Assets/NuclearEngine/Shaders/PostProcessing.ps.hlsl", defines, std::vector<std::string>(), true).c_str();
			Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &PShader);

			PipelineStateDesc PSODesc;

			PSODesc.Name = "CameraPP_PSO";
			PSODesc.IsComputePipeline = false;
			PSODesc.GraphicsPipeline.NumRenderTargets = 1;
			PSODesc.GraphicsPipeline.RTVFormats[0] = Graphics::Context::GetSwapChain()->GetDesc().ColorBufferFormat;
			PSODesc.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSODesc.GraphicsPipeline.DSVFormat = Graphics::Context::GetSwapChain()->GetDesc().DepthBufferFormat;
			PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			PSODesc.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
			PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
			PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
			PSODesc.GraphicsPipeline.pVS = VShader;
			PSODesc.GraphicsPipeline.pPS = PShader;
			PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
			PSODesc.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();
			PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

			std::vector<ShaderResourceVariableDesc> Vars;
			Vars.push_back({ SHADER_TYPE_PIXEL, "ScreenTex", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE });

			std::vector<StaticSamplerDesc> StaticSamplers;
			StaticSamplers.push_back({ SHADER_TYPE_PIXEL, "ScreenTex", Sam_LinearClamp });

			Graphics::GraphicsEngine::GetShaderManager()->ProcessAndCreatePipeline(&mPSO, PSODesc, Vars, false, StaticSamplers);
		}

		void CameraComponent::ProcessEye(float xoffset, float yoffset, bool constrainPitch)
		{
			xoffset *= MouseSensitivity;
			yoffset *= MouseSensitivity;

			Yaw += xoffset;
			Pitch += yoffset;

			// Make sure that when pitch is out of bounds, screen doesn't get flipped
			if (constrainPitch)
			{
				if (Pitch > 89.0f)
					Pitch = 89.0f;
				if (Pitch < -89.0f)
					Pitch = -89.0f;
			}
		}
		void CameraComponent::ProcessMovement(Camera_Movement direction, float deltaTime)
		{
			float velocity = MovementSpeed * deltaTime;
			if (direction == FORWARD)
				position += Front * velocity;
			if (direction == BACKWARD)
				position -= Front * velocity;
			if (direction == LEFT)
				position -= Right * velocity;
			if (direction == RIGHT)
				position += Right * velocity;
		}
		void CameraComponent::ProcessZoom(float yoffset)
		{
			if (Zoom >= 1.0f && Zoom <= 45.0f)
				Zoom -= yoffset;
			if (Zoom <= 1.0f)
				Zoom = 1.0f;
			if (Zoom >= 45.0f)
				Zoom = 45.0f;
		}
		Math::Vector3 CameraComponent::GetFrontView()
		{
			return Front;
		}
	}
}