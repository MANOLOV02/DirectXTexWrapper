// DxtCodec — see DxtCodec.h. Copyright (C) 2026  ManoloV02. GPL-3.0-or-later.
#include "DxtCodec.h"

#include <d3d11.h>
#include <dxgi.h>
#include <mutex>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
    std::once_flag g_deviceOnce;
    ComPtr<ID3D11Device> g_device;
    // the device's immediate context is not free-threaded: DirectXTex's GPU compressor uses it, one call at a time
    std::mutex g_gpuMutex;

    // texconv's CreateDevice (Texconv/texconv.cpp) with its default adapter 0: the DXGI factory's first adapter,
    // feature level 11_0 / 10_1 / 10_0, below 11_0 only with compute shaders on 4.x
    void CreateDeviceOnce() noexcept
    {
        ComPtr<IDXGIFactory1> factory;
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
        {
            if (FAILED(factory->EnumAdapters(0, adapter.GetAddressOf())))
                return;
        }
        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        UINT createFlags = 0;
#ifdef _DEBUG
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        ComPtr<ID3D11Device> device;
        D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_10_0;
        HRESULT hr = D3D11CreateDevice(adapter.Get(), adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                       createFlags, levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
                                       device.GetAddressOf(), &fl, nullptr);
        if (FAILED(hr))
            return;
        if (fl < D3D_FEATURE_LEVEL_11_0)
        {
            D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hw = {};
            if (FAILED(device->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hw, sizeof(hw))) ||
                !hw.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
                return;
        }
        g_device = device;
    }

    ID3D11Device* Device() noexcept
    {
        std::call_once(g_deviceOnce, CreateDeviceOnce);
        return g_device.Get();
    }

    bool HasGpuEncoder(DXGI_FORMAT f) noexcept
    {
        switch (f)
        {
        case DXGI_FORMAT_BC6H_TYPELESS: case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS: case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
            return true;
        default:
            return false;
        }
    }
}

namespace DxtCodec
{
    bool GpuAvailable() noexcept
    {
        return Device() != nullptr;
    }

    HRESULT Compress(const DirectX::Image* images, size_t count, const DirectX::TexMetadata& metadata, DXGI_FORMAT format,
                     DirectX::TEX_COMPRESS_FLAGS flags, float alphaWeight, float alphaThreshold, Encoder encoder,
                     DirectX::ScratchImage& out, bool* usedGpu) noexcept
    {
        if (usedGpu)
            *usedGpu = false;
        if (HasGpuEncoder(format) && encoder != Encoder::Cpu)
        {
            ID3D11Device* device = Device();
            if (device)
            {
                HRESULT hr;
                {
                    std::lock_guard<std::mutex> lock(g_gpuMutex);
                    hr = DirectX::Compress(device, images, count, metadata, format, flags, alphaWeight, out);
                }
                if (SUCCEEDED(hr))
                {
                    if (usedGpu)
                        *usedGpu = true;
                    return hr;
                }
                if (encoder == Encoder::Gpu)
                    return hr;
                out.Release();                          // Auto: the CPU encoder after a failed GPU one
            }
            else if (encoder == Encoder::Gpu)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }
        return DirectX::Compress(images, count, metadata, format, flags, alphaThreshold, out);
    }

    HRESULT Decompress(const DirectX::Image* images, size_t count, const DirectX::TexMetadata& metadata, DXGI_FORMAT format,
                       DirectX::ScratchImage& out) noexcept
    {
        return DirectX::Decompress(images, count, metadata, format, out);
    }
    // The law and why its uses differ: DxtCodec.h (moved from DirectXTex_Wrapper.cpp verbatim).
    DXGI_FORMAT FormatoDeDescompresion(DXGI_FORMAT srcFmt, UsoDeDescompresion uso) noexcept
    {
        const bool preservarSrgb = (uso != UsoDeDescompresion::BitmapAdministrado);
        const bool preservarSigno = (uso == UsoDeDescompresion::PipelineDeConversion);

        switch (srcFmt)
        {
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case DXGI_FORMAT_BC4_UNORM:
            return DXGI_FORMAT_R8_UNORM;

        case DXGI_FORMAT_BC4_SNORM:
            return preservarSigno ? DXGI_FORMAT_R8_SNORM : DXGI_FORMAT_R8_UNORM;

        case DXGI_FORMAT_BC5_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_BC5_SNORM:
            return preservarSigno ? DXGI_FORMAT_R8G8_SNORM : DXGI_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return (preservarSrgb && DirectX::IsSRGB(srcFmt))
                ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                : DXGI_FORMAT_R8G8B8A8_UNORM;

        default:
            // ⛔ El `default` tambien difiere por uso, y NO por capricho: la copia del pipeline devolvia
            // R8G8B8A8_UNORM y las de decode UNKNOWN. Para el pipeline UNKNOWN es mejor (que elija
            // DirectXTex el natural del formato) pero cambiaria bytes en un formato exotico, asi que se
            // conserva lo que cada camino hacia.
            return (uso == UsoDeDescompresion::PipelineDeConversion)
                ? DXGI_FORMAT_R8G8B8A8_UNORM
                : DXGI_FORMAT_UNKNOWN;
        }
    }

