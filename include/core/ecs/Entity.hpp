#pragma once

#include <cstdint>

namespace core::ecs {

using EntityId = std::uint32_t;

struct Entity {
    EntityId id{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return id != 0; }
    explicit constexpr operator bool() const noexcept { return valid(); }

    friend constexpr bool operator==(Entity lhs, Entity rhs) noexcept { return lhs.id == rhs.id; }
    friend constexpr bool operator!=(Entity lhs, Entity rhs) noexcept { return !(lhs == rhs); }
};

constexpr Entity kInvalidEntity{0};

} // namespace core::ecs
