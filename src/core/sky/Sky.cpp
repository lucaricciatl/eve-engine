#include "core/sky/Sky.hpp"

#include <numeric>

namespace vkcore::sky {

std::optional<glm::vec4> loadSkyColorFromFile(const std::filesystem::path& path)
{
	if (!std::filesystem::exists(path)) {
		return std::nullopt;
	}

	vkengine::TextureData data{};
	try {
		data = vkengine::loadTexture(path, /*flipVertical*/ false);
	} catch (const std::exception&) {
		return std::nullopt;
	}
	if (data.width == 0 || data.height == 0 || data.pixels.empty() || data.channels == 0) {
		return std::nullopt;
	}

	const size_t pixelCount = static_cast<size_t>(data.width) * static_cast<size_t>(data.height);
	const size_t stride = static_cast<size_t>(data.channels);
	const uint8_t* pixels = data.pixels.data();

	double sumR = 0.0, sumG = 0.0, sumB = 0.0;
	for (size_t i = 0; i < pixelCount; ++i) {
		const size_t idx = i * stride;
		const double r = pixels[idx + 0];
		const double g = (stride > 1) ? pixels[idx + 1] : r;
		const double b = (stride > 2) ? pixels[idx + 2] : r;
		sumR += r;
		sumG += g;
		sumB += b;
	}

	const double inv = pixelCount > 0 ? 1.0 / static_cast<double>(pixelCount) : 0.0;
	glm::vec3 avg = {
		static_cast<float>((sumR * inv) / 255.0),
		static_cast<float>((sumG * inv) / 255.0),
		static_cast<float>((sumB * inv) / 255.0)
	};
	return glm::vec4{avg, 1.0f};
}

} // namespace vkcore::sky
