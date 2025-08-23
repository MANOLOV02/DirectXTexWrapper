#include <unordered_map>
#include "OpenGL_MapFormats.h"
#include "DirectXTex.h"


static const std::unordered_map<int, FormatMapping> dxgiFormatMap = {
    {0,  {"DXGI_FORMAT_UNKNOWN", false, 0x0000, "Incompatible", 0x0000, 0x0000}},               // Unknown / undefined format
    {1,  {"DXGI_FORMAT_R32G32B32A32_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // No OpenGL equivalent
    {2,  {"DXGI_FORMAT_R32G32B32A32_FLOAT", false, 0x8814, "GL_RGBA32F", 0x1908, 0x1406}},       // RGBA 32-bit float
    {3,  {"DXGI_FORMAT_R32G32B32A32_UINT", false, 0x8D70, "GL_RGBA32UI", 0x1908, 0x1405}},       // RGBA 32-bit unsigned int
    {4,  {"DXGI_FORMAT_R32G32B32A32_SINT", false, 0x8D82, "GL_RGBA32I", 0x1908, 0x1404}},        // RGBA 32-bit signed int
    {5,  {"DXGI_FORMAT_R32G32B32_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},    // Typeless format - no GL mapping
    {6,  {"DXGI_FORMAT_R32G32B32_FLOAT", false, 0x8815, "GL_RGB32F", 0x1907, 0x1406}},           // RGB 32-bit float
    {7, {"DXGI_FORMAT_R32G32B32_UINT", false, 0x8D71, "GL_RGB32UI", 0x1907, 0x1405}},       // RGB 96-bit unsigned int
    {8, {"DXGI_FORMAT_R32G32B32_SINT", false, 0x8D83, "GL_RGB32I", 0x1907, 0x1404}},        // RGB 96-bit signed int
    {9,  {"DXGI_FORMAT_R16G16B16A16_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Typeless format - no GL mapping
    {10, {"DXGI_FORMAT_R16G16B16A16_FLOAT", false, 0x881F, "GL_RGBA16F", 0x1908, 0x140B}},       // RGBA 16-bit float
    {11, {"DXGI_FORMAT_R16G16B16A16_UNORM", false, 0x805B, "GL_RGBA16", 0x1908, 0x1403}},        // RGBA 16-bit unsigned normalized
    {12, {"DXGI_FORMAT_R16G16B16A16_UINT", false, 0x8D76, "GL_RGBA16UI", 0x1908, 0x1403}}, // RGBA 16-bit unsigned int (duplicate)
    {13, {"DXGI_FORMAT_R16G16B16A16_SNORM", false, 0x8F9B, "GL_RGBA16_SNORM", 0x1908, 0x1402}},  // RGBA 16-bit signed normalized
    {14, {"DXGI_FORMAT_R16G16B16A16_SINT", false, 0x8D88, "GL_RGBA16I", 0x1908, 0x1404}}, // RGBA 16-bit signed int
    {15, {"DXGI_FORMAT_R32G32_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Typeless - no GL mapping
    {16, {"DXGI_FORMAT_R32G32_FLOAT", false, 0x8230, "GL_RG32F", 0x8227, 0x1406}},        // RG 32-bit float
    {17, {"DXGI_FORMAT_R32G32_UINT", false, 0x823C, "GL_RG32UI", 0x8227, 0x1405}},        // RG 32-bit unsigned int
    {18, {"DXGI_FORMAT_R32G32_SINT", false, 0x823B, "GL_RG32I", 0x8227, 0x1404}},         // RG 32-bit signed int
    {19, {"DXGI_FORMAT_R32G8X24_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Typeless forma
    {20, {"DXGI_FORMAT_D32_FLOAT_S8X24_UINT", false, 0x8CAD, "GL_DEPTH32F_STENCIL8", 0x84F9, 0x0000}}, // Depth 32-bit float + stencil 8-bit
    {21, {"DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Typeless depth-stencil - no GL mapping
    {22, {"DXGI_FORMAT_X32_TYPELESS_G8X24_UINT", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Typeless stencil - no GL equivalent
    {23, {"DXGI_FORMAT_R10G10B10A2_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},  // Typeless format - no GL mapping
    {24, {"DXGI_FORMAT_R10G10B10A2_UNORM", false, 0x8059, "GL_RGB10_A2", 0x1908, 0x8036}},     // RGBA 10-bit unsigned normalized
    {25, {"DXGI_FORMAT_R10G10B10A2_UINT", false, 0x906F, "GL_RGB10_A2UI", 0x1908, 0x8036}},            // RGBA 10-bit unsigned int
    {26, {"DXGI_FORMAT_R11G11B10_FLOAT", false, 0x8C3A, "GL_R11F_G11F_B10F", 0x1907, 0x1401}}, // RGB 11-bit float (shared exponent) HDR
    {27, {"DXGI_FORMAT_R8G8B8A8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // No hay equivalente OpenGL
    {28, {"DXGI_FORMAT_R8G8B8A8_UNORM", false, 0x8058, "GL_RGBA8", 0x1908, 0x1401}},             // RGBA 8-bit unsigned normalized
    {29, {"DXGI_FORMAT_R8G8B8A8_UNORM_SRGB", false, 0x8C43, "GL_SRGB8_ALPHA8", 0x1908, 0x1401}}, // RGBA 8-bit sRGB normalized
    {30, {"DXGI_FORMAT_R8G8B8A8_UINT", false, 0x8D7C, "GL_RGBA8UI", 0x1908, 0x1401}},       // RGBA 8-bit unsigned int
    {31, {"DXGI_FORMAT_R8G8B8A8_SNORM", false, 0x8F97, "GL_RGBA8_SNORM", 0x1908, 0x1401}}, // RGBA 8-bit signed normalized
    {32, {"DXGI_FORMAT_R8G8B8A8_SINT", false, 0x8D8E, "GL_RGBA8I", 0x1908, 0x1400}},        // RGBA 8-bit signed int
    {33, {"DXGI_FORMAT_R16G16_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},      // Typeless RG 16-bit
    {34, {"DXGI_FORMAT_R16G16_FLOAT", false, 0x822F, "GL_RG16F", 0x8227, 0x140B}}, // RG 16-bit float
    {35, {"DXGI_FORMAT_R16G16_UNORM", false, 0x8054, "GL_RG16", 0x8227, 0x1403}}, // RG 16-bit unsigned normalized
    {36, {"DXGI_FORMAT_R16G16_UINT", false, 0x8D77, "GL_RG16UI", 0x8227, 0x1403}}, // RG 16-bit unsigned int
    {37, {"DXGI_FORMAT_R16G16_SNORM", false, 0x8F3B, "GL_RG16_SNORM", 0x8227, 0x1402}}, // RG + GL_SHORT
    {38, {"DXGI_FORMAT_R16G16_SINT", false, 0x8D89, "GL_RG16I", 0x8227, 0x1402}},  // RG 16-bit signed int
    {39, {"DXGI_FORMAT_R32_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},         // Typeless R32 - no GL mapping
    {40, {"DXGI_FORMAT_D32_FLOAT", false, 0x81A7, "GL_DEPTH_COMPONENT32F", 0x1902, 0x1406}},     // Depth 32-bit float
    {41, {"DXGI_FORMAT_R32_FLOAT", false, 0x822E, "GL_R32F", 0x8229, 0x1406}}, // Red float
    {42, {"DXGI_FORMAT_R32_UINT", false, 0x8236, "GL_R32UI", 0x8229, 0x1405}}, // Red uint
    {43, {"DXGI_FORMAT_R32_SINT", false, 0x8230, "GL_R32I", 0x8229, 0x1404}}, // Red int
    {44, {"DXGI_FORMAT_R24G8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},   // No OpenGL equivalent
    {45, {"DXGI_FORMAT_D24_UNORM_S8_UINT", false, 0x88F0, "GL_DEPTH24_STENCIL8", 0x84F9, 0x0000}}, // Depth 24-bit + stencil 8-bit
    {46, {"DXGI_FORMAT_R24_UNORM_X8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // No hay equivalente directo en OpenGL
    {47, {"DXGI_FORMAT_X24_TYPELESS_G8_UINT", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Typeless stencil - no GL equivalent
    {48, {"DXGI_FORMAT_R8G8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},        // Typeless RG 8-bit
    {49, {"DXGI_FORMAT_R8G8_UNORM", false, 0x822B, "GL_RG8", 0x8227, 0x1401}},                 // RG 8-bit unsigned normalized
    {50, {"DXGI_FORMAT_R8G8_UINT", false, 0x8238, "GL_RG8UI", 0x8227, 0x1401}},                // RG 8-bit unsigned int
    {51, {"DXGI_FORMAT_R8G8_SNORM", false, 0x8F95, "GL_RG8_SNORM", 0x8227, 0x1401}}, // RG + GL_BYTE
    {52, {"DXGI_FORMAT_R8G8_SINT", false, 0x8237, "GL_RG8I", 0x8227, 0x1400}},                 // RG 8-bit signed int
    {53, {"DXGI_FORMAT_R16_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},         // Typeless R 16-bit
    {54, {"DXGI_FORMAT_R16_FLOAT", false, 0x822D, "GL_R16F", 0x8229, 0x140B}},    // R 16-bit float
    {55, {"DXGI_FORMAT_D16_UNORM", false, 0x81A5, "GL_DEPTH_COMPONENT16", 0x1902, 0x1403}}, // Depth 16-bit unsigned normalized
    {56, {"DXGI_FORMAT_R16_UNORM", false, 0x822A, "GL_R16", 0x8229, 0x1403}},             // R 16-bit unsigned normalized
    {57, {"DXGI_FORMAT_R16_UINT", false, 0x8234, "GL_R16UI", 0x8229, 0x1403}},            // R 16-bit unsigned int
    {58, {"DXGI_FORMAT_R16_SNORM", false, 0x8F98, "GL_R16_SNORM", 0x8229, 0x1402}},       // R 16-bit signed normalized
    {59, {"DXGI_FORMAT_R16_SINT", false, 0x8233, "GL_R16I", 0x8229, 0x1402}},             // R 16-bit signed int
    {60, {"DXGI_FORMAT_R8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}},     // Typeless R 8-bit
    {61, {"DXGI_FORMAT_R8_UNORM", false, 0x8229, "GL_R8", 0x8229, 0x1401}},       // R 8-bit unsigned normalized
    {62, {"DXGI_FORMAT_R8_UINT", false, 0x8232, "GL_R8UI", 0x8229, 0x1401}},      // R 8-bit unsigned int
    {63, {"DXGI_FORMAT_R8_SNORM", false, 0x8F94, "GL_R8_SNORM", 0x8229, 0x1400}}, // R 8-bit signed normalized
    {64, {"DXGI_FORMAT_R8_SINT", false, 0x8231, "GL_R8I", 0x8229, 0x1400}},       // R 8-bit signed int
    {65, {"DXGI_FORMAT_A8_UNORM", false, 0x803C, "GL_ALPHA8", 0x1906, 0x1401}},           // Alpha 8-bit normalized
    {66, {"DXGI_FORMAT_R1_UNORM", false, 0x0000, "Incompatible", 0x0000, 0x0000}},        // 1-bit format - unsupported in GL
    {67, {"DXGI_FORMAT_R9G9B9E5_SHAREDEXP", false, 0x8C3D, "GL_RGB9_E5", 0x1907, 0x1401}}, // RGB 9-bit + 5-bit shared exponent
    {68, {"DXGI_FORMAT_R8G8_B8G8_UNORM", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Packed format - unsupported in GL
    {69, {"DXGI_FORMAT_G8R8_G8B8_UNORM", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // No equivalente OpenGL estándar
    {70, {"DXGI_FORMAT_BC1_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}},                  // BC1 typeless - no GL mapping
    {71, {"DXGI_FORMAT_BC1_UNORM", true, 0x83F1, "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT", 0x0000, 0x0000}}, // BC1 (DXT1) compressed
    {72, {"DXGI_FORMAT_BC1_UNORM_SRGB", true, 0x8C4C, "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT", 0x0000, 0x0000}}, // BC1 (DXT1) SRGB compressed
    {73, {"DXGI_FORMAT_BC2_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}},                  // BC2 typeless - no GL mapping
    {74, {"DXGI_FORMAT_BC2_UNORM", true, 0x83F2, "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT", 0x0000, 0x0000}}, // BC2 (DXT3) compressed
    {75, {"DXGI_FORMAT_BC2_UNORM_SRGB", true, 0x8C4D, "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT", 0x0000, 0x0000}}, // BC2 (DXT3) SRGB compressed
    {76, {"DXGI_FORMAT_BC3_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}},                  // BC3 typeless - no GL mapping
    {77, {"DXGI_FORMAT_BC3_UNORM", true, 0x83F3, "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT", 0x0000, 0x0000}}, // BC3 (DXT5) compressed
    {78, {"DXGI_FORMAT_BC3_UNORM_SRGB", true, 0x8C4E, "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT", 0x0000, 0x0000}}, // BC3 (DXT5) SRGB compressed
    {79, {"DXGI_FORMAT_BC4_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}},                  // BC4 typeless - no GL mapping
    {80, {"DXGI_FORMAT_BC4_UNORM", true, 0x8DBB, "GL_COMPRESSED_RED_RGTC1", 0x0000, 0x0000}},     // BC4 compressed format
    {81, {"DXGI_FORMAT_BC4_SNORM", true, 0x8DBD, "GL_COMPRESSED_SIGNED_RED_RGTC1", 0x0000, 0x0000}},             // BC4 signed normalized
    {82, {"DXGI_FORMAT_BC5_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Tipeless y comprimido
    {83, {"DXGI_FORMAT_BC5_UNORM", true, 0x8DBD, "GL_COMPRESSED_RG_RGTC2", 0x0000, 0x0000}},      // BC5 compressed format
    {84, {"DXGI_FORMAT_BC5_SNORM", true, 0x8DBE, "GL_COMPRESSED_SIGNED_RG_RGTC2", 0x0000, 0x0000}}, // BC5 signed normalized
    {85, {"DXGI_FORMAT_B5G6R5_UNORM", false, 0x8D62, "GL_RGB565", 0x1907, 0x8363}},       // RGB 5-6-5 format
    {86, {"DXGI_FORMAT_B5G5R5A1_UNORM", false, 0x8057, "GL_RGB5_A1", 0x1908, 0x8034}},    // RGBA 5-5-5-1 format
    //{87, {"DXGI_FORMAT_B8G8R8A8_UNORM", false, 0x0000, "GL_BGRA8_EXT", 0x80E1, 0x1401}},    // BGRA 8-bit unsigned normalized
    {87, {"DXGI_FORMAT_B8G8R8A8_UNORM", false, 0x93A1, "GL_BGRA8_EXT", 0x80E1, 0x1401}},    // BGRA 8-bit unsigned normalized
    {88, {"DXGI_FORMAT_B8G8R8X8_UNORM", false, 0x8058, "GL_RGBA8", 0x80E0, 0x1401}},      // BGRX 8-bit normalized (used as RGBA8)
    {89, {"DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // XR Bias - no GL equivalent
    {90, {"DXGI_FORMAT_B8G8R8A8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Tipeless
    {91, {"DXGI_FORMAT_B8G8R8A8_UNORM_SRGB", false, 0x8C41, "GL_SRGB8_ALPHA8_EXT", 0x80E1, 0x1401}}, // BGRA 8-bit sRGB normalized
    {92, {"DXGI_FORMAT_B8G8R8X8_TYPELESS", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Tipeless
    {93, {"DXGI_FORMAT_B8G8R8X8_UNORM_SRGB", false, 0x8C41, "GL_SRGB8_ALPHA8_EXT", 0x80E0, 0x1401}}, // BGRX sRGB (approx)
    {94, {"DXGI_FORMAT_BC6H_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}},                          // BC6H typeless - no direct GL equivalent
    {95, {"DXGI_FORMAT_BC6H_UF16", true, 0x8E8F, "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB", 0x0000, 0x0000}}, // Compressed BPTC
    {96, {"DXGI_FORMAT_BC6H_SF16", true, 0x8F3E, "GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB", 0x0000, 0x0000}},   // BC6H signed float
    {97, {"DXGI_FORMAT_BC7_TYPELESS", true, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Tipeless
    {98, {"DXGI_FORMAT_BC7_UNORM", true, 0x8E8C, "GL_COMPRESSED_RGBA_BPTC_UNORM_ARB", 0x0000, 0x0000}}, // BC7 compressed format
//    {98, {"DXGI_FORMAT_BC7_UNORM", true, 0x8E8C, "GL_COMPRESSED_RGBA_BPTC_UNORM_ARB", 0x0000, 0x0000}}, // BC7 compressed format
//    {98, {"DXGI_FORMAT_BC7_UNORM", true, 0x0000, "Incompatible", 0x0000, 0x0000}}, // BC7 compressed format
    {99, {"DXGI_FORMAT_BC7_UNORM_SRGB", true, 0x8E8D, "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB", 0x0000, 0x0000}}, // BC7 sRGB compressed format
    {100, {"DXGI_FORMAT_AYUV", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Formato video YUV
    {101, {"DXGI_FORMAT_Y410", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video
    {102, {"DXGI_FORMAT_Y416", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video
    {103, {"DXGI_FORMAT_NV12", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video planar
    {104, {"DXGI_FORMAT_P010", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video
    {105, {"DXGI_FORMAT_P016", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video
    {106, {"DXGI_FORMAT_420_OPAQUE", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video
    {107, {"DXGI_FORMAT_YUY2", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Packed YUV
    {108, {"DXGI_FORMAT_Y210", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Packed YUV 10-bit
    {109, {"DXGI_FORMAT_Y216", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Packed YUV 16-bit
    {110, {"DXGI_FORMAT_NV11", false, 0x0000, "Incompatible", 0x0000, 0x0000}}, // Video format
    {111, {"DXGI_FORMAT_AI44", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Legacy palettized format - not supported
    {112, {"DXGI_FORMAT_IA44", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Legacy palettized format - not supported
    {113, {"DXGI_FORMAT_P8", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Palettized 8-bit (no soporte OpenGL moderno)
    {114, {"DXGI_FORMAT_A8P8", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Alpha + palette - not supported
    {115, {"DXGI_FORMAT_B4G4R4A4_UNORM", false, 0x8033, "GL_RGBA4", 0x1908, 0x8033} }, // RGBA 4-bit each channel
    {130, {"DXGI_FORMAT_P208", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Video format - not OpenGL compatible
    {131, {"DXGI_FORMAT_V208", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Video format - not OpenGL compatible
    {132, {"DXGI_FORMAT_V408", false, 0x0000, "Incompatible", 0x0000, 0x0000} }, // Video format - not OpenGL compatible

};

const FormatMapping* GetFormatMapping(int dxgiCode)
{
    auto it = dxgiFormatMap.find(dxgiCode);
    return (it != dxgiFormatMap.end()) ? &it->second : nullptr;
}


