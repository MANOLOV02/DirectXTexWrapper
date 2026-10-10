// DxtCodec — DirectXTex compression and decompression shared by DirectXTexWrapper (C++/CLI) and native hosts
// (FabricGen through the C ABI of DxtCodecC.cpp). Copyright (C) 2026  ManoloV02  <https://github.com/MANOLOV02>
// GNU General Public License v3 or later; wraps DirectXTex, (C) Microsoft Corporation, MIT License (see LICENSE).
//
// BC6H / BC7 have a DirectCompute (GPU) encoder in DirectXTex besides the CPU one: the GPU one is ~30x faster
// (a 4K overlay: 13.6 s against 467 s) and gives other bytes. The encoder is chosen per call:
//   Auto - the GPU when a D3D11 device with compute shaders exists (created once per process, adapter 0, as texconv's
//          CreateDevice), otherwise - or when the GPU compression fails - the CPU;
//   Cpu  - the CPU always;
//   Gpu  - the GPU or an error (no silent fallback: a caller that must not mix the two codecs' bytes).
// The other formats (BC1-BC5...) have only the CPU encoder: every mode uses it for them.
// The calls are texconv's (Texconv/texconv.cpp): Compress(device, ..., flags, alphaWeight) on the GPU and
// Compress(..., flags, alphaThreshold) on the CPU - the flags as the caller gives them (texconv adds
// TEX_COMPRESS_PARALLEL for its CPU encoder; a caller that wants it passes it).
#pragma once

#include <cstddef>
#include "DirectXTex.h"

namespace DxtCodec
{
    enum class Encoder : int { Auto = 0, Cpu = 1, Gpu = 2 };

    /// <summary>Whether the process has a DirectCompute-capable D3D11 device (created on the first call).</summary>
    bool GpuAvailable() noexcept;

    /// <summary>Compress images (a mip chain / array) to format with the chosen encoder. alphaWeight: the GPU BC7
    /// encoder's alpha error weight (texconv -aw; DirectX::TEX_ALPHA_WEIGHT_DEFAULT = 1); alphaThreshold: the CPU
    /// encoder's (DirectX::TEX_THRESHOLD_DEFAULT). usedGpu (optional) tells which encoder ran.</summary>
    HRESULT Compress(const DirectX::Image* images, size_t count, const DirectX::TexMetadata& metadata, DXGI_FORMAT format,
                     DirectX::TEX_COMPRESS_FLAGS flags, float alphaWeight, float alphaThreshold, Encoder encoder,
                     DirectX::ScratchImage& out, bool* usedGpu = nullptr) noexcept;

    /// <summary>Decompress block-compressed images to format (DirectX::Decompress, the decoder texconv uses).</summary>
    HRESULT Decompress(const DirectX::Image* images, size_t count, const DirectX::TexMetadata& metadata, DXGI_FORMAT format,
                       DirectX::ScratchImage& out) noexcept;
    // ===============================================================================================
    // ⭐ LA LEY DE "A QUE FORMATO DESCOMPRIMIR UN BCn", ESCRITA UNA VEZ, CON EL USO DECLARADO.
    //
    // ⛔ Estaba transcripta TRES veces —acá, adentro de LoadTextures y adentro de ConvertForBitmap— y
    // las tres NO decian lo mismo. Al factorizarla aparecieron DOS ejes en los que los llamadores
    // difieren de verdad. Ninguno es accidental: cada uno responde a un requisito distinto, y por eso
    // van como USO declarado y no como cuatro copias sueltas.
    //
    //   EJE 1 · sRGB. `ConvertForBitmap` SIEMPRE quiere lineal: su destino es un Bitmap .NET BGRA8
    //   lineal. Los otros dos conservan el sRGB de la fuente.
    //
    //   EJE 2 · SIGNO. La tuberia de conversion tiene que preservar SNORM porque re-encodear es una
    //   transformacion FIEL: mandar SNORM a UNORM satura la mitad negativa a 0 y ademas recuantiza
    //   (127 pasos -> 255). Los dos caminos de DECODE, en cambio, tienen que aterrizar en UNORM, porque
    //   la tabla de canales del lado administrado (`FaceTintCpuCompositor.CanalesDelFormatoDecodificado`)
    //   conoce 49/61 (R8G8_UNORM / R8_UNORM) y NO conoce 51/63 (R8G8_SNORM / R8_SNORM) — ojo con
    //   confundirlos con 50/62, que son los UINT. Y es lo que esos dos caminos ya hacian en produccion.
    //
    // ⛔⛔ EL EJE DEL SIGNO LO ENCONTRO UN REVISOR, NO MI GATE. Yo habia unificado los tres a UNORM y lo
    // declare byte-neutro apoyandome en la fixture `huecos`... que digeria
    // `ConvertLoadedTextureToDdsBytes` con salida BC3. Con salida COMPRIMIDA la llave de la tabla es el
    // formato de SALIDA, asi que el eje de la ENTRADA nunca se tocaba: el gate estaba verde midiendo
    // otra cosa. El caso que si lo ejercita es entrada BCn SNORM con salida SIN COMPRIMIR, y ahora esta
    // en la fixture.
    // ===============================================================================================
    enum class UsoDeDescompresion
    {
        PipelineDeConversion,   // re-encode: fiel — conserva sRGB y conserva el signo
        ConsumoCpuOGl,          // LoadTextures: conserva sRGB, aterriza SNORM en UNORM
        BitmapAdministrado      // ConvertForBitmap: siempre lineal, aterriza SNORM en UNORM
    };
    DXGI_FORMAT FormatoDeDescompresion(DXGI_FORMAT srcFmt, UsoDeDescompresion uso) noexcept;

    /// <summary>The conversion of a texture to another format (DirectXTexWrapper's ConvertSubresources, and
    /// FabricGen's decode through DxtCodecC): a block-compressed input decompressed (to its canonical format,
    /// FormatoDeDescompresion PipelineDeConversion) when the output differs or mips are generated; the mips
    /// generated when asked (mipLevels 0: the full chain); then compressed (from the output's canonical format) or
    /// converted to outputFormat. failed names the step of an error.</summary>
    struct ConversionOptions
    {
        bool autoGenerateMipMaps = false;
        size_t mipLevels = 0;
        DirectX::TEX_FILTER_FLAGS filterFlags = DirectX::TEX_FILTER_DEFAULT;
        float alphaThreshold = DirectX::TEX_THRESHOLD_DEFAULT;
        DirectX::TEX_COMPRESS_FLAGS compressFlags = DirectX::TEX_COMPRESS_DEFAULT;
        float alphaWeight = DirectX::TEX_ALPHA_WEIGHT_DEFAULT;
        Encoder encoder = Encoder::Auto;
    };

    enum class ConversionStep : int { None = 0, Decompress = 1, MipMaps = 2, Canonical = 3, Compress = 4, Convert = 5 };

    HRESULT RunConversion(DirectX::ScratchImage& workingImage, DXGI_FORMAT outputFormat,
                          const ConversionOptions& options, ConversionStep& failed) noexcept;
}
