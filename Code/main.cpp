#include <iostream>
#include <fstream>
#include <algorithm>
#include <stack>
#include <map>
#include <vector>
#include <stdexcept>
#include <limits>
#include "RegisterAllocator.h"
#include "Graph.h"

// Verifica se duas webs se sobrepõem no tempo (interferem)
bool checkOverlap(const std::set<LinePoint>& set1, const std::set<LinePoint>& set2) {
    std::vector<LinePoint> intersection;
    std::set_intersection(set1.begin(), set1.end(),
                          set2.begin(), set2.end(),
                          std::back_inserter(intersection));
    return !intersection.empty();
}

void runAllocation(const std::string& rangesFile, const std::string& registersFile, std::string outputFile, bool isBatch) {
    std::cout << "Loading data..." << std::endl;

    std::vector<Web> webs = parseRangesFile(rangesFile);
    Config config = parseRegistersFile(registersFile);

    if (webs.empty()) {
        std::cerr << "Erro Execucao: Dados invalidos ou nenhuma Web encontrada." << std::endl;
        return;
    }
    if (config.numRegisters <= 0) {
        std::cerr << "Erro Execucao: Quantidade invalida de registos fornecida." << std::endl;
        return;
    }

    // Configuração standard direta e portátil
    std::string finalPath = outputFile;

    int N = config.numRegisters;
    std::string algo = config.algorithm;
    int maxSplits = config.k;

    bool isSplitting = (algo == "splitting");
    int currentSplits = 0;
    bool allocationFailed = false;

    std::map<int, int> webToRegister;
    std::set<int> spilledWebs;

    while (true) {
        // 1. Construir o Grafo de Interferência
        Graph<int> interferenceGraph;
        for (const auto& web : webs) {
            interferenceGraph.addVertex(web.id);
        }

        for (size_t i = 0; i < webs.size(); ++i) {
            for (size_t j = i + 1; j < webs.size(); ++j) {
                if (checkOverlap(webs[i].lines, webs[j].lines)) {
                    interferenceGraph.addBidirectionalEdge(webs[i].id, webs[j].id, 0.0);
                }
            }
        }

        // Mapear graus ativos dos nós que não sofreram spill
        std::map<int, int> activeDegrees;
        std::map<int, std::set<int>> adjList;

        for (auto v : interferenceGraph.getVertexSet()) {
            int uId = v->getInfo();
            if (spilledWebs.count(uId)) continue;

            int deg = 0;
            for (auto e : v->getAdj()) {
                int wId = e->getDest()->getInfo();
                if (!spilledWebs.count(wId)) {
                    deg++;
                    adjList[uId].insert(wId);
                }
            }
            activeDegrees[uId] = deg;
        }

        // 2. Fase de Simplificação (Algoritmo de Chaitin-Briggs)
        std::stack<int> simplifyStack;
        std::set<int> removedNodes;
        bool stuck = false;

        while (removedNodes.size() + spilledWebs.size() < webs.size()) {
            bool found = false;
            for (auto const& [node, deg] : activeDegrees) {
                if (removedNodes.count(node) || spilledWebs.count(node)) continue;

                if (deg < N) {
                    simplifyStack.push(node);
                    removedNodes.insert(node);

                    // Atualizar o grau dos vizinhos ativos
                    for (int neighbor : adjList[node]) {
                        if (!removedNodes.count(neighbor) && !spilledWebs.count(neighbor)) {
                            activeDegrees[neighbor]--;
                        }
                    }
                    found = true;
                    break;
                }
            }

            if (!found) {
                stuck = true;
                break;
            }
        }

        // 3. Resolução de Bloqueios: Spilling ou Splitting
        if (stuck) {
            int targetNode = -1;
            int maxDeg = -1;

            for (auto const& [node, deg] : activeDegrees) {
                if (removedNodes.count(node) || spilledWebs.count(node)) continue;
                if (deg > maxDeg) {
                    maxDeg = deg;
                    targetNode = node;
                }
            }

            if (isSplitting && currentSplits < maxSplits) {
                // Encontrar a Web original para efetuar o Split
                Web original;
                size_t origIndex = 0;
                bool foundOrig = false;
                for (size_t i = 0; i < webs.size(); ++i) {
                    if (webs[i].id == targetNode) {
                        original = webs[i];
                        origIndex = i;
                        foundOrig = true;
                        break;
                    }
                }

                if (!foundOrig || original.lines.size() <= 1) {
                    spilledWebs.insert(targetNode);
                } else {
                    std::vector<LinePoint> linhas(original.lines.begin(), original.lines.end());
                    size_t meio = linhas.size() / 2;

                    Web w1, w2;
                    static int nextWebId = 1000;
                    if (nextWebId == 1000) {
                        for (const auto& w : webs) if (w.id >= nextWebId) nextWebId = w.id + 1;
                    }

                    w1.id = nextWebId++;
                    w1.variableName = original.variableName;
                    w2.id = nextWebId++;
                    w2.variableName = original.variableName;

                    w1.lines.insert(linhas.begin(), linhas.begin() + meio);
                    w2.lines.insert(linhas.begin() + meio, linhas.end());

                    webs.erase(webs.begin() + origIndex);
                    webs.push_back(w1);
                    webs.push_back(w2);
                    currentSplits++;
                }
                continue;
            } else {
                spilledWebs.insert(targetNode);
                continue;
            }
        }

        // 4. Fase de Seleção (Coloragem do Grafo)
        webToRegister.clear();
        while (!simplifyStack.empty()) {
            int node = simplifyStack.top();
            simplifyStack.pop();

            std::set<int> usedRegisters;
            for (auto v : interferenceGraph.getVertexSet()) {
                if (v->getInfo() == node) {
                    for (auto e : v->getAdj()) {
                        int neighbor = e->getDest()->getInfo();
                        if (webToRegister.count(neighbor)) {
                            usedRegisters.insert(webToRegister[neighbor]);
                        }
                    }
                    break;
                }
            }

            int color = -1;
            for (int r = 0; r < N; ++r) {
                if (!usedRegisters.count(r)) {
                    color = r;
                    break;
                }
            }

            if (color != -1) {
                webToRegister[node] = color;
            } else {
                allocationFailed = true;
                break;
            }
        }

        break;
    }

    // ========================================================
    // PASSO FINAL: ESCRITA DO FICHEIRO OUT.TXT
    // ========================================================
    std::ofstream outFile(finalPath);
    if (!outFile.is_open()) {
        std::cerr << "Erro Execucao: Nao foi possivel criar o ficheiro de saida." << std::endl;
        return;
    }

    outFile << "webs: " << webs.size() << "\n";
    for (const auto& web : webs) {
        outFile << "web" << web.id << ": ";
        for (auto it = web.lines.begin(); it != web.lines.end(); ++it) {
            outFile << it->number;
            if (it->type != ' ') {
                outFile << it->type;
            }
            outFile << (std::next(it) == web.lines.end() ? "" : ",");
        }
        outFile << "\n";
    }

    if (allocationFailed) {
        std::cerr << "WARNING: A alocacao aos " << N << " registos fornecidos foi impossivel." << std::endl;
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
    std::cout << "SUCESSO! Ficheiro gerado." << std::endl;
}

int main(int argc, char* argv[]) {
    // Modo Batch (Script de avaliação automática)
    if (argc >= 2 && std::string(argv[1]) == "-b") {
        if (argc != 5) {
            std::cerr << "Erro: Argumentos em falta para o modo batch.\nUso: myProg -b <ranges.txt> <registers.txt> <allocation.txt>" << std::endl;
            return 1;
        }
        runAllocation(argv[2], argv[3], argv[4], true);
        return 0;
    }

    // Modo Interativo
    int choice = -1;
    do {
        std::cout << "\n=== Ferramenta de Alocacao de Registos ===\n1. Executar Alocacao\n0. Sair\nEscolha: ";
        if (!(std::cin >> choice)) {
            std::cerr << "Erro: Entrada de menu invalida!" << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (choice == 1) {
            std::string ranges, regs, out;
            std::cout << "Ficheiro de gamas: ex(datasets/ranges6.txt) ";
            std::cin >> ranges;
            std::cout << "Ficheiro de registos:datasets/registers3.txt) ";
            std::cin >> regs;
            std::cout << "Nome do ficheiro de Saida: ";
            std::cin >> out;

            runAllocation(ranges, regs, out, false);
        }
    } while (choice != 0);

    return 0;
}