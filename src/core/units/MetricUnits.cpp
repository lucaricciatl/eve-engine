#include "core/units/MetricUnits.hpp"

#include <algorithm>

namespace vkcore::units {
namespace {

double lengthToMeters(LengthUnit unit)
{
    switch (unit) {
    case LengthUnit::Meter:
        return 1.0;
    case LengthUnit::Kilometer:
        return 1000.0;
    case LengthUnit::Centimeter:
        return 0.01;
    case LengthUnit::Millimeter:
        return 0.001;
    case LengthUnit::Micrometer:
        return 1e-6;
    case LengthUnit::Nanometer:
        return 1e-9;
    default:
        return 1.0;
    }
}

double massToKilograms(MassUnit unit)
{
    switch (unit) {
    case MassUnit::Kilogram:
        return 1.0;
    case MassUnit::Gram:
        return 0.001;
    case MassUnit::Milligram:
        return 1e-6;
    default:
        return 1.0;
    }
}

double timeToSeconds(TimeUnit unit)
{
    switch (unit) {
    case TimeUnit::Second:
        return 1.0;
    case TimeUnit::Millisecond:
        return 0.001;
    case TimeUnit::Minute:
        return 60.0;
    case TimeUnit::Hour:
        return 3600.0;
    default:
        return 1.0;
    }
}

double toKelvin(double value, TemperatureUnit unit)
{
    switch (unit) {
    case TemperatureUnit::Kelvin:
        return value;
    case TemperatureUnit::Celsius:
        return value + 273.15;
    default:
        return value;
    }
}

double fromKelvin(double value, TemperatureUnit unit)
{
    switch (unit) {
    case TemperatureUnit::Kelvin:
        return value;
    case TemperatureUnit::Celsius:
        return value - 273.15;
    default:
        return value;
    }
}

} // namespace

Length Length::from(double value, LengthUnit unit)
{
    return Length{value * lengthToMeters(unit)};
}

double Length::in(LengthUnit unit) const
{
    return meters / lengthToMeters(unit);
}

Mass Mass::from(double value, MassUnit unit)
{
    return Mass{value * massToKilograms(unit)};
}

double Mass::in(MassUnit unit) const
{
    return kilograms / massToKilograms(unit);
}

Time Time::from(double value, TimeUnit unit)
{
    return Time{value * timeToSeconds(unit)};
}

double Time::in(TimeUnit unit) const
{
    return seconds / timeToSeconds(unit);
}

Temperature Temperature::from(double value, TemperatureUnit unit)
{
    return Temperature{toKelvin(value, unit)};
}

double Temperature::in(TemperatureUnit unit) const
{
    return fromKelvin(kelvin, unit);
}

Area Area::from(double value, LengthUnit unit)
{
    const double factor = lengthToMeters(unit);
    return Area{value * factor * factor};
}

double Area::in(LengthUnit unit) const
{
    const double factor = lengthToMeters(unit);
    return squareMeters / (factor * factor);
}

Volume Volume::from(double value, LengthUnit unit)
{
    const double factor = lengthToMeters(unit);
    return Volume{value * factor * factor * factor};
}

double Volume::in(LengthUnit unit) const
{
    const double factor = lengthToMeters(unit);
    return cubicMeters / (factor * factor * factor);
}

bool isFinite(double value)
{
    return std::isfinite(value);
}

Length operator+(Length lhs, Length rhs)
{
    return Length{lhs.meters + rhs.meters};
}

Length operator-(Length lhs, Length rhs)
{
    return Length{lhs.meters - rhs.meters};
}

Length operator*(Length lhs, double scalar)
{
    return Length{lhs.meters * scalar};
}

Length operator/(Length lhs, double scalar)
{
    return Length{lhs.meters / scalar};
}

Mass operator+(Mass lhs, Mass rhs)
{
    return Mass{lhs.kilograms + rhs.kilograms};
}

Mass operator-(Mass lhs, Mass rhs)
{
    return Mass{lhs.kilograms - rhs.kilograms};
}

Mass operator*(Mass lhs, double scalar)
{
    return Mass{lhs.kilograms * scalar};
}

Mass operator/(Mass lhs, double scalar)
{
    return Mass{lhs.kilograms / scalar};
}

Time operator+(Time lhs, Time rhs)
{
    return Time{lhs.seconds + rhs.seconds};
}

Time operator-(Time lhs, Time rhs)
{
    return Time{lhs.seconds - rhs.seconds};
}

Time operator*(Time lhs, double scalar)
{
    return Time{lhs.seconds * scalar};
}

Time operator/(Time lhs, double scalar)
{
    return Time{lhs.seconds / scalar};
}

Area area(Length width, Length height)
{
    return Area{width.meters * height.meters};
}

Volume volume(Length width, Length height, Length depth)
{
    return Volume{width.meters * height.meters * depth.meters};
}

Velocity velocity(Length distance, Time duration)
{
    return Velocity{distance.meters / duration.seconds};
}

Acceleration acceleration(Velocity deltaVelocity, Time duration)
{
    return Acceleration{deltaVelocity.metersPerSecond / duration.seconds};
}

Force force(Mass mass, Acceleration accel)
{
    return Force{mass.kilograms * accel.metersPerSecondSquared};
}

Energy energy(Force forceValue, Length distance)
{
    return Energy{forceValue.newtons * distance.meters};
}

Pressure pressure(Force forceValue, Area areaValue)
{
    return Pressure{forceValue.newtons / areaValue.squareMeters};
}

Density density(Mass mass, Volume volumeValue)
{
    return Density{mass.kilograms / volumeValue.cubicMeters};
}

std::string_view toString(LengthUnit unit)
{
    switch (unit) {
    case LengthUnit::Meter:
        return "m";
    case LengthUnit::Kilometer:
        return "km";
    case LengthUnit::Centimeter:
        return "cm";
    case LengthUnit::Millimeter:
        return "mm";
    case LengthUnit::Micrometer:
        return "um";
    case LengthUnit::Nanometer:
        return "nm";
    default:
        return "m";
    }
}

std::string_view toString(MassUnit unit)
{
    switch (unit) {
    case MassUnit::Kilogram:
        return "kg";
    case MassUnit::Gram:
        return "g";
    case MassUnit::Milligram:
        return "mg";
    default:
        return "kg";
    }
}

std::string_view toString(TimeUnit unit)
{
    switch (unit) {
    case TimeUnit::Second:
        return "s";
    case TimeUnit::Millisecond:
        return "ms";
    case TimeUnit::Minute:
        return "min";
    case TimeUnit::Hour:
        return "h";
    default:
        return "s";
    }
}

std::string_view toString(TemperatureUnit unit)
{
    switch (unit) {
    case TemperatureUnit::Kelvin:
        return "K";
    case TemperatureUnit::Celsius:
        return "C";
    default:
        return "K";
    }
}

} // namespace vkcore::units
