#include "stb_image.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstring>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
	bool g_flipVertically = false;
	std::string g_lastFailure;

	void setFailure(const std::string& reason)
	{
		g_lastFailure = reason;
	}

	std::wstring toWide(const char* utf8)
	{
		if (!utf8) {
			return L"";
		}
		const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (len <= 0) {
			return L"";
		}
		std::wstring wide(static_cast<std::size_t>(len), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), len);
		// Remove trailing null that MultiByteToWideChar writes
		if (!wide.empty() && wide.back() == L'\0') {
			wide.pop_back();
		}
		return wide;
	}
}

extern "C" {

void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip)
{
	g_flipVertically = flag_true_if_should_flip != 0;
}

const char* stbi_failure_reason(void)
{
	return g_lastFailure.empty() ? "" : g_lastFailure.c_str();
}

void stbi_image_free(void* retval_from_stbi_load)
{
	delete[] static_cast<stbi_uc*>(retval_from_stbi_load);
}

stbi_uc* stbi_load(char const* filename, int* x, int* y, int* comp, int req_comp)
{
	g_lastFailure.clear();
	if (!filename) {
		setFailure("No filename provided");
		return nullptr;
	}

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool alreadyInitializedDifferentMode = (hr == RPC_E_CHANGED_MODE);
	const bool weInitializedCom = SUCCEEDED(hr);
	const bool comUsable = weInitializedCom || alreadyInitializedDifferentMode;
	if (!comUsable) {
		setFailure("Failed to initialize COM for image loading");
		return nullptr;
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
		setFailure("Failed to create WIC factory");
		return nullptr;
	}

	ComPtr<IWICBitmapDecoder> decoder;
	const std::wstring widePath = toWide(filename);
	hr = factory->CreateDecoderFromFilename(widePath.c_str(),
											nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	if (FAILED(hr)) {
		setFailure("Failed to decode image: cannot open file");
		return nullptr;
	}

	ComPtr<IWICBitmapFrameDecode> frame;
	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr)) {
		setFailure("Failed to decode image frame");
		return nullptr;
	}

	ComPtr<IWICFormatConverter> converter;
	hr = factory->CreateFormatConverter(&converter);
	if (FAILED(hr)) {
		setFailure("Failed to create format converter");
		return nullptr;
	}

	hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
							   WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
	if (FAILED(hr)) {
		setFailure("Failed to convert image to RGBA");
		return nullptr;
	}

	UINT width = 0, height = 0;
	converter->GetSize(&width, &height);
	if (width == 0 || height == 0) {
		setFailure("Decoded image has invalid dimensions");
		return nullptr;
	}

	const UINT stride = width * 4;
	const UINT bufferSize = stride * height;
	stbi_uc* buffer = new stbi_uc[bufferSize];

	hr = converter->CopyPixels(nullptr, stride, bufferSize, buffer);
	if (FAILED(hr)) {
		delete[] buffer;
		setFailure("Failed to copy image pixels");
		return nullptr;
	}

	if (g_flipVertically && height > 1) {
		std::vector<stbi_uc> row(stride);
		for (UINT yRow = 0; yRow < height / 2; ++yRow) {
			stbi_uc* top = buffer + yRow * stride;
			stbi_uc* bottom = buffer + (height - 1 - yRow) * stride;
			std::memcpy(row.data(), top, stride);
			std::memcpy(top, bottom, stride);
			std::memcpy(bottom, row.data(), stride);
		}
	}

	if (x) *x = static_cast<int>(width);
	if (y) *y = static_cast<int>(height);
	if (comp) *comp = req_comp > 0 ? req_comp : 4;

	return buffer;
}

} // extern "C"
