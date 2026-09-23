/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */
#include <Diligent/Primitives/interface/BasicTypes.h>

#include <Diligent/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Diligent/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Diligent/Primitives/interface/Errors.hpp>
#include <Diligent/Platforms/Basic/interface/DebugUtilities.hpp>

#include <algorithm>
#include <limits>
#include <math.h>
#include <vector>

#include "TextureLoaderImpl.hpp"
#include <Diligent/Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp>

#include <Diligent/Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp>

#include <Diligent/Graphics/GraphicsAccessories/interface/ColorConversion.h>
#include <Diligent/Graphics/GraphicsTools/interface/GraphicsUtilities.h>

#include "Image.h"
#include <Diligent/Common/interface/FileWrapper.hpp>
#include <Diligent/Common/interface/DataBlobImpl.hpp>
#include <Diligent/Common/interface/Align.hpp>

namespace Diligent
{

template <typename ChannelType>
void ModifyComponentCount(const void* pSrcData,
                          Uint32      SrcStride,
                          Uint32      SrcCompCount,
                          void*       pDstData,
                          Uint32      DstStride,
                          Uint32      Width,
                          Uint32      Height,
                          Uint32      DstCompCount)
{
    auto CompToCopy = std::min(SrcCompCount, DstCompCount);
    for (size_t row = 0; row < size_t{Height}; ++row)
    {
        for (size_t col = 0; col < size_t{Width}; ++col)
        {
            // clang-format off
            auto*       pDst = reinterpret_cast<      ChannelType*>((reinterpret_cast<      Uint8*>(pDstData) + size_t{DstStride} * row)) + col * DstCompCount;
            const auto* pSrc = reinterpret_cast<const ChannelType*>((reinterpret_cast<const Uint8*>(pSrcData) + size_t{SrcStride} * row)) + col * SrcCompCount;
            // clang-format on

            for (size_t c = 0; c < CompToCopy; ++c)
                pDst[c] = pSrc[c];

            for (size_t c = CompToCopy; c < DstCompCount; ++c)
            {
                pDst[c] = c < 3 ?
                    (SrcCompCount == 1 ? pSrc[0] : 0) :      // For single-channel source textures, propagate r to other channels
                    std::numeric_limits<ChannelType>::max(); // Use 1.0 as default value for alpha
            }
        }
    }
}

static TextureDesc TexDescFromTexLoadInfo(const TextureLoadInfo& TexLoadInfo, const std::string& Name)
{
    TextureDesc TexDesc;
    TexDesc.Name           = Name.c_str();
    TexDesc.Format         = TexLoadInfo.Format;
    TexDesc.Usage          = TexLoadInfo.Usage;
    TexDesc.BindFlags      = TexLoadInfo.BindFlags;
    TexDesc.CPUAccessFlags = TexLoadInfo.CPUAccessFlags;
    return TexDesc;
}

TextureLoaderImpl::TextureLoaderImpl(IReferenceCounters*        pRefCounters,
                                     const TextureLoadInfo&     TexLoadInfo,
                                     const Uint8*               pData,
                                     size_t                     DataSize,
                                     RefCntAutoPtr<IDataBlob>&& pDataBlob) :
    TBase{pRefCounters},
    m_pDataBlob{std::move(pDataBlob)},
    m_Name{TexLoadInfo.Name != nullptr ? TexLoadInfo.Name : ""},
    m_TexDesc{TexDescFromTexLoadInfo(TexLoadInfo, m_Name)}
{
    const auto ImgFileFormat = Image::GetFileFormat(pData, DataSize);
    if (ImgFileFormat == IMAGE_FILE_FORMAT_UNKNOWN)
    {
        LOG_ERROR_AND_THROW("Unable to derive image format.");
    }

    if (ImgFileFormat == IMAGE_FILE_FORMAT_PNG ||
        ImgFileFormat == IMAGE_FILE_FORMAT_JPEG ||
        ImgFileFormat == IMAGE_FILE_FORMAT_TIFF ||
        ImgFileFormat == IMAGE_FILE_FORMAT_SGI)
    {
        LOG_ERROR_AND_THROW("image format support removed.");

      /*  ImageLoadInfo ImgLoadInfo;
        ImgLoadInfo.Format = ImgFileFormat;
        if (!m_pDataBlob)
        {
            m_pDataBlob = DataBlobImpl::Create(DataSize, pData);
        }
        Image::CreateFromDataBlob(m_pDataBlob, ImgLoadInfo, &m_pImage);
        LoadFromImage(TexLoadInfo);
        m_pDataBlob.Release();*/
    }
    else
    {
        if (ImgFileFormat == IMAGE_FILE_FORMAT_DDS)
        {
            LoadFromDDS(TexLoadInfo, pData, DataSize);
        }
        else if (ImgFileFormat == IMAGE_FILE_FORMAT_KTX)
        {
            LoadFromKTX(TexLoadInfo, pData, DataSize);
        }
    }

    if (TexLoadInfo.IsSRGB)
    {
        m_TexDesc.Format = UnormFormatToSRGB(m_TexDesc.Format);
    }
}

void TextureLoaderImpl::CreateTexture(IRenderDevice* pDevice,
                                      ITexture**     ppTexture)
{
    TextureData InitData{m_SubResources.data(), static_cast<Uint32>(m_SubResources.size())};
    pDevice->CreateTexture(m_TexDesc, &InitData, ppTexture);
}


void CreateTextureLoaderFromFile(const char*            FilePath,
                                 IMAGE_FILE_FORMAT      FileFormat,
                                 const TextureLoadInfo& TexLoadInfo,
                                 ITextureLoader**       ppLoader)
{
    try
    {
        FileWrapper File{FilePath, EFileAccessMode::Read};
        if (!File)
            LOG_ERROR_AND_THROW("Failed to open file '", FilePath, "'.");

        auto pFileData = DataBlobImpl::Create();
        File->Read(pFileData);

        RefCntAutoPtr<ITextureLoader> pTexLoader{
            MakeNewRCObj<TextureLoaderImpl>()(TexLoadInfo, reinterpret_cast<const Uint8*>(pFileData->GetConstDataPtr()), pFileData->GetSize(), std::move(pFileData)) //
        };
        if (pTexLoader)
            pTexLoader->QueryInterface(IID_TextureLoader, reinterpret_cast<IObject**>(ppLoader));
    }
    catch (std::runtime_error& err)
    {
        LOG_ERROR("Failed to create texture loader from file: ", err.what());
    }
}

void CreateTextureLoaderFromMemory(const void*            pData,
                                   size_t                 Size,
                                   IMAGE_FILE_FORMAT      FileFormat,
                                   bool                   MakeDataCopy,
                                   const TextureLoadInfo& TexLoadInfo,
                                   ITextureLoader**       ppLoader)
{
    VERIFY_EXPR(pData != nullptr && Size > 0);
    try
    {
        RefCntAutoPtr<IDataBlob> pDataCopy;
        if (MakeDataCopy)
        {
            pDataCopy = DataBlobImpl::Create(Size, pData);
            pData     = pDataCopy->GetConstDataPtr();
        }
        RefCntAutoPtr<ITextureLoader> pTexLoader{MakeNewRCObj<TextureLoaderImpl>()(TexLoadInfo, reinterpret_cast<const Uint8*>(pData), Size, std::move(pDataCopy))};
        if (pTexLoader)
            pTexLoader->QueryInterface(IID_TextureLoader, reinterpret_cast<IObject**>(ppLoader));
    }
    catch (std::runtime_error& err)
    {
        LOG_ERROR("Failed to create texture loader from memory: ", err.what());
    }
}


} // namespace Diligent

extern "C"
{
    void Diligent_CreateTextureLoaderFromFile(const char*                      FilePath,
                                              Diligent::IMAGE_FILE_FORMAT      FileFormat,
                                              const Diligent::TextureLoadInfo& TexLoadInfo,
                                              Diligent::ITextureLoader**       ppLoader)
    {
        Diligent::CreateTextureLoaderFromFile(FilePath, FileFormat, TexLoadInfo, ppLoader);
    }

    void Diligent_CreateTextureLoaderFromMemory(const void*                      pData,
                                                size_t                           Size,
                                                Diligent::IMAGE_FILE_FORMAT      FileFormat,
                                                bool                             MakeCopy,
                                                const Diligent::TextureLoadInfo& TexLoadInfo,
                                                Diligent::ITextureLoader**       ppLoader)
    {
        Diligent::CreateTextureLoaderFromMemory(pData, Size, FileFormat, MakeCopy, TexLoadInfo, ppLoader);
    }
}
