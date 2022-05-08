#pragma once
#include <Engine/Rendering/PBRCapture.h>
#include <Engine/Graphics/RenderTarget.h>
#include <Engine/Graphics/Texture.h>
#include <Diligent/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Engine/Rendering/Skybox.h>

namespace Nuclear
{
	namespace Rendering
	{
		struct ImageBasedLightingDesc
		{
			TEXTURE_FORMAT mIrradianceCubeFmt = TEX_FORMAT_RGBA32_FLOAT;
			TEXTURE_FORMAT mPrefilteredEnvMapFmt = TEX_FORMAT_RGBA16_FLOAT;
			Uint32         mIrradianceCubeDim = 32;
			Uint32         mPrefilteredEnvMapDim = 128;
			Uint32         mBRDF_LUTDim = 512;
		};

		class NEAPI ImageBasedLighting
		{
		public:
			void Initialize(const ImageBasedLightingDesc& desc);

			Rendering::PBRCapture EquirectangularToCubemap(Graphics::Texture* Tex);

			void SetEnvironmentCapture(Rendering::PBRCapture* cap);
			Rendering::PBRCapture* GetEnvironmentCapture();

			Assets::Image* GetBRDF_LUT();
		protected:
			RefCntAutoPtr<IPipelineState> pERectToCubemap_PSO;
			RefCntAutoPtr<IShaderResourceBinding> pERectToCubemap_SRB;

			RefCntAutoPtr<IPipelineState> pPrecomputeIrradiancePSO;
			RefCntAutoPtr<IShaderResourceBinding> pPrecomputeIrradiance_SRB;

			RefCntAutoPtr<IPipelineState> pPrecomputePrefilterPSO;
			RefCntAutoPtr<IShaderResourceBinding> pPrecomputePrefilter_SRB;

			RefCntAutoPtr<IPipelineState> pPrecomputeBRDF_PSO;
			RefCntAutoPtr<IShaderResourceBinding> pPrecomputeBRDF_SRB;

			RefCntAutoPtr<IBuffer> pCaptureCB;
			RefCntAutoPtr<IBuffer> pPrefilterRoughnessCB;

			Assets::Image mBRDF_LUT_Image;

			Rendering::PBRCapture* pEnvCapture;

			Skybox mSkybox;

			Graphics::RenderTarget mCaptureDepthRT;

			ImageBasedLightingDesc mDesc;
			Math::Matrix4 mCaptureProjection;
			Math::Matrix4 mCaptureViews[6];
		};
	}
}