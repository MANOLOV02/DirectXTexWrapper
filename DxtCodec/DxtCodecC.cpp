// DxtCodecC — the C ABI of DxtCodec for native hosts (FabricGen loads DxtCodecC.dll with ctypes).
// Copyright (C) 2026  ManoloV02. GPL-3.0-or-later.
//
// A texture is a tight-packed 2D mip chain: level 0 first, each level's rows contiguous (DirectX::ComputePitch's row
// and slice pitch for its size and format); the caller allocates the output (its size: dxt_chain_bytes).
#include "DxtCodec.h"

#include <objbase.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
    // COM on the calling thread, as texconv has it (CoInitializeEx MULTITHREADED once, for the process's life, in its
    // wmain): DirectX::Convert goes through WIC for some format pairs (B8G8R8A8, B5G6R5, R8, R10G10B10A2, FP16 ->
    // RGBA8...) and WIC needs COM - a .NET host (DirectXTexWrapper) has it, a native host's thread (FabricGen's Python)
    // does not: E_NOINTERFACE (measured, decode_gate 6-oct-2026). Once per thread and never undone: DirectXTex keeps
    // its WIC factory process-wide (GetWICFactory), so uninitializing after a call left the next one on a released
    // factory (access violation, measured). A thread already in another apartment (RPC_E_CHANGED_MODE) has COM.
    HRESULT ThreadCom() noexcept
    {
        thread_local const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        return (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) ? S_OK : hr;
    }

    // the chain's images over the caller's bytes (no copy); false when the bytes do not match the chain's size
    bool ChainImages(const uint8_t* data, size_t bytes, int width, int height, int mips, DXGI_FORMAT format,
                     std::vector<DirectX::Image>& images, DirectX::TexMetadata& meta) noexcept
    {
        if (width <= 0 || height <= 0 || mips <= 0)
            return false;
        meta = {};
        meta.width = static_cast<size_t>(width);
        meta.height = static_cast<size_t>(height);
        meta.depth = 1;
        meta.arraySize = 1;
        meta.mipLevels = static_cast<size_t>(mips);
        meta.format = format;
        meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
        images.clear();
        size_t off = 0;
        size_t w = meta.width, h = meta.height;
        for (int m = 0; m < mips; ++m)
        {
            size_t row = 0, slice = 0;
            if (FAILED(DirectX::ComputePitch(format, w, h, row, slice)))
                return false;
            DirectX::Image img = {};
            img.width = w; img.height = h; img.format = format; img.rowPitch = row; img.slicePitch = slice;
            img.pixels = const_cast<uint8_t*>(data) + off;
            images.push_back(img);
            off += slice;
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
        }
        return off == bytes;
    }

    // the result's images in chain order into the caller's buffer, tight-packed
    HRESULT CopyOut(const DirectX::ScratchImage& img, uint8_t* out, size_t bytes) noexcept
    {
        size_t off = 0;
        const DirectX::TexMetadata& m = img.GetMetadata();
        for (size_t mip = 0; mip < m.mipLevels; ++mip)
        {
            const DirectX::Image* p = img.GetImage(mip, 0, 0);
            size_t row = 0, slice = 0;
            if (!p || FAILED(DirectX::ComputePitch(p->format, p->width, p->height, row, slice)))
                return E_FAIL;
            if (off + slice > bytes)
                return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            if (p->rowPitch == row)
                std::memcpy(out + off, p->pixels, slice);
            else
            {
                const size_t rows = slice / row;
                for (size_t r = 0; r < rows; ++r)
                    std::memcpy(out + off + r * row, p->pixels + r * p->rowPitch, row);
            }
            off += slice;
        }
        return off == bytes ? S_OK : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
}

