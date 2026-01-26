#pragma once

#include "engine/PhysicsConstants.hpp"

#include <cstdint>

namespace vkengine {
namespace periodic {

struct Element {
    std::uint8_t atomicNumber;   // Z
    const char* symbol;          // Chemical symbol
    const char* name;            // English name
    double atomicMass;           // Standard atomic weight (g/mol)
    int commonOxidation;         // Common oxidation state (charge), 0 means neutral reference
};

// Helper to convert a molar mass to single-atom mass in kg
inline constexpr double molarToAtomMassKg(double gramsPerMol)
{
    return gramsPerMol / physicsconstants::Avogadro / 1000.0;
}

inline constexpr Element periodicTable[] = {
    {1, "H", "Hydrogen", 1.008, +1},
    {2, "He", "Helium", 4.002602, 0},
    {3, "Li", "Lithium", 6.94, +1},
    {4, "Be", "Beryllium", 9.0121831, +2},
    {5, "B", "Boron", 10.81, +3},
    {6, "C", "Carbon", 12.011, -4},
    {7, "N", "Nitrogen", 14.007, -3},
    {8, "O", "Oxygen", 15.999, -2},
    {9, "F", "Fluorine", 18.998403163, -1},
    {10, "Ne", "Neon", 20.1797, 0},
    {11, "Na", "Sodium", 22.98976928, +1},
    {12, "Mg", "Magnesium", 24.305, +2},
    {13, "Al", "Aluminium", 26.9815385, +3},
    {14, "Si", "Silicon", 28.085, +4},
    {15, "P", "Phosphorus", 30.973761998, -3},
    {16, "S", "Sulfur", 32.06, -2},
    {17, "Cl", "Chlorine", 35.45, -1},
    {18, "Ar", "Argon", 39.948, 0},
    {19, "K", "Potassium", 39.0983, +1},
    {20, "Ca", "Calcium", 40.078, +2},
    {21, "Sc", "Scandium", 44.955908, +3},
    {22, "Ti", "Titanium", 47.867, +4},
    {23, "V", "Vanadium", 50.9415, +5},
    {24, "Cr", "Chromium", 51.9961, +3},
    {25, "Mn", "Manganese", 54.938043, +2},
    {26, "Fe", "Iron", 55.845, +2},
    {27, "Co", "Cobalt", 58.933194, +2},
    {28, "Ni", "Nickel", 58.6934, +2},
    {29, "Cu", "Copper", 63.546, +2},
    {30, "Zn", "Zinc", 65.38, +2},
    {31, "Ga", "Gallium", 69.723, +3},
    {32, "Ge", "Germanium", 72.630, +4},
    {33, "As", "Arsenic", 74.921595, -3},
    {34, "Se", "Selenium", 78.971, -2},
    {35, "Br", "Bromine", 79.904, -1},
    {36, "Kr", "Krypton", 83.798, 0},
    {37, "Rb", "Rubidium", 85.4678, +1},
    {38, "Sr", "Strontium", 87.62, +2},
    {39, "Y", "Yttrium", 88.90584, +3},
    {40, "Zr", "Zirconium", 91.224, +4},
    {41, "Nb", "Niobium", 92.90637, +5},
    {42, "Mo", "Molybdenum", 95.95, +6},
    {43, "Tc", "Technetium", 98.0, +7},
    {44, "Ru", "Ruthenium", 101.07, +3},
    {45, "Rh", "Rhodium", 102.90550, +3},
    {46, "Pd", "Palladium", 106.42, +2},
    {47, "Ag", "Silver", 107.8682, +1},
    {48, "Cd", "Cadmium", 112.414, +2},
    {49, "In", "Indium", 114.818, +3},
    {50, "Sn", "Tin", 118.710, +2},
    {51, "Sb", "Antimony", 121.760, -3},
    {52, "Te", "Tellurium", 127.60, -2},
    {53, "I", "Iodine", 126.90447, -1},
    {54, "Xe", "Xenon", 131.293, 0},
    {55, "Cs", "Caesium", 132.90545196, +1},
    {56, "Ba", "Barium", 137.327, +2},
    {57, "La", "Lanthanum", 138.90547, +3},
    {58, "Ce", "Cerium", 140.116, +3},
    {59, "Pr", "Praseodymium", 140.90766, +3},
    {60, "Nd", "Neodymium", 144.242, +3},
    {61, "Pm", "Promethium", 145.0, +3},
    {62, "Sm", "Samarium", 150.36, +3},
    {63, "Eu", "Europium", 151.964, +3},
    {64, "Gd", "Gadolinium", 157.25, +3},
    {65, "Tb", "Terbium", 158.92535, +3},
    {66, "Dy", "Dysprosium", 162.500, +3},
    {67, "Ho", "Holmium", 164.93033, +3},
    {68, "Er", "Erbium", 167.259, +3},
    {69, "Tm", "Thulium", 168.93422, +3},
    {70, "Yb", "Ytterbium", 173.045, +3},
    {71, "Lu", "Lutetium", 174.9668, +3},
    {72, "Hf", "Hafnium", 178.49, +4},
    {73, "Ta", "Tantalum", 180.94788, +5},
    {74, "W", "Tungsten", 183.84, +6},
    {75, "Re", "Rhenium", 186.207, +4},
    {76, "Os", "Osmium", 190.23, +4},
    {77, "Ir", "Iridium", 192.217, +3},
    {78, "Pt", "Platinum", 195.084, +2},
    {79, "Au", "Gold", 196.966569, +3},
    {80, "Hg", "Mercury", 200.592, +2},
    {81, "Tl", "Thallium", 204.3835, +3},
    {82, "Pb", "Lead", 207.2, +2},
    {83, "Bi", "Bismuth", 208.98040, +3},
    {84, "Po", "Polonium", 209.0, +4},
    {85, "At", "Astatine", 210.0, -1},
    {86, "Rn", "Radon", 222.0, 0},
    {87, "Fr", "Francium", 223.0, +1},
    {88, "Ra", "Radium", 226.0, +2},
    {89, "Ac", "Actinium", 227.0, +3},
    {90, "Th", "Thorium", 232.0377, +4},
    {91, "Pa", "Protactinium", 231.03588, +5},
    {92, "U", "Uranium", 238.02891, +6},
    {93, "Np", "Neptunium", 237.0, +5},
    {94, "Pu", "Plutonium", 244.0, +4},
    {95, "Am", "Americium", 243.0, +3},
    {96, "Cm", "Curium", 247.0, +3},
    {97, "Bk", "Berkelium", 247.0, +3},
    {98, "Cf", "Californium", 251.0, +3},
    {99, "Es", "Einsteinium", 252.0, +3},
    {100, "Fm", "Fermium", 257.0, +3},
    {101, "Md", "Mendelevium", 258.0, +2},
    {102, "No", "Nobelium", 259.0, +2},
    {103, "Lr", "Lawrencium", 266.0, +3},
    {104, "Rf", "Rutherfordium", 267.0, +4},
    {105, "Db", "Dubnium", 268.0, +5},
    {106, "Sg", "Seaborgium", 269.0, +6},
    {107, "Bh", "Bohrium", 270.0, +7},
    {108, "Hs", "Hassium", 270.0, +8},
    {109, "Mt", "Meitnerium", 278.0, +1},
    {110, "Ds", "Darmstadtium", 281.0, +1},
    {111, "Rg", "Roentgenium", 282.0, +1},
    {112, "Cn", "Copernicium", 285.0, +2},
    {113, "Nh", "Nihonium", 286.0, +1},
    {114, "Fl", "Flerovium", 289.0, +2},
    {115, "Mc", "Moscovium", 290.0, +1},
    {116, "Lv", "Livermorium", 293.0, +2},
    {117, "Ts", "Tennessine", 294.0, -1},
    {118, "Og", "Oganesson", 294.0, 0},
};

inline constexpr std::size_t periodicTableSize = sizeof(periodicTable) / sizeof(periodicTable[0]);

inline constexpr const Element* findByAtomicNumber(std::uint8_t z)
{
    for (const auto& e : periodicTable) {
        if (e.atomicNumber == z) {
            return &e;
        }
    }
    return nullptr;
}

inline constexpr const Element* findBySymbol(const char* symbol)
{
    // Simple linear search; symbol strings are short.
    for (const auto& e : periodicTable) {
        const char* s = e.symbol;
        if (s[0] == symbol[0] && s[1] == symbol[1] && s[2] == '\0' && symbol[2] == '\0') {
            return &e;
        }
    }
    return nullptr;
}

} // namespace periodic
} // namespace vkengine
