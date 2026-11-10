#include <MeshPlacementPreview.h>
#include <Assets/AssetLibrary.h>
#include <Graphics/GraphicsModule.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cmath>
#include <limits>
#include <cstring>

namespace Nuclear::Editor
{
	using namespace Diligent;

	void MeshPlacementPreview::Begin(const Core::UUID& mesh)
	{
		if (!mMesh || *mMesh != mesh)
		{
			mMeasured = false;
			mMinimum = mMaximum = Math::Vector3(0);
		}
		mMesh = mesh;
		mValid = false;
	}

	void MeshPlacementPreview::Cancel()
	{
		mMesh.reset();
		mValid = false;
		mMeasured = false;
	}

	void MeshPlacementPreview::MeasureBounds(Assets::Mesh& mesh)
	{
		Math::Vector3 minimum(std::numeric_limits<float>::max());
		Math::Vector3 maximum(std::numeric_limits<float>::lowest());
		bool found = false;
		auto sample = [&](const Assets::Mesh::Vertex& value) {
			const auto position = value.Position;
			if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return;
			minimum = Math::min(minimum, position);
			maximum = Math::max(maximum, position);
			found = true;
		};
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		for (auto& part : mesh.mSubMeshes)
		{
			if (!part.data.Vertices.empty())
			{
				for (const auto& value : part.data.Vertices) sample(value);
				continue;
			}
			if (!part.mVB) continue;
			auto desc = part.mVB->GetDesc();
			desc.Name = "Placement contact bounds";
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			desc.Mode = BUFFER_MODE_UNDEFINED;
			desc.ElementByteStride = 0;
			RefCntAutoPtr<IBuffer> staging;
			Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(desc, nullptr, &staging);
			if (!staging) continue;
			context->CopyBuffer(part.mVB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
				staging, 0, desc.Size, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			context->WaitForIdle();
			MapHelper<Assets::Mesh::Vertex> vertices(context, staging, MAP_READ, MAP_FLAG_NONE);
			if (vertices)
				for (Uint64 i = 0; i < desc.Size / sizeof(Assets::Mesh::Vertex); ++i) sample(vertices[i]);
		}
		if (found)
		{
			mMinimum = minimum;
			mMaximum = maximum;
		}
		mMeasured = true;
	}

	float MeshPlacementPreview::ReadDepth(Components::CameraComponent& camera, UI::Rect viewport, UI::Vec2 point)
	{
		auto* view = camera.GetDepthRT().GetRTV();
		if (!view) return 1;
		auto* source = view->GetTexture();
		auto desc = source->GetDesc();
		if (desc.Format != TEX_FORMAT_D32_FLOAT && desc.Format != TEX_FORMAT_D24_UNORM_S8_UINT) return 1;
		if (!mDepthReadback || mDepthReadback->GetDesc().Width != desc.Width ||
			mDepthReadback->GetDesc().Height != desc.Height || mDepthReadback->GetDesc().Format != desc.Format)
		{
			mDepthReadback.Release();
			desc.Name = "Mesh placement scene depth readback";
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			Graphics::GraphicsModule::Get().GetDevice()->CreateTexture(desc, nullptr, &mDepthReadback);
		}
		if (!mDepthReadback) return 1;
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
		CopyTextureAttribs copy;
		copy.pSrcTexture = source;
		copy.pDstTexture = mDepthReadback;
		copy.SrcTextureTransitionMode = copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		context->CopyTexture(copy);
		context->WaitForIdle();
		MappedTextureSubresource mapped;
		context->MapTextureSubresource(mDepthReadback, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
		if (!mapped.pData) return 1;
		const auto x = std::min(desc.Width - 1, Uint32((point.x - viewport.x) / viewport.Width * desc.Width));
		const auto y = std::min(desc.Height - 1, Uint32((point.y - viewport.y) / viewport.Height * desc.Height));
		auto sample = [&](Uint32 column, Uint32 row) {
			const auto* pixel = static_cast<const char*>(mapped.pData) + row * mapped.Stride + column * 4;
			float value = 1;
			if (desc.Format == TEX_FORMAT_D32_FLOAT) std::memcpy(&value, pixel, sizeof(value));
			else
			{
				Uint32 packed = 0;
				std::memcpy(&packed, pixel, sizeof(packed));
				value = float(packed & 0x00FFFFFF) / float(0x00FFFFFF);
			}
			return value;
		};
		const float depth = sample(x, y);
		if (depth < .999999f && x + 1 < desc.Width && y + 1 < desc.Height)
		{
			const auto inverse = Math::inverse(camera.GetProjectionMatrix() * camera.mCameraData.View);
			auto unproject = [&](Uint32 column, Uint32 row, float value) {
				const auto world = inverse * Math::Vector4(2 * (float(column) + .5f) / desc.Width - 1,
					1 - 2 * (float(row) + .5f) / desc.Height, value, 1);
				return Math::Vector3(world) / world.w;
			};
			const float rightDepth = sample(x + 1, y);
			const float belowDepth = sample(x, y + 1);
			if (rightDepth < .999999f && belowDepth < .999999f &&
				std::abs(rightDepth - depth) < .001f && std::abs(belowDepth - depth) < .001f)
			{
				const auto world = unproject(x, y, depth);
				const auto normal = Math::cross(unproject(x, y + 1, belowDepth) - world,
					unproject(x + 1, y, rightDepth) - world);
				const float length = Math::length(normal);
				if (std::isfinite(length) && length > .0000001f)
				{
					mHitNormal = normal / length;
					if (Math::dot(mHitNormal, camera.GetPosition() - world) < 0) mHitNormal = -mHitNormal;
				}
			}
		}
		context->UnmapTextureSubresource(mDepthReadback, 0, 0);
		return depth;
	}

	bool MeshPlacementPreview::Update(Components::CameraComponent& camera, UI::Rect viewport, UI::Vec2 point)
	{
		mValid = false;
		mHitNormal = {0, 1, 0};
		auto* mesh = mMesh ? Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(*mMesh) : nullptr;
		if (!mesh || mesh->GetState() != Assets::IAsset::State::Created || !viewport.Contains(point)) return false;
		const bool hasGeometry = std::any_of(mesh->mSubMeshes.begin(), mesh->mSubMeshes.end(), [](const auto& part) {
			return part.mVB && part.mIB && part.mIndicesCount;
		});
		if (!hasGeometry) return false;
		if (viewport.Width < 1 || viewport.Height < 1) return false;
		const float x = (point.x - viewport.x) / viewport.Width * 2 - 1;
		const float y = 1 - (point.y - viewport.y) / viewport.Height * 2;
		const auto inverse = Math::inverse(camera.GetProjectionMatrix() * camera.mCameraData.View);
		const auto nearPoint = inverse * Math::Vector4(x, y, 0, 1);
		const auto farPoint = inverse * Math::Vector4(x, y, 1, 1);
		if (std::abs(nearPoint.w) < .000001f || std::abs(farPoint.w) < .000001f) return false;
		const auto origin = Math::Vector3(nearPoint) / nearPoint.w;
		const auto delta = Math::Vector3(farPoint) / farPoint.w - origin;
		if (!std::isfinite(Math::length(delta)) || Math::length(delta) < .000001f) return false;
		const auto direction = Math::normalize(delta);
		if (!mMeasured) MeasureBounds(*mesh);
		const float depth = ReadDepth(camera, viewport, point);
		if (std::isfinite(depth) && depth >= 0 && depth < .999999f)
		{
			const auto world = inverse * Math::Vector4(x, y, depth, 1);
			if (std::abs(world.w) < .000001f) return false;
			mPosition = Math::Vector3(world) / world.w;
		}
		else
		{
			const float distance = std::abs(direction.y) > .00001f ? -origin.y / direction.y : -1;
			mPosition = distance > 0 && distance < 10000 ? origin + direction * distance : origin + direction * 10.f;
		}
		const Math::Vector3 support{
			mHitNormal.x >= 0 ? mMinimum.x : mMaximum.x,
			mHitNormal.y >= 0 ? mMinimum.y : mMaximum.y,
			mHitNormal.z >= 0 ? mMinimum.z : mMaximum.z};
		mPosition -= mHitNormal * Math::dot(support, mHitNormal);
		mValid = std::isfinite(mPosition.x) && std::isfinite(mPosition.y) && std::isfinite(mPosition.z);
		return mValid;
	}

	void MeshPlacementPreview::Render(Components::CameraComponent& camera)
	{
		auto* mesh = mMesh && mValid ? Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(*mMesh) : nullptr;
		if (!mesh || mesh->GetState() != Assets::IAsset::State::Created) return;
		if ((!mPipeline || !mBinding) && !InitializePipeline(camera)) return;
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		context->SetRenderTargets(1, camera.GetColorRT().GetRTVDblPtr(), camera.GetDepthRT().GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const auto size = camera.GetColorRT().GetDimensions();
		Viewport viewport;
		viewport.Width = float(size.x);
		viewport.Height = float(size.y);
		context->SetViewports(1, &viewport, size.x, size.y);
		{
			MapHelper<Constants> constants(context, mConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			constants->ModelViewProjection = camera.GetProjectionMatrix() * camera.mCameraData.View * Math::translate(Math::Matrix4(1), mPosition);
			constants->LightDirection = Math::Vector4(Math::normalize(Math::Vector3(.6f, 1, .8f)), 0);
			constants->BaseColor = {.2f, .9f, .7f, .55f};
		}
		context->SetPipelineState(mPipeline);
		context->CommitShaderResources(mBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		for (auto& part : mesh->mSubMeshes)
		{
			if (!part.mVB || !part.mIB || !part.mIndicesCount) continue;
			IBuffer* buffer = part.mVB;
			const Uint64 offset = 0;
			context->SetVertexBuffers(0, 1, &buffer, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
			context->SetIndexBuffer(part.mIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			DrawIndexedAttribs draw;
			draw.IndexType = VT_UINT32;
			draw.NumIndices = part.mIndicesCount;
			draw.Flags = DRAW_FLAG_VERIFY_ALL;
			context->DrawIndexed(draw);
		}
	}
	static const char* PreviewVS = R"(
cbuffer PreviewConstants
{
    float4x4 ModelViewProjection;
    float4 LightDirection;
    float4 BaseColor;
};

struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Position = mul(ModelViewProjection, float4(input.Position, 1.0));
    output.Normal = input.Normal;
    return output;
}
)";

	static const char* PreviewPS = R"(
cbuffer PreviewConstants
{
    float4x4 ModelViewProjection;
    float4 LightDirection;
    float4 BaseColor;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 N = length(input.Normal) > 0.001 ? normalize(input.Normal) : float3(0.0, 1.0, 0.0);
    float3 L = normalize(LightDirection.xyz);
    float NdotL = max(dot(N, L), 0.0);
    float3 ambient = BaseColor.rgb * 0.35;
    float3 diffuse = BaseColor.rgb * NdotL * 0.65;
    return float4(ambient + diffuse, BaseColor.a);
}
)";

	bool MeshPlacementPreview::InitializePipeline(Components::CameraComponent& camera)
	{
		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		if (!device)
			return false;

		BufferDesc bufferDesc;
		bufferDesc.Name = "Mesh preview constants";
		bufferDesc.Size = sizeof(Constants);
		bufferDesc.Usage = USAGE_DYNAMIC;
		bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
		bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		device->CreateBuffer(bufferDesc, nullptr, &mConstantBuffer);
		if (!mConstantBuffer)
			return false;

		RefCntAutoPtr<IShader> vs;
		RefCntAutoPtr<IShader> ps;
		Graphics::GraphicsModule::Get().CreateShader(PreviewVS, &vs, SHADER_TYPE_VERTEX);
		Graphics::GraphicsModule::Get().CreateShader(PreviewPS, &ps, SHADER_TYPE_PIXEL);
		if (!vs || !ps)
			return false;

		GraphicsPipelineStateCreateInfo psoCI;
		psoCI.PSODesc.Name = "Mesh placement ghost pipeline";
		psoCI.GraphicsPipeline.NumRenderTargets = 1;
		psoCI.GraphicsPipeline.RTVFormats[0] = camera.GetColorRT().GetRTV()->GetTexture()->GetDesc().Format;
		psoCI.GraphicsPipeline.DSVFormat = camera.GetDepthRT().GetRTV()->GetTexture()->GetDesc().Format;
		psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
		psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
		psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
		psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

		const LayoutElement layout[] = {
			{0, 0, 3, VT_FLOAT32, false, offsetof(Assets::Mesh::Vertex, Position), sizeof(Assets::Mesh::Vertex)},
			{1, 0, 3, VT_FLOAT32, false, offsetof(Assets::Mesh::Vertex, Normal), sizeof(Assets::Mesh::Vertex)}};
		psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
		psoCI.GraphicsPipeline.InputLayout.NumElements = 2;
		auto& blend = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
		blend.BlendEnable = true;
		blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
		blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
		psoCI.pVS = vs;
		psoCI.pPS = ps;

		psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
		device->CreateGraphicsPipelineState(psoCI, &mPipeline);
		if (!mPipeline)
			return false;

		auto* cbVarVS = mPipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "PreviewConstants");
		if (cbVarVS)
			cbVarVS->Set(mConstantBuffer);
		auto* cbVarPS = mPipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PreviewConstants");
		if (cbVarPS)
			cbVarPS->Set(mConstantBuffer);

		mPipeline->CreateShaderResourceBinding(&mBinding, true);
		return mBinding != nullptr;
	}

}