extern "C"
{
    __declspec(dllexport) int dxt_gpu_available() noexcept
    {
        return DxtCodec::GpuAvailable() ? 1 : 0;
    }

    /// <summary>The bytes of a tight-packed mip chain (0 when the format or size is invalid).</summary>
    __declspec(dllexport) uint64_t dxt_chain_bytes(int width, int height, int mips, int format) noexcept
    {
        uint64_t total = 0;
        size_t w = static_cast<size_t>(width), h = static_cast<size_t>(height);
        for (int m = 0; m < mips; ++m)
        {
            size_t row = 0, slice = 0;
            if (FAILED(DirectX::ComputePitch(static_cast<DXGI_FORMAT>(format), w, h, row, slice)))
                return 0;
            total += slice;
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
        }
        return total;
    }

    /// <summary>DxtCodec::Compress of a mip chain (in_format) to out_format; encoder 0 Auto, 1 Cpu, 2 Gpu; used_gpu
    /// (optional) gets 1 when the GPU encoder ran. Returns an HRESULT.</summary>
    __declspec(dllexport) int32_t dxt_compress(int encoder, const uint8_t* src, uint64_t src_bytes, int width, int height,
                                               int mips, int in_format, int out_format, uint32_t flags,
                                               float alpha_weight, float alpha_threshold,
                                               uint8_t* dst, uint64_t dst_bytes, int* used_gpu) noexcept
    {
        if (const HRESULT com = ThreadCom(); FAILED(com))
            return com;
        std::vector<DirectX::Image> images;
        DirectX::TexMetadata meta;
        if (!ChainImages(src, static_cast<size_t>(src_bytes), width, height, mips, static_cast<DXGI_FORMAT>(in_format),
                         images, meta))
            return E_INVALIDARG;
        DirectX::ScratchImage out;
        bool gpu = false;
        HRESULT hr = DxtCodec::Compress(images.data(), images.size(), meta, static_cast<DXGI_FORMAT>(out_format),
                                        static_cast<DirectX::TEX_COMPRESS_FLAGS>(flags), alpha_weight, alpha_threshold,
                                        static_cast<DxtCodec::Encoder>(encoder), out, &gpu);
        if (used_gpu)
            *used_gpu = gpu ? 1 : 0;
        if (FAILED(hr))
            return hr;
        return CopyOut(out, dst, static_cast<size_t>(dst_bytes));
    }

    /// <summary>The DDS header of a 2D mip chain (DirectX::EncodeDDSHeader with DDS_FLAGS_NONE: the header texconv's
    /// SaveToDDSFile writes - a legacy FourCC where one exists, DX10 otherwise) into dst; header_bytes gets its size.
    /// Returns an HRESULT.</summary>
    __declspec(dllexport) int32_t dxt_dds_header(int width, int height, int mips, int format, uint8_t* dst,
                                                 uint64_t dst_bytes, uint64_t* header_bytes) noexcept
    {
        DirectX::TexMetadata meta = {};
        meta.width = static_cast<size_t>(width);
        meta.height = static_cast<size_t>(height);
        meta.depth = 1;
        meta.arraySize = 1;
        meta.mipLevels = static_cast<size_t>(mips);
        meta.format = static_cast<DXGI_FORMAT>(format);
        meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
        size_t required = 0;
        HRESULT hr = DirectX::EncodeDDSHeader(meta, DirectX::DDS_FLAGS_NONE, dst, static_cast<size_t>(dst_bytes), required);
        if (header_bytes)
            *header_bytes = required;
        return hr;
    }

    /// <summary>A DDS file (DirectX::LoadFromDDSFile, DDS_FLAGS_NONE as DirectXTexWrapper loads) converted to format by
    /// DxtCodec::RunConversion - DirectXTexWrapper's conversion: a BCn decompressed to its canonical format, then
    /// converted (TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT) - every image of the file (array items, mips) tight-packed in
    /// the file's order (item 0's chain first) into dst. With dst null only the sizes are given back (bytes: dst's size).
    /// Returns an HRESULT; step (optional) the ConversionStep that failed.</summary>
    __declspec(dllexport) int32_t dxt_decode_dds(const wchar_t* path, int format, int* width, int* height, int* mips,
                                                 int* items, uint8_t* dst, uint64_t dst_bytes, uint64_t* bytes,
                                                 int* step) noexcept
    {
        if (const HRESULT com = ThreadCom(); FAILED(com))
            return com;
        if (step)
            *step = 0;
        DirectX::TexMetadata info;
        DirectX::ScratchImage image;
        HRESULT hr = DirectX::LoadFromDDSFile(path, DirectX::DDS_FLAGS_NONE, &info, image);
        if (FAILED(hr))
            return hr;
        if (info.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || info.depth != 1)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        if (!dst)
        {
            uint64_t total = 0;
            size_t w = info.width, h = info.height;
            for (size_t m = 0; m < info.mipLevels; ++m)
            {
                size_t row = 0, slice = 0;
                if (FAILED(DirectX::ComputePitch(static_cast<DXGI_FORMAT>(format), w, h, row, slice)))
                    return E_INVALIDARG;
                total += slice;
                w = w > 1 ? w / 2 : 1;
                h = h > 1 ? h / 2 : 1;
            }
            *width = static_cast<int>(info.width); *height = static_cast<int>(info.height);
            *mips = static_cast<int>(info.mipLevels); *items = static_cast<int>(info.arraySize);
            *bytes = total * info.arraySize;
            return S_OK;
        }
        DxtCodec::ConversionOptions options;
        options.mipLevels = info.mipLevels;
        DxtCodec::ConversionStep failed = DxtCodec::ConversionStep::None;
        hr = DxtCodec::RunConversion(image, static_cast<DXGI_FORMAT>(format), options, failed);
        if (step)
            *step = static_cast<int>(failed);
        if (FAILED(hr))
            return hr;
        const DirectX::TexMetadata& m = image.GetMetadata();
        size_t off = 0;
        for (size_t item = 0; item < m.arraySize; ++item)
            for (size_t level = 0; level < m.mipLevels; ++level)
            {
                const DirectX::Image* p = image.GetImage(level, item, 0);
                size_t row = 0, slice = 0;
                if (!p || FAILED(DirectX::ComputePitch(p->format, p->width, p->height, row, slice)))
                    return E_FAIL;
                if (off + slice > dst_bytes)
                    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                const size_t rows = slice / row;
                for (size_t r = 0; r < rows; ++r)
                    std::memcpy(dst + off + r * row, p->pixels + r * p->rowPitch, row);
                off += slice;
            }
        *width = static_cast<int>(m.width); *height = static_cast<int>(m.height);
        *mips = static_cast<int>(m.mipLevels); *items = static_cast<int>(m.arraySize);
        if (bytes)
            *bytes = off;
        return off == dst_bytes ? S_OK : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    /// <summary>DxtCodec::Decompress of a block-compressed mip chain (in_format) to out_format. Returns an HRESULT.</summary>
    __declspec(dllexport) int32_t dxt_decompress(const uint8_t* src, uint64_t src_bytes, int width, int height, int mips,
                                                 int in_format, int out_format, uint8_t* dst, uint64_t dst_bytes) noexcept
    {
        if (const HRESULT com = ThreadCom(); FAILED(com))
            return com;
        std::vector<DirectX::Image> images;
        DirectX::TexMetadata meta;
        if (!ChainImages(src, static_cast<size_t>(src_bytes), width, height, mips, static_cast<DXGI_FORMAT>(in_format),
                         images, meta))
            return E_INVALIDARG;
        DirectX::ScratchImage out;
        HRESULT hr = DxtCodec::Decompress(images.data(), images.size(), meta, static_cast<DXGI_FORMAT>(out_format), out);
        if (FAILED(hr))
            return hr;
        return CopyOut(out, dst, static_cast<size_t>(dst_bytes));
    }
}
