#include "UISample.h"
#include "UIShowcaseWidget.h"
#include <Core/Engine.h>
#include <Graphics/GraphicsModule.h>
#include <Platform/Input.h>
#include <UI/UIModule.h>

void UISample::Load()
{
	using namespace Nuclear;
	Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
	auto& context = UI::UIModule::Get().GetContext();
	context.AddToViewport(std::make_unique<UIShowcaseWidget>());
	context.SetInputMode(UI::InputMode::UIOnly);
}

void UISample::Render(float)
{
	using namespace Nuclear;
	auto& graphics = Graphics::GraphicsModule::Get();
	auto* target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
	graphics.GetContext()->SetRenderTargets(1, &target, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	const float clear[] = {.015f, .025f, .045f, 1};
	graphics.GetContext()->ClearRenderTarget(target, clear, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void UISample::OnWindowResize(int width, int height)
{
	Nuclear::Graphics::GraphicsModule::Get().ResizeSwapChain(width, height);
}
