#pragma once

#include <cstdint>

namespace vkengine {
namespace physicsconstants {

// Fundamental constants (SI units)
inline constexpr double SpeedOfLight = 2.99792458e8;          // m / s
inline constexpr double Planck = 6.62607015e-34;             // J * s
inline constexpr double ReducedPlanck = 1.054571817e-34;     // J * s
inline constexpr double Boltzmann = 1.380649e-23;            // J / K
inline constexpr double Avogadro = 6.02214076e23;            // 1 / mol
inline constexpr double GasConstant = 8.314462618;           // J / (mol * K)
inline constexpr double ElementaryCharge = 1.602176634e-19;  // C
inline constexpr double VacuumPermittivity = 8.8541878128e-12; // F / m
inline constexpr double VacuumPermeability = 1.25663706212e-6; // N / A^2
inline constexpr double GravitationalConstant = 6.67430e-11;   // m^3 / (kg * s^2)

// Derived EM constants
inline constexpr double CoulombConstant = 8.9875517923e9;      // N * m^2 / C^2
inline constexpr double VacuumImpedance = 376.730313668;       // ohms

// Radiative/thermal constants
inline constexpr double StefanBoltzmann = 5.670374419e-8;      // W / (m^2 * K^4)
inline constexpr double WienDisplacement = 2.897771955e-3;     // m * K

// Particle masses (kg)
inline constexpr double ElectronMass = 9.1093837015e-31;
inline constexpr double ProtonMass = 1.67262192369e-27;
inline constexpr double NeutronMass = 1.67492749804e-27;
inline constexpr double AtomicMassUnit = 1.66053906660e-27;    // kg

// Derived or handy factors
inline constexpr double ElectronVolt = ElementaryCharge; // Joules per eV
inline constexpr double FaradayConstant = Avogadro * ElementaryCharge; // C / mol

// Atomic/quantum constants
inline constexpr double FineStructure = 7.2973525693e-3;       // unitless
inline constexpr double BohrRadius = 5.29177210903e-11;        // m
inline constexpr double RydbergConstant = 10973731.568160;     // 1 / m
inline constexpr double HartreeEnergy = 4.3597447222071e-18;   // J
inline constexpr double ClassicalElectronRadius = 2.8179403262e-15; // m
inline constexpr double ThomsonCrossSection = 6.6524587321e-29;     // m^2

} // namespace physicsconstants
} // namespace vkengine
