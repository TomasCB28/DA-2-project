#include "RegisterAllocator.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Auxiliar para verificar sobreposição usando a ordenação nativa por número de linha
bool hasOverlap(const std::set<LinePoint>& set1, const std::set<LinePoint>& set2) {
    std::vector<LinePoint> intersection;
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
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string key;
        std::getline(ss, key, ':');

        if (key == "registers") {
            ss >> config.numRegisters;
        } else if (key == "algorithm") {
            std::string algoStr;
            std::getline(ss, algoStr);
            algoStr.erase(0, algoStr.find_first_not_of(" \t"));

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
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string varName, rangesStr;

        std::getline(ss, varName, ':');
        std::getline(ss, rangesStr);

        varName.erase(remove_if(varName.begin(), varName.end(), isspace), varName.end());

        std::set<LinePoint> currentLines;
        std::stringstream rangeStream(rangesStr);
        std::string token;

        while (std::getline(rangeStream, token, ',')) {
            token.erase(remove_if(token.begin(), token.end(), isspace), token.end());
            if (token.empty()) continue;

            LinePoint lp;
            lp.type = ' '; // Por padrão, linha neutra

            if (token.back() == '+') {
                lp.type = '+';
                token.pop_back();
            } else if (token.back() == '-') {
                lp.type = '-';
                token.pop_back();
            }

            lp.number = std::stoi(token);
            currentLines.insert(lp);
        }

        bool merged = false;
        for (auto& web : webs) {
            if (web.variableName == varName && hasOverlap(web.lines, currentLines)) {
                // Efetuar a fusão (Fuse) de pontos e misturar os sets
                for (const auto& newLine : currentLines) {
                    auto it = web.lines.find(newLine);
                    if (it != web.lines.end()) {
                        // Se a linha já existe, aplicamos a regra de fusão do enunciado
                        // (ex: um fim '-' e um início '+' na mesma linha fundem-se em continuidade ' ')
                        if ((it->type == '-' && newLine.type == '+') || (it->type == '+' && newLine.type == '-')) {
                            // std::set não deixa alterar diretamente a chave, removemos e reinserimos atualizado
                            web.lines.erase(it);
                            LinePoint fusedLine = newLine;
                            fusedLine.type = ' ';
                            web.lines.insert(fusedLine);
                        }
                    } else {
                        web.lines.insert(newLine);
                    }
                }
                merged = true;
                break;
            }
        }

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