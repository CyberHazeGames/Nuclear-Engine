﻿#include "Common.h"
#include "SampleSelector.h"
#include <iostream>
#include "Engine\Scripting\ScriptingEngine.h"

#pragma comment(lib,"Nuclear.Core.lib")
#pragma comment(lib,"glfw3.lib")

RENDER_DEVICE_TYPE SelectRenderer()
{
	std::cout << "Select Renderer: \n"
		<< "1) DirectX 11 \n"
		<< "2) DirectX 12 \n"
		<< "3) OpenGL 4 \n"
		<< "4) Vulkan \n";

	int i;
	std::cin >> i;

	switch (i)
	{
	case 1:
		return RENDER_DEVICE_TYPE_D3D11;
	case 2:
		return RENDER_DEVICE_TYPE_D3D12;
	case 3:
		return RENDER_DEVICE_TYPE_GL;
	case 4:
		return RENDER_DEVICE_TYPE_VULKAN;
	}

	return RENDER_DEVICE_TYPE_D3D11;
}

int main(int argc, char* argv[])
{
	EngineStartupDesc desc;

	/*std::cout << "Auto Initialize? \n"
		<< "1) Yes \n"
		<< "2) No \n";
	*/
//	int i;
//	std::cin >> i;
//	if (i == 2)
	//{
	//	desc.Renderer = SelectRenderer();
	//	Engine::GetInstance()->Start(desc);

	//}
	//else
	//{
	//	EngineStartupDesc desc;
	//	Engine::GetInstance()->Start(desc);
	//}

	//EngineStartupDesc desc;
	desc.mEngineWindowDesc.WindowWidth = 800;
	desc.mEngineWindowDesc.WindowHeight = 600;

	Engine::GetInstance()->Start(desc);
	Core::Path::mReservedPaths["@CommonAssets@"] = "Assets/Common";
	Core::Path::mReservedPaths["@NuclearAssets@"] = "Assets/NuclearEngine";

	while (!Engine::GetInstance()->ShouldClose())
	{
		SampleSelector selector;
		Engine::GetInstance()->LoadClient(&selector);
	}
	return 0;
}