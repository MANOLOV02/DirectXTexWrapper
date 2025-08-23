// DirectXTex_Wrapper.cpp
// C++/CLI unified implementation: toda la lógica de DirectXTex dentro de métodos .NET
#include "DirectXTex_Wrapper.h"    // Define structs nativos y clases CLI bajo _MANAGED
#include <DirectXTex.h>
#include "OpenGL_MapFormats.h"
#include "OpenGL_MapFormats_FallBacks.h"

#include <vector>
#include <msclr/marshal.h>
#include <vcclr.h>

using namespace DirectX;
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace msclr;
#ifdef _MANAGED

namespace DirectXTexWrapperCLI
{

    array<System::Byte>^ Loader::EncodeDDSHeader(
        int dxgiFormat,
        int width,
        int height,
        int arraySize,
        int mipLevels,
        bool isCubemap
    )
    {
        using namespace DirectX;

        if (width <= 0 || height <= 0) throw gcnew System::ArgumentOutOfRangeException("dimensiones inválidas");
        if (arraySize <= 0) arraySize = 1;
        if (mipLevels <= 0) mipLevels = 1;

        TexMetadata meta = {};
        meta.width = static_cast<size_t>(width);
        meta.height = static_cast<size_t>(height);
        meta.depth = 1;
        meta.arraySize = static_cast<size_t>(arraySize);
        meta.mipLevels = static_cast<size_t>(mipLevels);
        meta.miscFlags = isCubemap ? TEX_MISC_TEXTURECUBE : 0;
        meta.miscFlags2 = 0;
        meta.format = static_cast<DXGI_FORMAT>(dxgiFormat);
        meta.dimension = TEX_DIMENSION_TEXTURE2D; // FO4 DX10 ? 2D

        size_t required = 0;
        HRESULT hr = DirectX::EncodeDDSHeader(meta, DDS_FLAGS_NONE, nullptr, 0, required);
        if (FAILED(hr) || required == 0)
            throw gcnew System::InvalidOperationException("EncodeDDSHeader (consulta tamaño) falló");

        std::vector<uint8_t> header(required);
        hr = DirectX::EncodeDDSHeader(meta, DDS_FLAGS_NONE, header.data(), header.size(), required);
        if (FAILED(hr))
            throw gcnew System::InvalidOperationException("EncodeDDSHeader (escritura) falló");

        auto managed = gcnew array<System::Byte>(static_cast<int>(header.size()));
        System::Runtime::InteropServices::Marshal::Copy(
            System::IntPtr(header.data()), managed, 0, static_cast<int>(header.size())
        );
        return managed;
    }

// -----------------------
// Constructores CLI
// -----------------------
    TextureLevel::TextureLevel(array<Byte>^ data, int width, int height)
        : Data(data), Width(width), Height(height)
    {}

    TextureLoaded::TextureLoaded()
    {
        Levels = gcnew List<TextureLevel^>(0);
    }

