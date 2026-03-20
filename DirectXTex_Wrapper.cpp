// DirectXTex_Wrapper.cpp
// C++/CLI unified implementation: toda la lógica de DirectXTex dentro de métodos .NET
#include "DirectXTex_Wrapper.h"    // Define structs nativos y clases CLI bajo _MANAGED
#include <DirectXTex.h>
#include "OpenGL_MapFormats.h"
#include "OpenGL_MapFormats_FallBacks.h"

#include <vector>
#include <cstring>
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
    static size_t CalculateMipExtent(size_t baseExtent, size_t mipLevel) noexcept
    {
        size_t value = baseExtent >> mipLevel;
        return value > 0 ? value : 1;
    }

    static size_t CalculateSurfaceRowCount(size_t height, DXGI_FORMAT format) noexcept
    {
        if (DirectX::IsCompressed(format))
        {
            size_t rows = (height + 3u) / 4u;
            return rows > 0 ? rows : 1;
        }

        return height > 0 ? height : 1;
    }

    static DXGI_FORMAT ChooseDecompressFormatForSource(DXGI_FORMAT srcFmt) noexcept
    {
        bool isSRGB = DirectX::IsSRGB(srcFmt);

        switch (srcFmt)
        {
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case DXGI_FORMAT_BC4_UNORM:
            return DXGI_FORMAT_R8_UNORM;

        case DXGI_FORMAT_BC4_SNORM:
            return DXGI_FORMAT_R8_SNORM;

        case DXGI_FORMAT_BC5_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_BC5_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;

        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return isSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    static DXGI_FORMAT ChooseCanonicalFormatForCompressedOutput(DXGI_FORMAT outputFmt) noexcept
    {
        bool isSRGB = DirectX::IsSRGB(outputFmt);

        switch (outputFmt)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return isSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

        case DXGI_FORMAT_BC4_UNORM:
            return DXGI_FORMAT_R8_UNORM;

        case DXGI_FORMAT_BC4_SNORM:
            return DXGI_FORMAT_R8_SNORM;

        case DXGI_FORMAT_BC5_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_BC5_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;

        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    static void CopyManagedSubresourceToScratchImage(
        TextureSubresource^ managedSubresource,
        size_t expectedWidth,
        size_t expectedHeight,
        DXGI_FORMAT format,
        const DirectX::Image* destinationImage)
    {
        if (managedSubresource == nullptr)
            throw gcnew ArgumentNullException("Subresources");
        if (managedSubresource->Data == nullptr)
            throw gcnew ArgumentException("Cada subrecurso debe tener Data.", "Subresources");
        if (managedSubresource->Data->Length == 0)
            throw gcnew ArgumentException("Cada subrecurso debe tener datos.", "Subresources");
        if (destinationImage == nullptr || destinationImage->pixels == nullptr)
            throw gcnew InvalidOperationException("No se pudo obtener la imagen destino para el subrecurso.");

        if (managedSubresource->Width > 0 && managedSubresource->Width != static_cast<int>(expectedWidth))
            throw gcnew ArgumentException("Width del subrecurso no coincide con el mip esperado.", "Subresources");
        if (managedSubresource->Height > 0 && managedSubresource->Height != static_cast<int>(expectedHeight))
            throw gcnew ArgumentException("Height del subrecurso no coincide con el mip esperado.", "Subresources");

        size_t tightRowPitch = 0;
        size_t tightSlicePitch = 0;
        HRESULT hr = DirectX::ComputePitch(format, expectedWidth, expectedHeight, tightRowPitch, tightSlicePitch, DirectX::CP_FLAGS_NONE);
        if (FAILED(hr))
            throw gcnew InvalidOperationException("ComputePitch fallÃ³ para el formato de entrada.");

        size_t sourceRowPitch = managedSubresource->RowPitch > 0
            ? static_cast<size_t>(managedSubresource->RowPitch)
            : tightRowPitch;
        size_t sourceSlicePitch = managedSubresource->SlicePitch > 0
            ? static_cast<size_t>(managedSubresource->SlicePitch)
            : tightSlicePitch;
        size_t sourceLength = static_cast<size_t>(managedSubresource->Data->LongLength);

        bool hasExplicitPitch = (managedSubresource->RowPitch > 0) || (managedSubresource->SlicePitch > 0);
        if (!hasExplicitPitch)
        {
            if (sourceLength != tightSlicePitch)
                throw gcnew ArgumentException("Cuando RowPitch/SlicePitch no se informan, el subrecurso debe venir tight-packed.", "Subresources");
        }
        else
        {
            if (sourceLength < sourceSlicePitch)
                throw gcnew ArgumentException("Data es mÃ¡s chico que SlicePitch para el subrecurso.", "Subresources");
        }

        if (sourceRowPitch < tightRowPitch)
            throw gcnew ArgumentException("RowPitch del subrecurso es menor que el mÃ­nimo requerido.", "Subresources");

        size_t rowCount = CalculateSurfaceRowCount(expectedHeight, format);
        if (sourceSlicePitch < (sourceRowPitch * rowCount))
            throw gcnew ArgumentException("SlicePitch del subrecurso no alcanza para contener todas las filas.", "Subresources");

        if (destinationImage->rowPitch < tightRowPitch || destinationImage->slicePitch < tightSlicePitch)
            throw gcnew InvalidOperationException("El buffer destino no tiene el pitch esperado.");

        pin_ptr<Byte> pinnedSource = &managedSubresource->Data[0];
        const auto srcBytes = reinterpret_cast<const uint8_t*>(pinnedSource);
        auto dstBytes = const_cast<uint8_t*>(destinationImage->pixels);

        for (size_t row = 0; row < rowCount; ++row)
        {
            std::memcpy(
                dstBytes + (row * destinationImage->rowPitch),
                srcBytes + (row * sourceRowPitch),
                tightRowPitch);
        }
    }

    static TextureSubresource^ CreateManagedSubresource(
        const DirectX::Image* image,
        int mipLevel,
        int arrayIndex)
    {
        if (image == nullptr || image->pixels == nullptr)
            throw gcnew InvalidOperationException("Subrecurso de salida invÃ¡lido.");
        if (image->slicePitch > static_cast<size_t>(Int32::MaxValue))
            throw gcnew InvalidOperationException("El subrecurso de salida excede el tamaÃ±o mÃ¡ximo soportado por .NET.");

        int sizeBytes = static_cast<int>(image->slicePitch);
        auto managedData = gcnew array<Byte>(sizeBytes);
        Marshal::Copy(IntPtr(const_cast<uint8_t*>(image->pixels)), managedData, 0, sizeBytes);

        return gcnew TextureSubresource(
            managedData,
            static_cast<int>(image->width),
            static_cast<int>(image->height),
            static_cast<int>(image->rowPitch),
            static_cast<int>(image->slicePitch),
            mipLevel,
            arrayIndex);
    }

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

    TextureSubresource::TextureSubresource()
        : Data(gcnew array<Byte>(0)), Width(0), Height(0), RowPitch(0), SlicePitch(0), MipLevel(-1), ArrayIndex(-1)
    {}

    TextureSubresource::TextureSubresource(array<Byte>^ data, int width, int height, int rowPitch, int slicePitch, int mipLevel, int arrayIndex)
        : Data(data), Width(width), Height(height), RowPitch(rowPitch), SlicePitch(slicePitch), MipLevel(mipLevel), ArrayIndex(arrayIndex)
    {}

    TextureConversionRequest::TextureConversionRequest()
        : Subresources(gcnew array<TextureSubresource^>(0)),
        Width(0), Height(0),
        InputDxgiFormat(static_cast<int>(DXGI_FORMAT_UNKNOWN)),
        OutputDxgiFormat(static_cast<int>(DXGI_FORMAT_UNKNOWN)),
        MipLevels(1), ArraySize(1),
        IsCubemap(false),
        AutoGenerateMipMaps(false),
        FilterFlags(static_cast<int>(TEX_FILTER_DEFAULT)),
        CompressFlags(static_cast<int>(TEX_COMPRESS_DEFAULT)),
        AlphaThreshold(TEX_THRESHOLD_DEFAULT)
    {}

    TextureConversionResult::TextureConversionResult()
        : Subresources(gcnew array<TextureSubresource^>(0)),
        Width(0), Height(0), DxgiFormat(static_cast<int>(DXGI_FORMAT_UNKNOWN)),
        MipLevels(0), ArraySize(0), IsCubemap(false)
    {}


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

    TextureConversionResult^ Loader::ConvertSubresources(TextureConversionRequest^ request)
    {
        if (request == nullptr)
            throw gcnew ArgumentNullException("request");
        if (request->Width <= 0 || request->Height <= 0)
            throw gcnew ArgumentOutOfRangeException("request", "Width and Height must be > 0.");
        if (request->ArraySize <= 0)
            throw gcnew ArgumentOutOfRangeException("request", "ArraySize must be > 0.");
        if (request->IsCubemap && (request->ArraySize % 6) != 0)
            throw gcnew ArgumentException("ArraySize must be a multiple of 6 for cubemaps.", "request");

        bool autoGenerateMipMaps = request->AutoGenerateMipMaps;
        size_t requestedMipLevels = 0;
        if (autoGenerateMipMaps)
        {
            if (request->MipLevels < 0)
                throw gcnew ArgumentOutOfRangeException("request", "MipLevels must be >= 0 when AutoGenerateMipMaps is enabled.");
            requestedMipLevels = request->MipLevels > 0 ? static_cast<size_t>(request->MipLevels) : 0u;
        }
        else
        {
            if (request->MipLevels <= 0)
                throw gcnew ArgumentOutOfRangeException("request", "MipLevels must be > 0.");
            requestedMipLevels = static_cast<size_t>(request->MipLevels);
        }

        int inputMipCount = autoGenerateMipMaps ? 1 : request->MipLevels;
        long long expectedCount64 = static_cast<long long>(inputMipCount) * static_cast<long long>(request->ArraySize);
        if (expectedCount64 <= 0 || expectedCount64 > static_cast<long long>(Int32::MaxValue))
            throw gcnew ArgumentException("The total number of subresources is invalid.", "request");

        int expectedCount = static_cast<int>(expectedCount64);
        if (request->Subresources == nullptr || request->Subresources->Length != expectedCount)
        {
            if (autoGenerateMipMaps)
                throw gcnew ArgumentException("Subresources must contain exactly ArraySize base subresources when AutoGenerateMipMaps is enabled.", "request");
            throw gcnew ArgumentException("Subresources must contain exactly MipLevels * ArraySize elements.", "request");
        }

        DXGI_FORMAT inputFormat = static_cast<DXGI_FORMAT>(request->InputDxgiFormat);
        DXGI_FORMAT outputFormat = static_cast<DXGI_FORMAT>(request->OutputDxgiFormat);
        if (inputFormat == DXGI_FORMAT_UNKNOWN)
            throw gcnew ArgumentException("InputDxgiFormat cannot be DXGI_FORMAT_UNKNOWN.", "request");
        if (outputFormat == DXGI_FORMAT_UNKNOWN)
            throw gcnew ArgumentException("OutputDxgiFormat cannot be DXGI_FORMAT_UNKNOWN.", "request");
        if (DirectX::IsTypeless(inputFormat) || DirectX::IsTypeless(outputFormat))
            throw gcnew ArgumentException("Typeless formats are not supported by ConvertSubresources.", "request");

        TexMetadata sourceMetadata = {};
        sourceMetadata.width = static_cast<size_t>(request->Width);
        sourceMetadata.height = static_cast<size_t>(request->Height);
        sourceMetadata.depth = 1;
        sourceMetadata.arraySize = static_cast<size_t>(request->ArraySize);
        sourceMetadata.mipLevels = static_cast<size_t>(inputMipCount);
        sourceMetadata.miscFlags = request->IsCubemap ? TEX_MISC_TEXTURECUBE : 0;
        sourceMetadata.miscFlags2 = 0;
        sourceMetadata.format = inputFormat;
        sourceMetadata.dimension = TEX_DIMENSION_TEXTURE2D;

        ScratchImage sourceImage;
        HRESULT hr = sourceImage.Initialize(sourceMetadata);
        if (FAILED(hr))
            throw gcnew InvalidOperationException("Unable to initialize the source texture for conversion.");

        for (int mip = 0; mip < inputMipCount; ++mip)
        {
            size_t mipWidth = CalculateMipExtent(sourceMetadata.width, static_cast<size_t>(mip));
            size_t mipHeight = CalculateMipExtent(sourceMetadata.height, static_cast<size_t>(mip));

            for (int item = 0; item < request->ArraySize; ++item)
            {
                int index = (mip * request->ArraySize) + item;
                auto managedSubresource = request->Subresources[index];
                if (managedSubresource == nullptr)
                    throw gcnew ArgumentException("Subresources contains a null element.", "request");
                if (managedSubresource->MipLevel >= 0 && managedSubresource->MipLevel != mip)
                    throw gcnew ArgumentException("Subresource MipLevel does not match its position.", "request");
                if (managedSubresource->ArrayIndex >= 0 && managedSubresource->ArrayIndex != item)
                    throw gcnew ArgumentException("Subresource ArrayIndex does not match its position.", "request");

                auto destinationImage = sourceImage.GetImage(static_cast<size_t>(mip), static_cast<size_t>(item), 0);
                CopyManagedSubresourceToScratchImage(managedSubresource, mipWidth, mipHeight, inputFormat, destinationImage);
            }
        }

        ScratchImage workingImage = std::move(sourceImage);
        bool willGenerateMipMaps = autoGenerateMipMaps && (requestedMipLevels != 1u);

        if (DirectX::IsCompressed(inputFormat) && ((inputFormat != outputFormat) || willGenerateMipMaps))
        {
            DXGI_FORMAT decompressFormat = DirectX::IsCompressed(outputFormat)
                ? ChooseCanonicalFormatForCompressedOutput(outputFormat)
                : ChooseDecompressFormatForSource(inputFormat);

            ScratchImage decompressedImage;
            hr = DirectX::Decompress(
                workingImage.GetImages(),
                workingImage.GetImageCount(),
                workingImage.GetMetadata(),
                decompressFormat,
                decompressedImage);
            if (FAILED(hr))
                throw gcnew InvalidOperationException("Unable to decompress the input texture.");

            workingImage = std::move(decompressedImage);
        }

        if (willGenerateMipMaps)
        {
            ScratchImage mipmappedImage;
            hr = DirectX::GenerateMipMaps(
                workingImage.GetImages(),
                workingImage.GetImageCount(),
                workingImage.GetMetadata(),
                static_cast<TEX_FILTER_FLAGS>(request->FilterFlags),
                requestedMipLevels,
                mipmappedImage);
            if (FAILED(hr))
                throw gcnew InvalidOperationException("Unable to auto-generate mipmaps for the requested texture.");

            workingImage = std::move(mipmappedImage);
        }

        if (DirectX::IsCompressed(outputFormat))
        {
            if (workingImage.GetMetadata().format != outputFormat)
            {
                DXGI_FORMAT canonicalFormat = ChooseCanonicalFormatForCompressedOutput(outputFormat);
                if (workingImage.GetMetadata().format != canonicalFormat)
                {
                    ScratchImage canonicalImage;
                    hr = DirectX::Convert(
                        workingImage.GetImages(),
                        workingImage.GetImageCount(),
                        workingImage.GetMetadata(),
                        canonicalFormat,
                        static_cast<TEX_FILTER_FLAGS>(request->FilterFlags),
                        request->AlphaThreshold,
                        canonicalImage);
                    if (FAILED(hr))
                        throw gcnew InvalidOperationException("Unable to convert to the intermediate format needed for compression.");

                    workingImage = std::move(canonicalImage);
                }

                ScratchImage compressedImage;
                hr = DirectX::Compress(
                    workingImage.GetImages(),
                    workingImage.GetImageCount(),
                    workingImage.GetMetadata(),
                    outputFormat,
                    static_cast<TEX_COMPRESS_FLAGS>(request->CompressFlags),
                    request->AlphaThreshold,
                    compressedImage);
                if (FAILED(hr))
                    throw gcnew InvalidOperationException("Unable to compress to the requested output format.");

                workingImage = std::move(compressedImage);
            }
        }
        else if (workingImage.GetMetadata().format != outputFormat)
        {
            ScratchImage convertedImage;
            hr = DirectX::Convert(
                workingImage.GetImages(),
                workingImage.GetImageCount(),
                workingImage.GetMetadata(),
                outputFormat,
                static_cast<TEX_FILTER_FLAGS>(request->FilterFlags),
                request->AlphaThreshold,
                convertedImage);
            if (FAILED(hr))
                throw gcnew InvalidOperationException("Unable to convert to the requested output format.");

            workingImage = std::move(convertedImage);
        }

        const TexMetadata& resultMetadata = workingImage.GetMetadata();
        unsigned long long resultCount64 = static_cast<unsigned long long>(resultMetadata.mipLevels) * static_cast<unsigned long long>(resultMetadata.arraySize);
        if (resultCount64 > static_cast<unsigned long long>(Int32::MaxValue))
            throw gcnew InvalidOperationException("The resulting texture exceeds the range supported by .NET.");
        if (resultMetadata.width > static_cast<size_t>(Int32::MaxValue) || resultMetadata.height > static_cast<size_t>(Int32::MaxValue))
            throw gcnew InvalidOperationException("The resulting texture exceeds the range supported by .NET.");

        int resultCount = static_cast<int>(resultCount64);
        auto result = gcnew TextureConversionResult();
        result->Width = static_cast<int>(resultMetadata.width);
        result->Height = static_cast<int>(resultMetadata.height);
        result->DxgiFormat = static_cast<int>(resultMetadata.format);
        result->MipLevels = static_cast<int>(resultMetadata.mipLevels);
        result->ArraySize = static_cast<int>(resultMetadata.arraySize);
        result->IsCubemap = resultMetadata.IsCubemap() != 0;
        result->Subresources = gcnew array<TextureSubresource^>(resultCount);

        int writeIndex = 0;
        for (int mip = 0; mip < result->MipLevels; ++mip)
        {
            for (int item = 0; item < result->ArraySize; ++item)
            {
                auto image = workingImage.GetImage(static_cast<size_t>(mip), static_cast<size_t>(item), 0);
                result->Subresources[writeIndex] = CreateManagedSubresource(image, mip, item);
                ++writeIndex;
            }
        }

        return result;
    }

} // namespace DirectXTexWrapperCLI

#endif // _MANAGED




