#pragma once

namespace vkengine {

class IGameEngine;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void attachEngine(IGameEngine& engine) = 0;
    virtual void run() = 0;
};

} // namespace vkengine
