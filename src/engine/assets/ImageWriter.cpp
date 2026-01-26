#include "engine/assets/ImageWriter.hpp"

#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#endif

namespace vkengine {

bool writeJpeg(const std::filesystem::path& path,
               int width,
               int height,
               const std::vector<std::uint8_t>& rgbaPixels,
               int quality)
{
#if defined(_WIN32)
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (rgbaPixels.size() < static_cast<std::size_t>(width * height * 4)) {
        return false;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool alreadyInitializedDifferentMode = (hr == RPC_E_CHANGED_MODE);
    const bool weInitializedCom = SUCCEEDED(hr);
    if (!weInitializedCom && !alreadyInitializedDifferentMode) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    const auto widePath = path.wstring();
    hr = stream->InitializeFromFilename(widePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    if (props) {
        PROPBAG2 option{};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value{};
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = std::clamp(static_cast<float>(quality) / 100.0f, 0.1f, 1.0f);
        props->Write(1, &option, &value);
        VariantClear(&value);
    }

    hr = frame->Initialize(props.Get());
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    const UINT stride = static_cast<UINT>(width) * 3;
    std::vector<std::uint8_t> bgr(static_cast<std::size_t>(stride) * height);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = rgbaPixels.data() + static_cast<std::size_t>(y) * width * 4;
        std::uint8_t* dst = bgr.data() + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const std::uint8_t r = src[x * 4 + 0];
            const std::uint8_t g = src[x * 4 + 1];
            const std::uint8_t b = src[x * 4 + 2];
            dst[x * 3 + 0] = b;
            dst[x * 3 + 1] = g;
            dst[x * 3 + 2] = r;
        }
    }

    hr = frame->WritePixels(static_cast<UINT>(height), stride,
                            static_cast<UINT>(bgr.size()), bgr.data());
    if (FAILED(hr)) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    if (FAILED(frame->Commit()) || FAILED(encoder->Commit())) {
        if (weInitializedCom) {
            CoUninitialize();
        }
        return false;
    }

    if (weInitializedCom) {
        CoUninitialize();
    }

    return true;
#else
    (void)path;
    (void)width;
    (void)height;
    (void)rgbaPixels;
    (void)quality;
    return false;
#endif
}

} // namespace vkengine