    // Helper: calcula tamaño comprimido
    static int CalculateCompressedSize(int width, int height, DXGI_FORMAT fmt)
    {
        int blockSize = 16;
        switch (fmt)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            blockSize = 8;
            break;
        default:
            blockSize = 16;
            break;
        }
        int bw = (width + 3) / 4; if (bw < 1) bw = 1;
        int bh = (height + 3) / 4; if (bh < 1) bh = 1;
        return bw * bh * blockSize;
    }

    // -----------------------
    // Loader: LoadTextures
    // -----------------------
    List<TextureLoaded^>^ Loader::LoadTextures(array<array<Byte>^>^ ddsFiles, bool useCompress, bool forceOpenGL)
    {
        if (ddsFiles == nullptr || ddsFiles->Length == 0)
            throw gcnew ArgumentException("ddsFiles debe contener al menos una textura");

        auto output = gcnew List<TextureLoaded^>(ddsFiles->Length);

        for (int t = 0; t < ddsFiles->Length; ++t)
        {
            auto texfb = gcnew TextureLoaded();
            texfb->Loaded = false;        // por defecto
            texfb->DxgiCodeOriginal = -1; // valores seguros por defecto
            texfb->DxgiCodeFinal = -1;

            auto dataArr = ddsFiles[t];
            if (dataArr == nullptr || dataArr->Length == 0)
            {
                output->Add(texfb);
                continue;
            }

            // Carga DDS
            pin_ptr<Byte> pData = &dataArr[0];
            ScratchImage image;
            HRESULT hr = DirectX::LoadFromDDSMemory(
                reinterpret_cast<const uint8_t*>(pData),
                static_cast<size_t>(dataArr->Length),
                DDS_FLAGS_NONE, nullptr, image
            );
            if (FAILED(hr))
            {
                output->Add(texfb);
                continue;
            }

            const TexMetadata* meta = &image.GetMetadata();
            int dxgiOrig = static_cast<int>(meta->format);
            int dxgiFinal = static_cast<int>(meta->format);
            bool compressed = IsCompressed(meta->format);

            // Determinar compatibilidad OpenGL
            const FormatMapping* fmap = GetFormatMapping(dxgiFinal);
            bool compatibleGL = fmap && fmap->glInternalFormat && (!compressed || useCompress);
            bool needsDecompress = (compressed && !useCompress) || (forceOpenGL && !compatibleGL);

            // Imagen final
            ScratchImage finalImg;
            if (!needsDecompress)
            {
                finalImg = std::move(image);
                // Tras mover, refrescar SIEMPRE metadata/flags
                meta = &finalImg.GetMetadata();
                dxgiFinal = static_cast<int>(meta->format);
                compressed = IsCompressed(meta->format);
                fmap = GetFormatMapping(dxgiFinal);
                compatibleGL = fmap && fmap->glInternalFormat && (!compressed || useCompress);
            }
            else
            {
                // --- Descompresión inteligente con sRGB ---
                DXGI_FORMAT srcFmt = meta->format;
                bool isSRGB = DirectX::IsSRGB(srcFmt);

                DXGI_FORMAT decompFmt = DXGI_FORMAT_R8G8B8A8_UNORM; // default para BC1/2/3/7 LDR
                switch (srcFmt)
                {
                // BC6H (HDR) -> float 16
                case DXGI_FORMAT_BC6H_UF16:
                case DXGI_FORMAT_BC6H_SF16:
                    decompFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
                    break;

                // BC4 -> 1 canal
                case DXGI_FORMAT_BC4_UNORM:
                case DXGI_FORMAT_BC4_SNORM:
                    decompFmt = DXGI_FORMAT_R8_UNORM;
                    break;

                // BC5 -> 2 canales
                case DXGI_FORMAT_BC5_UNORM:
                case DXGI_FORMAT_BC5_SNORM:
                    decompFmt = DXGI_FORMAT_R8G8_UNORM;
                    break;

                // BC1/2/3/7 (UNORM/SRGB) -> RGBA8 (usar _SRGB si fuente es SRGB)
                case DXGI_FORMAT_BC1_UNORM:
                case DXGI_FORMAT_BC1_UNORM_SRGB:
                case DXGI_FORMAT_BC2_UNORM:
                case DXGI_FORMAT_BC2_UNORM_SRGB:
                case DXGI_FORMAT_BC3_UNORM:
                case DXGI_FORMAT_BC3_UNORM_SRGB:
                case DXGI_FORMAT_BC7_UNORM:
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    decompFmt = isSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                        : DXGI_FORMAT_R8G8B8A8_UNORM;
                    break;

                default:
                    // Para otros comprimidos raros, dejar UNKNOWN para que el lib elija
                    if (compressed)
                        decompFmt = DXGI_FORMAT_UNKNOWN;
                    break;
                }

                hr = DirectX::Decompress(
                    image.GetImages(), image.GetImageCount(), *meta,
                    decompFmt,           // <- NUNCA pasar un formato BC aquí
                    finalImg
                );
                if (FAILED(hr))
                {
                    output->Add(texfb);
                    continue;
                }

                // Refrescar metadata/flags tras descomprimir
                meta = &finalImg.GetMetadata();
                dxgiFinal = static_cast<int>(meta->format);
                compressed = IsCompressed(meta->format);
                fmap = GetFormatMapping(dxgiFinal);
                compatibleGL = fmap && fmap->glInternalFormat && (!compressed || useCompress);
            }

            // Convertir forzado a OpenGL (si aún no es compatible)
            if (forceOpenGL && !compatibleGL)
            {
                int fallbackCode = ResolveFallback(dxgiFinal);
                hr = DirectX::Convert(
                    finalImg.GetImages(), finalImg.GetImageCount(), *meta,
                    static_cast<DXGI_FORMAT>(fallbackCode),
                    TEX_FILTER_DEFAULT, 0.0f,
                    finalImg
                );
                if (FAILED(hr))
                {
                    output->Add(texfb);
                    continue;
                }

                // Refrescar metadata tras Convert
                meta = &finalImg.GetMetadata();
                dxgiFinal = static_cast<int>(meta->format);
                fmap = GetFormatMapping(dxgiFinal);
            }

            // Construir objeto administrado
            auto tex = gcnew TextureLoaded();
            tex->DxgiCodeOriginal = dxgiOrig;
            tex->DxgiCodeFinal = static_cast<int>(meta->format);
            fmap = GetFormatMapping(tex->DxgiCodeFinal);
            if (!fmap)
            {
                output->Add(texfb);
                continue;
            }

            tex->GlInternalFormat = fmap->glInternalFormat;
            tex->GlPixelFormat = fmap->glPixelFormat;
            tex->GlPixelType = fmap->glPixelType;
            tex->IsCompressedGL = fmap->isCompressed;
            tex->IsCubemap = meta->IsCubemap() != 0;
            tex->Loaded = true;
            tex->Miplevels = max(1, static_cast<int>(finalImg.GetMetadata().mipLevels));
            tex->Faces = static_cast<int>(finalImg.GetMetadata().arraySize); // cast explícito

            // Extraer primero todas las caras del nivel 0, luego las del nivel 1, etc.
            for (int m = 0; m < tex->Miplevels; ++m)
            {
                for (int f = 0; f < tex->Faces; ++f)
                {
                    const Image* img = finalImg.GetImage(m, f, 0); // slice=0 (si quieres 3D, iterar depth)
                    if (!img || !img->pixels) continue;

                    int sizeBytes = fmap->isCompressed
                        ? CalculateCompressedSize(
                            static_cast<int>(img->width),
                            static_cast<int>(img->height),
                            finalImg.GetMetadata().format)
                        : static_cast<int>(img->slicePitch);

                    auto managedData = gcnew array<Byte>(sizeBytes);
                    Marshal::Copy(
                        IntPtr(const_cast<uint8_t*>(img->pixels)),
                        managedData, 0, sizeBytes
                    );

                    tex->Levels->Add(gcnew TextureLevel(
                        managedData,
                        static_cast<int>(img->width),
                        static_cast<int>(img->height)
                    ));
                }
            }

            output->Add(tex);
        }

        return output;
    }



    // -----------------------
    // Loader: ConvertForBitmap
    // -----------------------
    TextureLoaded^ Loader::ConvertForBitmap(array<Byte>^ ddsFile)
    {
        if (ddsFile == nullptr || ddsFile->Length == 0)
            throw gcnew ArgumentException("ddsFile inválido");

        // Cargar DDS
        pin_ptr<Byte> pData = &ddsFile[0];
        DirectX::ScratchImage image;
        HRESULT hr = DirectX::LoadFromDDSMemory(
            reinterpret_cast<const uint8_t*>(pData),
            static_cast<size_t>(ddsFile->Length),
            DirectX::DDS_FLAGS_NONE, nullptr, image
        );
        if (FAILED(hr))
            throw gcnew InvalidOperationException("No se pudo cargar DDS para bitmap");

        const DirectX::TexMetadata* meta = &image.GetMetadata();
        DirectX::ScratchImage finalImg;

        // Queremos un bitmap compatible con .NET: BGRA8 lineal (no sRGB)
        const DXGI_FORMAT targetFmt = DXGI_FORMAT_B8G8R8A8_UNORM;
        const DXGI_FORMAT srcFmt = meta->format;

        // Si está comprimida, descomprimir al formato adecuado según BCn
        if (IsCompressed(srcFmt))
        {
            DXGI_FORMAT decompFmt = DXGI_FORMAT_R8G8B8A8_UNORM; // default para BC1/2/3/7

            switch (srcFmt)
            {
            // HDR (BC6H) -> float16
            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
                decompFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
                break;

            // 1 canal (BC4)
            case DXGI_FORMAT_BC4_UNORM:
            case DXGI_FORMAT_BC4_SNORM:
                decompFmt = DXGI_FORMAT_R8_UNORM;
                break;

            // 2 canales (BC5)
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC5_SNORM:
                decompFmt = DXGI_FORMAT_R8G8_UNORM;
                break;

            // BC1/2/3/7 (incluye *_SRGB) -> RGBA8 lineal (bitmap target es UNORM)
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                decompFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;

            default:
                // Si llega otro formato comprimido, dejar que la lib decida
                decompFmt = DXGI_FORMAT_UNKNOWN;
                break;
            }

            hr = DirectX::Decompress(
                image.GetImages(), image.GetImageCount(), *meta,
                decompFmt,
                finalImg
            );
            if (FAILED(hr))
                throw gcnew InvalidOperationException("No se pudo descomprimir DDS");

            // Si lo descomprimido no es aún BGRA8, convertir
            const DirectX::TexMetadata* metaDecomp = &finalImg.GetMetadata();
            if (metaDecomp->format != targetFmt)
            {
                DirectX::ScratchImage tmp;
                hr = DirectX::Convert(
                    finalImg.GetImages(), finalImg.GetImageCount(), *metaDecomp,
                    targetFmt, DirectX::TEX_FILTER_DEFAULT, 0.0f, tmp
                );
                if (FAILED(hr))
                    throw gcnew InvalidOperationException("Error al convertir DDS a BGRA8");

                finalImg = std::move(tmp);
            }
        }
        else if (srcFmt != targetFmt)
        {
            // No comprimida pero con formato distinto -> convertir a BGRA8
            hr = DirectX::Convert(
                image.GetImages(), image.GetImageCount(), *meta,
                targetFmt, DirectX::TEX_FILTER_DEFAULT, 0.0f, finalImg
            );
            if (FAILED(hr))
                throw gcnew InvalidOperationException("Error en conversión a BGRA8");
        }
        else
        {
            // Ya está en BGRA8
            finalImg = std::move(image);
        }

        // Extraer datos del primer mip/face/slice
        const DirectX::Image* img = finalImg.GetImage(0, 0, 0);
        if (!img || !img->pixels)
            throw gcnew InvalidOperationException("Imagen final inválida");

        int sizeBytes = static_cast<int>(img->slicePitch);
        auto managedData = gcnew array<Byte>(sizeBytes);
        System::Runtime::InteropServices::Marshal::Copy(
            IntPtr(const_cast<uint8_t*>(img->pixels)), managedData, 0, sizeBytes
        );

        // Construir resultado administrado
        auto tex = gcnew TextureLoaded();
        tex->DxgiCodeOriginal = static_cast<int>(srcFmt);                         // formato original del DDS cargado
        tex->DxgiCodeFinal = static_cast<int>(finalImg.GetMetadata().format);  // BGRA8_UNORM
        tex->IsCubemap = false;
        tex->Loaded = true;
        tex->GlInternalFormat = 0;
        tex->GlPixelFormat = 0;
        tex->GlPixelType = 0;
        tex->Miplevels = 1;
        tex->Faces = 1;

        tex->Levels->Add(gcnew TextureLevel(
            managedData,
            static_cast<int>(img->width),
            static_cast<int>(img->height)
        ));

        return tex;
    }

} // namespace DirectXTexWrapperCLI

#endif // _MANAGED

