#include <NuclearEditor.h>
NuclearEditor::NuclearEditor()
	: Client({"Nuclear Editor", "0.02", "Nuclear Engine", Core::ClientType::Editor}), GUI(mActiveProject)
{
}
void NuclearEditor::Load()
{
	GUI.Initialize();
}
void NuclearEditor::Update(float deltaTime)
{
	GUI.Update(deltaTime);
}
void NuclearEditor::Render(float deltaTime)
{
	GUI.Render(deltaTime);
}
void NuclearEditor::OnWindowResize(int width, int height)
{
	Graphics::GraphicsModule::Get().ResizeSwapChain(width, height);
	GUI.Resize(width, height);
}
void NuclearEditor::Shutdown()
{
	GUI.Shutdown();
}

void NuclearEditor::OnAuxiliaryWindowEvent(const Platform::Event& event)
{
	GUI.OnAuxiliaryWindowEvent(event);
}
