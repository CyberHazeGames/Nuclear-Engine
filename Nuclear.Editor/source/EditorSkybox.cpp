#include <EditorSkybox.h>
#include <Components/CameraComponent.h>
#include <Graphics/GraphicsModule.h>
#include <Rendering/RenderingModule.h>
#include <Math/Math.h>
#include <array>
#include <vector>
#include <cmath>

namespace Nuclear::Editor
{
	bool EditorSkybox::Initialize(Components::CameraComponent& camera)
	{
		constexpr Uint32 size = 32;
		std::vector<Math::Vector4> pixels(size * size * 6);
		std::array<Diligent::TextureSubResData, 6> faces;
		for (Uint32 face = 0; face < 6; ++face)
		{
			for (Uint32 y = 0; y < size; ++y)
				for (Uint32 x = 0; x < size; ++x)
				{
					const float u = 2.f * (float(x) + .5f) / size - 1.f;
					const float v = 2.f * (float(y) + .5f) / size - 1.f;
					const Math::Vector3 directions[] = {{1, -v, -u}, {-1, -v, u}, {u, 1, v},
						{u, -1, -v}, {u, -v, 1}, {-u, -v, -1}};
					const float height = Math::normalize(directions[face]).y;
					const Math::Vector3 horizon(.48f, .66f, .86f);
					const Math::Vector3 pole = height >= 0 ? Math::Vector3(.06f, .22f, .52f)
						: Math::Vector3(.12f, .14f, .18f);
					const auto color = Math::mix(horizon, pole, std::pow(std::abs(height), .55f));
					pixels[face * size * size + y * size + x] = Math::Vector4(color, 1.f);
				}
			faces[face].pData = pixels.data() + face * size * size;
			faces[face].Stride = size * sizeof(Math::Vector4);
		}
		Diligent::TextureDesc desc;
		desc.Name = "Editor default sky cubemap";
		desc.Type = Diligent::RESOURCE_DIM_TEX_CUBE;
		desc.Width = desc.Height = size;
		desc.ArraySize = 6;
		desc.MipLevels = 1;
		desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
		desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
		desc.Usage = Diligent::USAGE_IMMUTABLE;
		Diligent::TextureData data;
		data.pSubResources = faces.data();
		data.NumSubresources = 6;
		Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
		Graphics::GraphicsModule::Get().GetDevice()->CreateTexture(desc, &data, &texture);
		if (!texture)
			return false;
		mTexture.SetTextureView(texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
		mTexture.SetState(Assets::IAsset::State::Created);
		return mSkybox.Initialize(&mTexture, camera.GetColorRT().GetRTV()->GetTexture()->GetDesc().Format,
			camera.GetDepthRT().GetRTV()->GetTexture()->GetDesc().Format);
	}

	void EditorSkybox::Render(Components::CameraComponent& camera)
	{
		Rendering::RenderingModule::Get().UpdateCameraCB(&camera);
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		context->SetRenderTargets(1, camera.GetColorRT().GetRTVDblPtr(), camera.GetDepthRT().GetRTV(),
			Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		mSkybox.Render();
	}
}
