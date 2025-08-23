#pragma once

struct FallbackMapping
{
    const char* originalName;
    int fallbackCode;
    const char* fallbackName;
};

#ifdef __cplusplus
extern "C" {
#endif

    int ResolveFallback(int dxgiCode);

#ifdef __cplusplus
}
#endif
