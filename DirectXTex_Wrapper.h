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
        // Declaration only; implementation in .cpp
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
        // Declaration only; implementation in .cpp
        TextureLoaded();
    };

    /// <summary>Loader methods replacing P/Invoke.</summary>
    public ref class Loader
    {
    public:
        static List<TextureLoaded^>^ LoadTextures(array<array<Byte>^>^ ddsFiles, bool useCompress, bool forceOpenGL);
        static TextureLoaded^ ConvertForBitmap(array<Byte>^ ddsFile);
        // Construye y devuelve el header DDS (incluye DDS_HEADER y, si corresponde, DDS_HEADER_DXT10)
        static array<System::Byte>^ EncodeDDSHeader(int dxgiFormat,int width,int height,int arraySize,int mipLevels,bool isCubemap);
    };

} // namespace DirectXTexWrapperCLI

#endif // _MANAGED
