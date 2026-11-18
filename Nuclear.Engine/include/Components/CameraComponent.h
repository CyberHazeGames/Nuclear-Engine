#pragma once
#include <NE_Common.h>
#include <Components\ShaderStructs.h>
#include <Graphics\Color.h>
#include <Graphics/RenderTarget.h>

namespace Nuclear
{
	namespace Components
	{
		enum CAMERA_MOVEMENT {
			CAMERA_MOVEMENT_FORWARD,
			CAMERA_MOVEMENT_BACKWARD,
			CAMERA_MOVEMENT_LEFT,
			CAMERA_MOVEMENT_RIGHT
		};

		// Default camera values
		const float YAW = -90.0f;
		const float PITCH = 0.0f;
		const float SPEED = 2.5f;
		const float SENSITIVTY = 0.05f;
		const float ZOOM = 45.0f;


		struct CameraComponentDesc
		{
			Math::Matrix4 mProjection;
			float mYaw = YAW;
			float mPitch = PITCH;
			float mSpeed = SPEED;
			float mSensitivity = SENSITIVTY;
			float mZoom = ZOOM;

			bool mIsActiveCamera = true;
			bool mInitRTs = true;
			Graphics::RenderTargetDesc mRTDesc;
			Graphics::RenderTargetDesc mDepthRTDesc;
		};

		class NEAPI CameraComponent
		{
		public:
			CameraComponent();
			CameraComponent(const CameraComponentDesc& desc);
			~CameraComponent();

			void ProcessEye(float xoffset, float yoffset, bool constrainPitch = true);
			void ProcessMovement(CAMERA_MOVEMENT direction, float deltaTime);
			void ProcessZoom(float yoffset);

			const Math::Vector3 GetFrontView() const
			{
				return Front;
			}
			void UpdateBuffer();

			FORCE_INLINE const Math::Matrix4 GetModelMatrix() const
			{
				return mCameraData.Model;
			}
			FORCE_INLINE const Math::Matrix4 GetViewMatrix() const
			{
				return mCameraData.Model;
			}
			FORCE_INLINE const Math::Matrix4 GetProjectionMatrix() const
			{
				return mCameraData.Projection;
			}


			FORCE_INLINE Graphics::RenderTarget& GetColorRT()
			{
				return mRT;
			}

			FORCE_INLINE Graphics::RenderTarget& GetDepthRT()
			{
				return mDepthRT;
			}


			void SetModelMatrix(Math::Matrix4 modelMatrix);
			void SetViewMatrix(Math::Matrix4 viewMatrix);
			void SetProjectionMatrix(Math::Matrix4 projectionMatrix);

			void SetPosition(Math::Vector3 cameraposition);
			/// Applies the authored local-to-world orientation (identity looks down -Z) to the camera.
			void SetOrientation(const Math::Quaternion& rotation);
			FORCE_INLINE const Math::Vector3 GetPosition() const
			{
				return Position;
			}

			void ResizeRTs(Uint32 RTWidth, Uint32 RTHeight);

			void SetIsActive(bool value);
			bool GetIsActive();

			void CreateRenderTargets(const Graphics::RenderTargetDesc& ColorRTDesc, const Graphics::RenderTargetDesc& DepthRTDesc);

			// Camera options
			float MovementSpeed;
			float MouseSensitivity;
			float Zoom;

			CameraBuffer mCameraData;

			Graphics::Color mRTClearColor;
		protected:
			bool mIsActive;

			// Eular Angles
			float Yaw;
			float Pitch;

			Math::Vector3 Front, Up, Right;
			Math::Vector3 WorldUp = Math::Vector3(0.0f, 1.0f, 0.0f);

			Math::Vector3 Position, direction;

			Graphics::RenderTarget mRT;
			Graphics::RenderTarget mDepthRT;
		};
	}
}