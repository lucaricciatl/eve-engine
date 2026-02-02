#pragma once

// Thin adapter so CubeExample depends on the shared core renderer implementation.
#include "core/VulkanRenderer.hpp"

// Backward compatibility alias for the example entry point name.
using VulkanRenderer = ::VulkanRenderer;