    // DirectXTexWrapper's RunConversionPipeline after its managed copy-in (moved verbatim: the same steps, the same
    // calls, in the same order); each throw of the wrapper is the step returned in failed.
    HRESULT RunConversion(DirectX::ScratchImage& workingImage, DXGI_FORMAT outputFormat,
                          const ConversionOptions& options, ConversionStep& failed) noexcept
    {
        using namespace DirectX;
        failed = ConversionStep::None;
        const DXGI_FORMAT inputFormat = workingImage.GetMetadata().format;
        HRESULT hr = S_OK;
        bool willGenerateMipMaps = options.autoGenerateMipMaps && (options.mipLevels != 1u);

        if (DirectX::IsCompressed(inputFormat) && ((inputFormat != outputFormat) || willGenerateMipMaps))
        {
            // Misma ley, distinta LLAVE: si la salida es comprimida hay que aterrizar en el formato
            // canonico que ESA compresion consume; si no, en el natural de la ENTRADA. Antes eran dos
            // funciones y una de ellas se contradecia con la otra en el mismo ternario.
            const DXGI_FORMAT llave = DirectX::IsCompressed(outputFormat) ? outputFormat : inputFormat;
            const DXGI_FORMAT decompressFormat = FormatoDeDescompresion(llave, UsoDeDescompresion::PipelineDeConversion);

            ScratchImage decompressedImage;
            hr = DirectX::Decompress(
                workingImage.GetImages(),
                workingImage.GetImageCount(),
                workingImage.GetMetadata(),
                decompressFormat,
                decompressedImage);
            if (FAILED(hr))
            {
                failed = ConversionStep::Decompress;
                return hr;
            }

            workingImage = std::move(decompressedImage);
        }

        if (willGenerateMipMaps)
        {
            ScratchImage mipmappedImage;
            hr = DirectX::GenerateMipMaps(
                workingImage.GetImages(),
                workingImage.GetImageCount(),
                workingImage.GetMetadata(),
                options.filterFlags,
                options.mipLevels,
                mipmappedImage);
            if (FAILED(hr))
            {
                failed = ConversionStep::MipMaps;
                return hr;
            }

            workingImage = std::move(mipmappedImage);
        }

        if (DirectX::IsCompressed(outputFormat))
        {
            if (workingImage.GetMetadata().format != outputFormat)
            {
                DXGI_FORMAT canonicalFormat = FormatoDeDescompresion(outputFormat, UsoDeDescompresion::PipelineDeConversion);
                if (workingImage.GetMetadata().format != canonicalFormat)
                {
                    ScratchImage canonicalImage;
                    hr = DirectX::Convert(
                        workingImage.GetImages(),
                        workingImage.GetImageCount(),
                        workingImage.GetMetadata(),
                        canonicalFormat,
                        options.filterFlags,
                        options.alphaThreshold,
                        canonicalImage);
                    if (FAILED(hr))
                    {
                        failed = ConversionStep::Canonical;
                        return hr;
                    }

                    workingImage = std::move(canonicalImage);
                }

                ScratchImage compressedImage;
                hr = Compress(
                    workingImage.GetImages(),
                    workingImage.GetImageCount(),
                    workingImage.GetMetadata(),
                    outputFormat,
                    options.compressFlags,
                    options.alphaWeight,
                    options.alphaThreshold,
                    options.encoder,
                    compressedImage);
                if (FAILED(hr))
                {
                    failed = ConversionStep::Compress;
                    return hr;
                }

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
                options.filterFlags,
                options.alphaThreshold,
                convertedImage);
            if (FAILED(hr))
            {
                failed = ConversionStep::Convert;
                return hr;
            }

            workingImage = std::move(convertedImage);
        }

        return S_OK;
    }
}
