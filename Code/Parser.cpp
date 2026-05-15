#include "RegisterAllocator.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Helper to check if two sets of integers intersect
bool hasOverlap(const std::set<int>& set1, const std::set<int>& set2) {
    std::vector<int> intersection;
    std::set_intersection(set1.begin(), set1.end(), 
                          set2.begin(), set2.end(), 
                          std::back_inserter(intersection));
    return !intersection.empty();
}

Config parseRegistersFile(const std::string& filename) {
    Config config;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << std::endl;
        return config;
    }

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments

        std::stringstream ss(line);
        std::string key;
        std::getline(ss, key, ':');

        if (key == "registers") {
            ss >> config.numRegisters;
        } else if (key == "algorithm") {
            std::string algoStr;
            std::getline(ss, algoStr);
            
            // Clean up leading spaces
            algoStr.erase(0, algoStr.find_first_not_of(" \t"));
            
            // Check if there is a comma (e.g., "spilling, 2")
            size_t commaPos = algoStr.find(',');
            if (commaPos != std::string::npos) {
                config.algorithm = algoStr.substr(0, commaPos);
                config.k = std::stoi(algoStr.substr(commaPos + 1));
            } else {
                config.algorithm = algoStr;
            }
        }
    }
    return config;
}

std::vector<Web> parseRangesFile(const std::string& filename) {
    std::vector<Web> webs;
    std::ifstream file(filename);
    std::string line;
    int nextId = 0;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << std::endl;
        return webs;
    }

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments

        std::stringstream ss(line);
        std::string varName, rangesStr;
        
        std::getline(ss, varName, ':');
        std::getline(ss, rangesStr);

        // Remove spaces
        varName.erase(remove_if(varName.begin(), varName.end(), isspace), varName.end());

        // Parse the comma-separated numbers
        std::set<int> currentLines;
        std::stringstream rangeStream(rangesStr);
        std::string token;

        while (std::getline(rangeStream, token, ',')) {
            // Remove spaces, '+', and '-'
            token.erase(remove_if(token.begin(), token.end(), 
                [](char c) { return isspace(c) || c == '+' || c == '-'; }), token.end());
            
            if (!token.empty()) {
                currentLines.insert(std::stoi(token));
            }
        }

        // Greedy Merge: Check if we already have a web for this variable that overlaps
        bool merged = false;
        for (auto& web : webs) {
            if (web.variableName == varName && hasOverlap(web.lines, currentLines)) {
                // Merge the sets
                web.lines.insert(currentLines.begin(), currentLines.end());
                merged = true;
                break;
            }
        }

        // If it didn't overlap with an existing web, create a new one
        if (!merged) {
            Web newWeb;
            newWeb.id = nextId++;
            newWeb.variableName = varName;
            newWeb.lines = currentLines;
            webs.push_back(newWeb);
        }
    }

    return webs;
}