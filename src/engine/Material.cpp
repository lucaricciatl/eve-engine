#include "engine/Material.hpp"

#include <algorithm>
#include <cctype>

namespace vkengine {
namespace {
std::string normalizeName(const std::string& name)
{
    std::string out = name;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

Material makeDefaultMaterial()
{
    Material material{};
    material.name = "default";
    material.baseColor = glm::vec4(1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.6f;
    material.specular = 0.5f;
    material.emissive = glm::vec3(0.0f);
    material.emissiveIntensity = 0.0f;
    material.opacity = 1.0f;
    return material;
}
} // namespace

Material makeGlassMaterial()
{
    Material material{};
    material.name = "glass";
    material.baseColor = glm::vec4(0.7f, 0.85f, 1.0f, 0.25f);
    material.metallic = 0.0f;
    material.roughness = 0.05f;
    material.specular = 1.0f;
    material.opacity = 1.0f;
    return material;
}

Material makeWoodMaterial()
{
    Material material{};
    material.name = "wood";
    material.baseColor = glm::vec4(0.62f, 0.47f, 0.28f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.75f;
    material.specular = 0.25f;
    return material;
}

Material makeSteelMaterial()
{
    Material material{};
    material.name = "steel";
    material.baseColor = glm::vec4(0.72f, 0.74f, 0.78f, 1.0f);
    material.metallic = 0.9f;
    material.roughness = 0.2f;
    material.specular = 0.9f;
    return material;
}

Material makeCopperMaterial()
{
    Material material{};
    material.name = "copper";
    material.baseColor = glm::vec4(0.85f, 0.45f, 0.28f, 1.0f);
    material.metallic = 1.0f;
    material.roughness = 0.25f;
    material.specular = 0.95f;
    return material;
}

Material makeMetalMaterial()
{
    Material material{};
    material.name = "metal";
    material.baseColor = glm::vec4(0.65f, 0.68f, 0.72f, 1.0f);
    material.metallic = 0.8f;
    material.roughness = 0.3f;
    material.specular = 0.85f;
    return material;
}

Material makePlasticMaterial()
{
    Material material{};
    material.name = "plastic";
    material.baseColor = glm::vec4(0.18f, 0.22f, 0.28f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.4f;
    material.specular = 0.55f;
    return material;
}

Material makeRubberMaterial()
{
    Material material{};
    material.name = "rubber";
    material.baseColor = glm::vec4(0.07f, 0.07f, 0.07f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.9f;
    material.specular = 0.15f;
    return material;
}

Material makeGoldMaterial()
{
    Material material{};
    material.name = "gold";
    material.baseColor = glm::vec4(1.0f, 0.77f, 0.33f, 1.0f);
    material.metallic = 1.0f;
    material.roughness = 0.2f;
    material.specular = 1.0f;
    return material;
}

Material makeAluminumMaterial()
{
    Material material{};
    material.name = "aluminum";
    material.baseColor = glm::vec4(0.83f, 0.85f, 0.88f, 1.0f);
    material.metallic = 0.9f;
    material.roughness = 0.3f;
    material.specular = 0.8f;
    return material;
}

Material makeConcreteMaterial()
{
    Material material{};
    material.name = "concrete";
    material.baseColor = glm::vec4(0.55f, 0.56f, 0.57f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.85f;
    material.specular = 0.2f;
    return material;
}

Material makeMarbleMaterial()
{
    Material material{};
    material.name = "marble";
    material.baseColor = glm::vec4(0.9f, 0.9f, 0.92f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.2f;
    material.specular = 0.7f;
    return material;
}

Material makeLeatherMaterial()
{
    Material material{};
    material.name = "leather";
    material.baseColor = glm::vec4(0.36f, 0.23f, 0.12f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.65f;
    material.specular = 0.35f;
    return material;
}

Material makeIceMaterial()
{
    Material material{};
    material.name = "ice";
    material.baseColor = glm::vec4(0.75f, 0.9f, 1.0f, 0.45f);
    material.metallic = 0.0f;
    material.roughness = 0.08f;
    material.specular = 0.9f;
    material.opacity = 0.6f;
    return material;
}

Material makeFabricMaterial()
{
    Material material{};
    material.name = "fabric";
    material.baseColor = glm::vec4(0.4f, 0.12f, 0.2f, 1.0f);
    material.metallic = 0.0f;
    material.roughness = 0.85f;
    material.specular = 0.2f;
    return material;
}

MaterialLibrary::MaterialLibrary()
    : fallback(makeDefaultMaterial())
{
    add(fallback);
    add(makeGlassMaterial());
    add(makeWoodMaterial());
    add(makeSteelMaterial());
    add(makeCopperMaterial());
    add(makeMetalMaterial());
    add(makePlasticMaterial());
    add(makeRubberMaterial());
    add(makeGoldMaterial());
    add(makeAluminumMaterial());
    add(makeConcreteMaterial());
    add(makeMarbleMaterial());
    add(makeLeatherMaterial());
    add(makeIceMaterial());
    add(makeFabricMaterial());

    Material alias = makeCopperMaterial();
    alias.name = "chopper";
    add(alias);
}

void MaterialLibrary::add(const Material& material)
{
    if (material.name.empty()) {
        return;
    }
    materials[normalizeName(material.name)] = material;
}

bool MaterialLibrary::remove(const std::string& name)
{
    return materials.erase(normalizeName(name)) > 0;
}

bool MaterialLibrary::has(const std::string& name) const
{
    return materials.find(normalizeName(name)) != materials.end();
}

const Material* MaterialLibrary::find(const std::string& name) const
{
    auto it = materials.find(normalizeName(name));
    if (it == materials.end()) {
        return nullptr;
    }
    return &it->second;
}

const Material& MaterialLibrary::getOrDefault(const std::string& name) const
{
    if (const Material* found = find(name)) {
        return *found;
    }
    return fallback;
}

std::vector<std::string> MaterialLibrary::names() const
{
    std::vector<std::string> result;
    result.reserve(materials.size());
    for (const auto& [key, value] : materials) {
        result.push_back(value.name.empty() ? key : value.name);
    }
    return result;
}

} // namespace vkengine
