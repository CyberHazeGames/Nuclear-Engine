#pragma once
#include <Engine/Assets/Common.h>
#include <Engine/Assets/MaterialTypes.h>
#include <Engine\Rendering\ShaderPipeline.h>

namespace Nuclear
{
    namespace Assets
    {
        enum class ShaderType
        {
            _3DRendering
        };

        struct ShaderBuildDesc 
        {
            GraphicsPipelineStateCreateInfo mMainPSOCreateInfo;
            GraphicsPipelineStateCreateInfo mGBufferPSOCreateInfo;

            bool mSupportDefferedRendering = false;
            bool mSupportSkinnedMeshes = false;
            bool mSupportShadows = false;
        };

        struct ShaderShadowMapsInfo 
        {
            Assets::ShaderTexture mDirPos_SMInfo;         //Texture2DArray
            Assets::ShaderTexture mSpot_SMInfo;         //Texture2DArray
            Assets::ShaderTexture mOmniDir_SMInfo;        //TextureCubeArray
        };

        struct ShaderPipelineVarients
        {
            Rendering::ShaderPipeline StaticSP;
            Rendering::ShaderPipeline SkinnedSP;
        };

        class NEAPI Shader : public Asset<Shader>
        {
        public:

            void Build(const ShaderBuildDesc& desc);

            ShaderPipelineVarients mPipelines;

            RefCntAutoPtr<IShader> VSShader;
            RefCntAutoPtr<IShader> PSShader;

            //TODO: Multiple shader pipeline varients
            //std::unordered_map<ShaderPipelineType, Rendering::ShaderPipeline> mPipelines;
            virtual void ReflectPixelShaderData();


            //This can be filled automatically by ReflectPixelShaderData(), Or fill it manually
            //Note: It is very important to fill it in order for material creation work with the pipeline.
            std::vector<Assets::ShaderTexture> mMaterialTexturesInfo;
            std::vector<Assets::ShaderTexture> mIBLTexturesInfo;

            ShaderShadowMapsInfo mShadowMapsInfo;

            ShaderType mType;

        protected:
            Rendering::ShaderPipeline* pMainPipeline;
        };
    }
}