#pragma once

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace vkengine {

struct Material {
    std::string name;
    glm::vec4 baseColor{1.0f};
    float metallic{0.0f};
    float roughness{0.6f};
    float specular{0.5f};
    glm::vec3 emissive{0.0f};
    float emissiveIntensity{0.0f};
    float opacity{1.0f};
    std::string albedoTexture;
};

class MaterialLibrary {
public:
    MaterialLibrary();

    void add(const Material& material);
    bool remove(const std::string& name);
    [[nodiscard]] bool has(const std::string& name) const;
    [[nodiscard]] const Material* find(const std::string& name) const;
    [[nodiscard]] const Material& getOrDefault(const std::string& name) const;
    [[nodiscard]] const Material& defaultMaterial() const noexcept { return fallback; }
    [[nodiscard]] std::vector<std::string> names() const;

private:
    std::unordered_map<std::string, Material> materials;
    Material fallback{};
};

Material makeGlassMaterial();
Material makeWoodMaterial();
Material makeSteelMaterial();
Material makeCopperMaterial();
Material makeMetalMaterial();
Material makePlasticMaterial();
Material makeRubberMaterial();
Material makeGoldMaterial();
Material makeAluminumMaterial();
Material makeConcreteMaterial();
Material makeMarbleMaterial();
Material makeLeatherMaterial();
Material makeIceMaterial();
Material makeFabricMaterial();

} // namespace vkengine
