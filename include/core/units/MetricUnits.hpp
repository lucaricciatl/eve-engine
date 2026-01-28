#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace vkcore::units {

enum class LengthUnit : std::uint8_t {
    Meter,
    Kilometer,
    Centimeter,
    Millimeter,
    Micrometer,
    Nanometer
};

enum class MassUnit : std::uint8_t {
    Kilogram,
    Gram,
    Milligram
};

enum class TimeUnit : std::uint8_t {
    Second,
    Millisecond,
    Minute,
    Hour
};

enum class TemperatureUnit : std::uint8_t {
    Kelvin,
    Celsius
};

struct Length {
    double meters{0.0};

    static Length from(double value, LengthUnit unit);
    double in(LengthUnit unit) const;
};

struct Mass {
    double kilograms{0.0};

    static Mass from(double value, MassUnit unit);
    double in(MassUnit unit) const;
};

struct Time {
    double seconds{0.0};

    static Time from(double value, TimeUnit unit);
    double in(TimeUnit unit) const;
};

struct Temperature {
    double kelvin{0.0};

    static Temperature from(double value, TemperatureUnit unit);
    double in(TemperatureUnit unit) const;
};

struct Area {
    double squareMeters{0.0};

    static Area from(double value, LengthUnit unit);
    double in(LengthUnit unit) const;
};

struct Volume {
    double cubicMeters{0.0};

    static Volume from(double value, LengthUnit unit);
    double in(LengthUnit unit) const;
};

struct Velocity {
    double metersPerSecond{0.0};
};

struct Acceleration {
    double metersPerSecondSquared{0.0};
};

struct Force {
    double newtons{0.0};
};

struct Energy {
    double joules{0.0};
};

struct Pressure {
    double pascals{0.0};
};

struct Density {
    double kilogramsPerCubicMeter{0.0};
};

bool isFinite(double value);

Length operator+(Length lhs, Length rhs);
Length operator-(Length lhs, Length rhs);
Length operator*(Length lhs, double scalar);
Length operator/(Length lhs, double scalar);

Mass operator+(Mass lhs, Mass rhs);
Mass operator-(Mass lhs, Mass rhs);
Mass operator*(Mass lhs, double scalar);
Mass operator/(Mass lhs, double scalar);

Time operator+(Time lhs, Time rhs);
Time operator-(Time lhs, Time rhs);
Time operator*(Time lhs, double scalar);
Time operator/(Time lhs, double scalar);

Area area(Length width, Length height);
Volume volume(Length width, Length height, Length depth);

Velocity velocity(Length distance, Time duration);
Acceleration acceleration(Velocity deltaVelocity, Time duration);
Force force(Mass mass, Acceleration accel);
Energy energy(Force forceValue, Length distance);
Pressure pressure(Force forceValue, Area areaValue);
Density density(Mass mass, Volume volumeValue);

std::string_view toString(LengthUnit unit);
std::string_view toString(MassUnit unit);
std::string_view toString(TimeUnit unit);
std::string_view toString(TemperatureUnit unit);

} // namespace vkcore::units
