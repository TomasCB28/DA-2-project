#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include "RegisterAllocator.h"
#include "Graph.h"

// Função com nome alterado para não chocar com o Parser.cpp
bool websInterfere(const std::set<int>& s1, const std::set<int>& s2) {
    auto it1 = s1.begin();
    auto it2 = s2.begin();
    while (it1 != s1.end() && it2 != s2.end()) {
        if (*it1 < *it2) ++it1;
        else if (*it2 < *it1) ++it2;
        else return true; // Encontrou um número igual
    }
    return false;
}

void runAllocation(const std::string& rangesFile, const std::string& registersFile, const std::string& outputFile) {
    std::cout << "Loading data..." << std::endl;

    std::vector<Web> webs = parseRangesFile(rangesFile);
    Config config = parseRegistersFile(registersFile);

    if (webs.empty()) {
        std::cerr << "Erro: Nenhuma Web encontrada em " << rangesFile << std::endl;
        return;
    }

    std::cout << "Parsed " << webs.size() << " distinct webs." << std::endl;
    std::cout << "Config: " << config.numRegisters << " registers, Algorithm: " << config.algorithm << std::endl;

    // 1. Construção do Grafo
    Graph<Web> interferenceGraph;
    for (const auto& w : webs) {
        interferenceGraph.addVertex(w);
    }

    for (size_t i = 0; i < webs.size(); ++i) {
        for (size_t j = i + 1; j < webs.size(); ++j) {
            if (websInterfere(webs[i].lines, webs[j].lines)) {
                interferenceGraph.addBidirectionalEdge(webs[i], webs[j], 1.0);
            }
        }
    }

    // 2. Coloração
    std::map<int, int> allocation;
    for (auto v : interferenceGraph.getVertexSet()) {
        std::set<int> neighborColors;

        for (auto edge : v->getAdj()) {
            int neighborId = edge->getDest()->getInfo().id;
            if (allocation.count(neighborId)) {
                neighborColors.insert(allocation[neighborId]);
            }
        }

        for (int color = 1; color <= config.numRegisters; ++color) {
            if (neighborColors.find(color) == neighborColors.end()) {
                allocation[v->getInfo().id] = color;
                break;
            }
        }
    }

    // 3. Escrita do Ficheiro de Output
    std::ofstream outFile(outputFile);
    if (!outFile.is_open()) {
        std::cerr << "Erro ao criar o ficheiro de saída: " << outputFile << std::endl;
        return;
    }

    outFile << "# Register Allocation Results for " << rangesFile << "\n";
    outFile << "# Algorithm: " << config.algorithm << "\n";

    for (const auto& w : webs) {
        outFile << w.variableName << " (Web " << w.id << "): ";
        if (allocation.count(w.id)) {
            outFile << "R" << allocation[w.id] << "\n";
        } else {
            outFile << "SPILL\n";
        }
    }

    outFile.close();
    std::cout << "Sucesso! Resultado guardado em: " << outputFile << std::endl;
}

// A tua função MAIN original, sã e salva!
int main(int argc, char* argv[]) {
    if (argc == 5 && std::string(argv[1]) == "-b") {
        std::string rangesFile = argv[2];
        std::string registersFile = argv[3];
        std::string outputFile = argv[4];

        runAllocation(rangesFile, registersFile, outputFile);
        return 0;
    }

    int choice;
    do {
        std::cout << "\n=== Compiler Register Allocation Tool ===" << std::endl;
        std::cout << "1. Run Allocation" << std::endl;
        std::cout << "0. Exit" << std::endl;
        std::cout << "Enter choice: ";
        std::cin >> choice;

        if (choice == 1) {
            std::string ranges, regs, out;
            std::cout << "Enter ranges file path (e.g., ranges1.txt): ";
            std::cin >> ranges;
            std::cout << "Enter registers file path (e.g., registers2.txt): ";
            std::cin >> regs;
            std::cout << "Enter output file path (e.g., out.txt): ";
            std::cin >> out;

            runAllocation(ranges, regs, out);
        }
    } while (choice != 0);

    return 0;
}