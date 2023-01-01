#pragma once
#include <NE_Common.h>
#include <Diligent/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <Diligent\Graphics\GraphicsTools\interface\GraphicsUtilities.h>
#include <string>

namespace Nuclear
{
	namespace Assets
	{
		class NEAPI ImageData
		{
		public:
			ImageData()
			{
				mPath = "";
				mWidth = 0;
				mHeight = 0;
				mBitsPerPixel = 0;
				mNumComponents = 0;
				mRowStride = 0;
				mData = nullptr;
				mComponentType = VT_UNDEFINED;
				mUsage = USAGE_IMMUTABLE;
				mBindFlags = BIND_SHADER_RESOURCE;
				mMipLevels = 0;
				mCPUAccessFlags = CPU_ACCESS_NONE;
				mIsSRGB = false;
				mFormat = TEX_FORMAT_UNKNOWN;
			}
			~ImageData()
			{
				mPath = "";
				mWidth = 0;
				mHeight = 0;
				mBitsPerPixel = 0;
				mNumComponents = 0;
				mRowStride = 0;
				mData = nullptr;
				mComponentType = VT_UNDEFINED;
			}

			std::string mPath;

			Uint32 mWidth;

			Uint32 mHeight;

			Uint32 mBitsPerPixel;

			Uint32 mNumComponents;

			Uint32 mRowStride;

			VALUE_TYPE mComponentType;

			Byte* mData;

			//DESC
			USAGE mUsage;

			BIND_FLAGS mBindFlags;

			CPU_ACCESS_FLAGS mCPUAccessFlags;

			RESOURCE_DIMENSION mType;

			TEXTURE_FORMAT mFormat;

			bool mGenerateMipMaps = true;

			Uint32 mMipLevels;

			bool mIsSRGB;

			float mMipMapsAlphaCutoff = 0;

			MIP_FILTER_TYPE mMipMapsFilter = MIP_FILTER_TYPE_DEFAULT;
		};
	}
}