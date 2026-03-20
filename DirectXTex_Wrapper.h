#pragma once

// DirectXTex_Wrapper.h - C++/CLI wrapper declarations

#include <cstdint>
#include "OpenGL_MapFormats.h"     // Mappings for GL formats
#include "DirectXTex.h"            // Core DirectXTex API

#ifdef _MANAGED

#include <vcclr.h>
#include <msclr/marshal_cppstd.h>
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace DirectXTexWrapperCLI
{

    /// <summary>Managed representation of a single mipmap level.</summary>
    public ref class TextureLevel
    {
    public:
        array<Byte>^ Data;
        int Width;
        int Height;
        TextureLevel(array<Byte>^ data, int width, int height);
    };

    /// <summary>Managed representation of a loaded texture.</summary>
    public ref class TextureLoaded
    {
    public:
        List<TextureLevel^>^ Levels;
        bool IsCompressedGL;
        bool Loaded;
        bool IsCubemap;
        int DxgiCodeOriginal;
        int DxgiCodeFinal;
        int Faces;
        int Miplevels;
        unsigned int GlInternalFormat;
        unsigned int GlPixelFormat;
        unsigned int GlPixelType;
        TextureLoaded();
    };

    /// <summary>
    /// Managed representation of an explicit texture subresource.
    /// Used by ConvertSubresources to remove ambiguity around mipmaps and array faces.
    /// </summary>
    public ref class TextureSubresource
    {
    public:
        array<Byte>^ Data;
        int Width;
        int Height;
        int RowPitch;
        int SlicePitch;
        int MipLevel;
        int ArrayIndex;
        TextureSubresource();
        TextureSubresource(array<Byte>^ data, int width, int height, int rowPitch, int slicePitch, int mipLevel, int arrayIndex);
    };

    /// <summary>
    /// Explicit conversion request for subresource-based texture conversions.
    /// Supports Texture2D, Texture2DArray and Cubemap.
    /// Subresources must be ordered as mip-major and then array/face-major.
    /// If AutoGenerateMipMaps is true, only the base subresources are required and MipLevels can be 0 for the full chain.
    /// </summary>
    public ref class TextureConversionRequest
    {
    public:
        array<TextureSubresource^>^ Subresources;
        int Width;
        int Height;
        int InputDxgiFormat;
        int OutputDxgiFormat;
        int MipLevels;
        int ArraySize;
        bool IsCubemap;
        bool AutoGenerateMipMaps;
        int FilterFlags;
        int CompressFlags;
        float AlphaThreshold;
        TextureConversionRequest();
    };

    /// <summary>
    /// Managed conversion result returned by ConvertSubresources.
    /// Output data is returned tight-packed and in mip-major order.
    /// </summary>
    public ref class TextureConversionResult
    {
    public:
        array<TextureSubresource^>^ Subresources;
        int Width;
        int Height;
        int DxgiFormat;
        int MipLevels;
        int ArraySize;
        bool IsCubemap;
        TextureConversionResult();
    };

    /// <summary>Loader methods replacing P/Invoke.</summary>
    public ref class Loader
    {
    public:
        static List<TextureLoaded^>^ LoadTextures(array<array<Byte>^>^ ddsFiles, bool useCompress, bool forceOpenGL);
        static TextureLoaded^ ConvertForBitmap(array<Byte>^ ddsFile);
        static TextureConversionResult^ ConvertSubresources(TextureConversionRequest^ request);
        static array<System::Byte>^ EncodeDDSHeader(int dxgiFormat, int width, int height, int arraySize, int mipLevels, bool isCubemap);
    };

} // namespace DirectXTexWrapperCLI

#endif // _MANAGED
