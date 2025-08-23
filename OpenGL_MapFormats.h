#pragma once

struct FormatMapping
{
    const char* dxgiName;
    bool isCompressed;
    unsigned int glInternalFormat;
    const char* glInternalName;
    unsigned int glPixelFormat;
    unsigned int glPixelType;
};

#ifdef __cplusplus
extern "C" {
#endif

    const FormatMapping* GetFormatMapping(int dxgiCode);

#ifdef __cplusplus
}
#endif

