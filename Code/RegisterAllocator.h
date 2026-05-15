#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include <iostream>
#include <vector>
#include <string>
#include <set>

// Structure to hold our Web data
struct Web {
    int id;                   // Unique integer ID to use in the Graph
    std::string variableName; // e.g., "sum", "i", "x"
    std::set<int> lines;      // Set of execution lines where this variable is live
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