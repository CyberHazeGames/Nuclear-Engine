#pragma once
#include <NE_Common.h>
#include <Components\GenericCamera.h>
#include <vector>

namespace NuclearEngine {


	namespace Components 
	{
		struct DirectionalLight;
		struct PointLight;
		struct SpotLight;
	}

	namespace Core {

		enum class NEAPI LightShading
		{
			PhongShading
		};

		enum class NEAPI LightEffects
		{
			None
		};

		enum class NEAPI RenderingTechnique
		{
			Forward
		};

		enum class Renderer3DStatusFlag
		{
			Ready,
			RequireBaking
		};

		struct NEAPI Renderer3D_Desc
		{
			//Light
			LightShading lightmodel;
			LightEffects effect;
			RenderingTechnique tech;
		};


		class NEAPI Renderer3D
		{
		public:
			Renderer3D(const Renderer3D_Desc& desc, Components::GenericCamera *cam);

			void AddLight(Components::DirectionalLight *light);
			void AddLight(Components::PointLight *light);
			void AddLight(Components::SpotLight *light);

			API::Shader *GetShader();

			void SetCamera(Components::GenericCamera *cam);

			void Bake();

			void Render();
		private:
			API::Shader *m_shader;
			API::UniformBuffer *m_lightubo;

			std::vector<Components::DirectionalLight*> dirLights;
			std::vector<Components::PointLight*> pointLights;
			std::vector<Components::SpotLight*> spotLights;
			Renderer3DStatusFlag flag;
			size_t lightubosize;

			Renderer3D_Desc m_desc;
			Components::GenericCamera *m_cam;

			bool initialized;
		};
	}
}