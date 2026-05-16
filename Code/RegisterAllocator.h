#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include <iostream>
#include <vector>
#include <string>
#include <set>
struct LinePoint {
    int number;
    char type; // ' ' para normal, '+' para definição, '-' para fim de live range

    // Necessário para o std::set manter a ordenação pelas linhas de forma crescente
    bool operator<(const LinePoint& other) const {
        return number < other.number;
    }
};
// Structure to hold our Web data
struct Web {
    int id;                   // Unique integer ID to use in the Graph
    std::string variableName; // e.g., "sum", "i", "x"
    std::set<LinePoint> lines;      // Set of execution lines where this variable is live
};

// Structure to hold the configuration from registers.txt
struct Config {
    int numRegisters = 0;
    std::string algorithm = "basic";
    int k = 0; // The parameter for spilling or splitting (if applicable)
};

// Parsing functions
Config parseRegistersFile(const std::string& filename);
std::vector<Web> parseRangesFile(const std::string& filename);

#endif // REGISTER_ALLOCATOR_H