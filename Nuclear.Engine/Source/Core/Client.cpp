#include <Core/Client.h>
#include <Assets\Scene.h>

namespace Nuclear {
	namespace Core {
		Client::Client(const ClientInfo& yourinfo)
		{
			minfo = yourinfo;
		}
		Client::~Client()
		{
		}
		ClientInfo Client::GetClientInfo()
		{
			return minfo;
		}
		void Client::Initialize()
		{
		}

		void Client::Load()
		{
		}
		void Client::Update(float Deltatime)
		{
		}

		void Client::Render(float Deltatime)
		{
		}
		void Client::ExitRendering()
		{
		}
		void Client::Shutdown()
		{
		}
		void Client::LoadFinishUp()
		{
		}
		void Client::OnMouseScroll(double val1, double val2)
		{
		}
		void Client::OnMouseMovement(int xpos, int ypos)
		{
		}

		void Client::OnWindowResize(int width, int height)
		{
		}

		//void Client::SetActiveScene(ECS::Scene* scene)
		//{
		//	pScene = scene;
		//}

		//ECS::Scene* Client::GetActiveScene()
		//{
		//	return pScene;
		//}
	}
}