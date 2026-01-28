#include "engine/assets/TextureLoader.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <lunasvg.h>

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <stb_image.h>

using Microsoft::WRL::ComPtr;

namespace vkengine {
namespace {

std::string readToken(std::istream& stream)
{
    std::string token;
    while (stream >> token) {
        if (!token.empty() && token[0] == '#') {
            stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        return token;
    }
    throw std::runtime_error("Unexpected end of file while parsing PPM texture");
}

std::uint8_t normalizeChannel(int value, int maxValue)
{
    value = std::clamp(value, 0, maxValue);
    if (maxValue == 255) {
        return static_cast<std::uint8_t>(value);
    }
    const float scaled = static_cast<float>(value) / static_cast<float>(maxValue);
    return static_cast<std::uint8_t>(std::clamp(scaled * 255.0f, 0.0f, 255.0f));
}

void flipRows(TextureData& data)
{
    if (data.height <= 1) {
        return;
    }
    const std::size_t rowStride = static_cast<std::size_t>(data.width) * data.channels;
    std::vector<std::uint8_t> temp(rowStride);
    for (std::uint32_t y = 0; y < data.height / 2; ++y) {
        auto* top = data.pixels.data() + y * rowStride;
        auto* bottom = data.pixels.data() + (data.height - 1 - y) * rowStride;
        std::memcpy(temp.data(), top, rowStride);
        std::memcpy(top, bottom, rowStride);
        std::memcpy(bottom, temp.data(), rowStride);
    }
}

} // namespace

TextureData loadWithStb(const std::filesystem::path& path, bool flipVertical)
{
    // WIC-based loader to avoid stb crashes on some JPGs/PNGs
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool alreadyInitializedDifferentMode = (hr == RPC_E_CHANGED_MODE);
    const bool weInitializedCom = SUCCEEDED(hr);
    const bool comUsable = weInitializedCom || alreadyInitializedDifferentMode;
    if (!comUsable) {
        throw std::runtime_error("Failed to initialize COM for image loading");
    }

    struct ComScope {
        bool active{false};
        ~ComScope() {
            if (active) {
                CoUninitialize();
            }
        }
    } comScope;
    comScope.active = weInitializedCom;

    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        if (weInitializedCom) CoUninitialize();
        throw std::runtime_error("Failed to create WIC imaging factory");
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to decode image: cannot open file");
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to decode image frame");
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create format converter");
    }

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to convert image to RGBA");
    }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        throw std::runtime_error("Decoded image has invalid dimensions");
    }

    const UINT stride = width * 4;
    const UINT bufferSize = stride * height;
    std::vector<stbi_uc> buffer(bufferSize);
    hr = converter->CopyPixels(nullptr, stride, bufferSize, buffer.data());
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to copy image pixels");
    }

    if (flipVertical && height > 1) {
        std::vector<stbi_uc> row(stride);
        for (UINT yRow = 0; yRow < height / 2; ++yRow) {
            stbi_uc* top = buffer.data() + yRow * stride;
            stbi_uc* bottom = buffer.data() + (height - 1 - yRow) * stride;
            std::memcpy(row.data(), top, stride);
            std::memcpy(top, bottom, stride);
            std::memcpy(bottom, row.data(), stride);
        }
    }

    TextureData texture{};
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.channels = 4;
    texture.pixels.assign(buffer.begin(), buffer.end());

    return texture;
}

TextureData loadSvgWithLuna(const std::filesystem::path& path, bool flipVertical)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open SVG file: " + path.string());
    }
    std::string svgData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (svgData.empty()) {
        throw std::runtime_error("SVG file is empty: " + path.string());
    }

    auto document = lunasvg::Document::loadFromData(svgData);
    if (!document) {
        throw std::runtime_error("Failed to parse SVG: " + path.string());
    }

    int width = static_cast<int>(document->width());
    int height = static_cast<int>(document->height());
    if (width <= 0 || height <= 0) {
        width = 512;
        height = 512;
    }

    lunasvg::Bitmap bitmap = document->renderToBitmap(width, height);
    if (!bitmap.valid()) {
        throw std::runtime_error("Failed to render SVG: " + path.string());
    }

    TextureData texture{};
    texture.width = static_cast<std::uint32_t>(bitmap.width());
    texture.height = static_cast<std::uint32_t>(bitmap.height());
    texture.channels = 4;
    const std::size_t byteCount = static_cast<std::size_t>(texture.width) * texture.height * 4;
    texture.pixels.assign(bitmap.data(), bitmap.data() + byteCount);

    if (flipVertical) {
        flipRows(texture);
    }

    return texture;
}

TextureData loadTexture(const std::filesystem::path& path, bool flipVertical)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Texture file does not exist: " + path.string());
    }

    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };

    const std::string extension = toLower(path.extension().string());

    // Fast path for common formats via WIC (jpg/png/bmp/etc.)
    if (extension != ".ppm" && extension != ".pnm") {
        if (extension == ".svg") {
            return loadSvgWithLuna(path, flipVertical);
        }
        return loadWithStb(path, flipVertical);
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open texture file: " + path.string());
    }

    const std::string magic = readToken(stream);
    if (magic != "P3" && magic != "P6") {
        stream.close();
        // Fall back to WIC loader for other masquerading formats
        return loadWithStb(path, flipVertical);
    }
    const bool ascii = magic == "P3";

    const int width = std::stoi(readToken(stream));
    const int height = std::stoi(readToken(stream));
    const int maxValue = std::stoi(readToken(stream));
    if (width <= 0 || height <= 0 || maxValue <= 0) {
        throw std::runtime_error("Invalid PPM header in texture: " + path.string());
    }

    TextureData texture{};
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.channels = 4;
    texture.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * texture.channels);

    if (ascii) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++i) {
            const int r = std::stoi(readToken(stream));
            const int g = std::stoi(readToken(stream));
            const int b = std::stoi(readToken(stream));
            const std::size_t offset = i * 4;
            texture.pixels[offset + 0] = normalizeChannel(r, maxValue);
            texture.pixels[offset + 1] = normalizeChannel(g, maxValue);
            texture.pixels[offset + 2] = normalizeChannel(b, maxValue);
            texture.pixels[offset + 3] = 255;
        }
    } else {
        int nextChar = stream.peek();
        while (std::isspace(nextChar)) {
            stream.get();
            nextChar = stream.peek();
        }

        const std::size_t expectedBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
        std::vector<std::uint8_t> rgb(expectedBytes);
        stream.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
        if (stream.gcount() != static_cast<std::streamsize>(rgb.size())) {
            throw std::runtime_error("Unexpected EOF while reading binary PPM: " + path.string());
        }

        for (std::size_t i = 0; i < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++i) {
            const std::size_t rgbOffset = i * 3;
            const std::size_t dstOffset = i * 4;
            texture.pixels[dstOffset + 0] = normalizeChannel(rgb[rgbOffset + 0], maxValue);
            texture.pixels[dstOffset + 1] = normalizeChannel(rgb[rgbOffset + 1], maxValue);
            texture.pixels[dstOffset + 2] = normalizeChannel(rgb[rgbOffset + 2], maxValue);
            texture.pixels[dstOffset + 3] = 255;
        }
    }

    if (flipVertical) {
        flipRows(texture);
    }

    return texture;
}

} // namespace vkengine
