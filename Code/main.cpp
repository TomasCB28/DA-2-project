#include <iostream>
#include <fstream>
#include <algorithm>
#include <stack>
#include <map>
#include <filesystem> // Para criar pastas automaticamente
#include "RegisterAllocator.h"
#include "Graph.h"

namespace fs = std::filesystem;

bool checkOverlap(const std::set<int>& set1, const std::set<int>& set2) {
    std::vector<int> intersection;
    std::set_intersection(set1.begin(), set1.end(),
                          set2.begin(), set2.end(),
                          std::back_inserter(intersection));
    return !intersection.empty();
}

void runAllocation(const std::string& rangesFile, const std::string& registersFile, std::string outputFile, bool isBatch) {
    std::cout << "Loading data..." << std::endl;

    std::vector<Web> webs = parseRangesFile(rangesFile);
    Config config = parseRegistersFile(registersFile);

    if (webs.empty() || config.numRegisters == 0) {
        std::cerr << "Erro: Dados invalidos ou 0 registos fornecidos." << std::endl;
        return;
    }

    // Configuração do caminho de saída
    std::string finalPath;
    if (isBatch) {
        finalPath = outputFile;
    } else {
        // No modo interativo, assume que queres guardar em ../datasets/outputs/
        std::string outputDir = "../datasets/outputs";

        // GARANTIA: Se a pasta não existir, o código cria-a agora!
        if (!fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        finalPath = outputDir + "/" + outputFile;
    }

    std::cout << "Config: " << config.numRegisters << " registos, Algoritmo: " << config.algorithm << std::endl;

    // --- PASSO 2: CONSTRUIR GRAFO ---
    Graph<int> interferenceGraph;
    for (const auto& web : webs) interferenceGraph.addVertex(web.id);

    for (size_t i = 0; i < webs.size(); ++i) {
        for (size_t j = i + 1; j < webs.size(); ++j) {
            if (checkOverlap(webs[i].lines, webs[j].lines)) {
                interferenceGraph.addBidirectionalEdge(webs[i].id, webs[j].id, 0);
            }
        }
    }

    // --- PASSO 3: ALOCAÇÃO (STACK + SPILLING) ---
    int N = config.numRegisters;
    int maxSpills = (config.algorithm == "spilling") ? config.k : 0;
    std::stack<int> S;
    std::set<int> spilledWebs;
    std::map<int, int> webToRegister;
    bool allocationFailed = false;

    std::set<int> activeNodes;
    std::map<int, int> activeDegrees;
    for (auto v : interferenceGraph.getVertexSet()) {
        activeNodes.insert(v->getInfo());
        activeDegrees[v->getInfo()] = v->getAdj().size();
    }

    int currentSpills = 0;
    while (!activeNodes.empty()) {
        int nodeToRemove = -1;
        for (int node : activeNodes) {
            if (activeDegrees[node] < N) { nodeToRemove = node; break; }
        }

        if (nodeToRemove != -1) {
            activeNodes.erase(nodeToRemove);
            S.push(nodeToRemove);
            Vertex<int>* v = interferenceGraph.findVertex(nodeToRemove);
            for (auto edge : v->getAdj()) {
                int neighbor = edge->getDest()->getInfo();
                if (activeNodes.find(neighbor) != activeNodes.end()) activeDegrees[neighbor]--;
            }
        } else {
            if (currentSpills < maxSpills) {
                int maxDegNode = -1, maxDeg = -1;
                for (int node : activeNodes) {
                    if (activeDegrees[node] > maxDeg) { maxDeg = activeDegrees[node]; maxDegNode = node; }
                }
                spilledWebs.insert(maxDegNode);
                activeNodes.erase(maxDegNode);
                currentSpills++;
                Vertex<int>* v = interferenceGraph.findVertex(maxDegNode);
                for (auto edge : v->getAdj()) {
                    int neighbor = edge->getDest()->getInfo();
                    if (activeNodes.find(neighbor) != activeNodes.end()) activeDegrees[neighbor]--;
                }
            } else { allocationFailed = true; break; }
        }
    }

    if (!allocationFailed) {
        while (!S.empty()) {
            int node = S.top(); S.pop();
            std::set<int> usedColors;
            Vertex<int>* v = interferenceGraph.findVertex(node);
            for (auto edge : v->getAdj()) {
                int neighbor = edge->getDest()->getInfo();
                if (webToRegister.count(neighbor)) usedColors.insert(webToRegister[neighbor]);
            }
            int color = -1;
            for (int c = 0; c < N; ++c) {
                if (!usedColors.count(c)) { color = c; break; }
            }
            if (color != -1) webToRegister[node] = color;
            else { allocationFailed = true; break; }
        }
    }

    // --- PASSO 4: ESCRITA DO FICHEIRO ---
    std::ofstream outFile(finalPath);
    if (!outFile.is_open()) {
        std::cerr << "Erro fatal: Nao foi possivel criar o ficheiro em " << finalPath << std::endl;
        return;
    }

    outFile << "webs: " << webs.size() << "\n";
    for (const auto& web : webs) {
        outFile << "web" << web.id << ": ";
        for (auto it = web.lines.begin(); it != web.lines.end(); ++it) {
            outFile << *it << (std::next(it) == web.lines.end() ? "" : ",");
        }
        outFile << "\n";
    }

    if (allocationFailed) {
        outFile << "registers: 0\n";
        for (const auto& web : webs) outFile << "M: web" << web.id << "\n";
    } else {
        outFile << "registers: " << N << "\n";
        for (const auto& web : webs) {
            if (spilledWebs.count(web.id)) outFile << "M: web" << web.id << "\n";
            else outFile << "r" << webToRegister[web.id] << ": web" << web.id << "\n";
        }
    }
    outFile.close();
    std::cout << "\nSUCESSO! Ficheiro gerado em: " << finalPath << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 5 && std::string(argv[1]) == "-b") {
        runAllocation(argv[2], argv[3], argv[4], true);
        return 0;
    }

    int choice;
    do {
        std::cout << "\n=== Ferramenta de Alocacao de Registos ===\n1. Executar Alocacao\n0. Sair\nEscolha: ";
        std::cin >> choice;
        if (choice == 1) {
            std::string ranges, regs, out;
            std::cout << "Ficheiro de ranges: "; std::cin >> ranges;
            std::cout << "Ficheiro de registos: "; std::cin >> regs;
            std::cout << "Nome do ficheiro (ex: out.txt): "; std::cin >> out;
            runAllocation(ranges, regs, out, false);
        }
    } while (choice != 0);
    return 0;
}