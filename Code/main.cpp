#include <iostream>
#include <fstream>
#include <algorithm>
#include <stack>
#include <map>
#include <vector>
#include <filesystem>
#include "RegisterAllocator.h"
#include "Graph.h"

namespace fs = std::filesystem;

// Função para verificar se duas webs se sobrepõem no tempo (interferem)
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

    // Configuração do caminho de saída para a pasta ../datasets/outputs/ (se não for batch)
    std::string finalPath = outputFile;
    if (!isBatch) {
        std::string outputDir = "../datasets/outputs";
        if (!fs::exists(outputDir)) fs::create_directories(outputDir);
        finalPath = outputDir + "/" + outputFile;
    }

    std::cout << "Config: " << config.numRegisters << " registos, Algoritmo: " << config.algorithm << std::endl;

    int N = config.numRegisters;
    bool isSpilling = config.algorithm.find("spilling") != std::string::npos;
    bool isSplitting = config.algorithm.find("splitting") != std::string::npos;
    int maxMods = (isSpilling || isSplitting) ? config.k : 0;

    std::map<int, int> webToRegister;
    std::set<int> spilledWebs;
    bool allocationFailed = false;

    // ========================================================
    // TASK 2.3: LÓGICA DE SPLITTING (Reconstrução iterativa do Grafo)
    // ========================================================
    if (isSplitting) {
        int currentSplits = 0;
        int nextWebId = 0;
        for (const auto& w : webs) if (w.id >= nextWebId) nextWebId = w.id + 1;

        bool success = false;
        while (!success && !allocationFailed) {
            Graph<int> interferenceGraph;
            for (const auto& web : webs) interferenceGraph.addVertex(web.id);
            for (size_t i = 0; i < webs.size(); ++i) {
                for (size_t j = i + 1; j < webs.size(); ++j) {
                    if (checkOverlap(webs[i].lines, webs[j].lines)) {
                        interferenceGraph.addBidirectionalEdge(webs[i].id, webs[j].id, 0);
                    }
                }
            }

            std::stack<int> S;
            std::set<int> activeNodes;
            std::map<int, int> activeDegrees;
            for (auto v : interferenceGraph.getVertexSet()) {
                activeNodes.insert(v->getInfo());
                activeDegrees[v->getInfo()] = v->getAdj().size();
            }

            bool encravou = false;
            int noProblematico = -1;

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
                        if (activeNodes.count(neighbor)) activeDegrees[neighbor]--;
                    }
                } else {
                    encravou = true;
                    int maxDeg = -1;
                    for (int node : activeNodes) {
                        if (activeDegrees[node] > maxDeg) { maxDeg = activeDegrees[node]; noProblematico = node; }
                    }
                    break;
                }
            }

            if (encravou) {
                if (currentSplits < maxMods) {
                    auto it = std::find_if(webs.begin(), webs.end(), [noProblematico](const Web& w){ return w.id == noProblematico; });
                    if (it != webs.end() && it->lines.size() > 1) {
                        std::cout << "-> Conflito! A dividir o Web " << noProblematico << " ao meio...\n";
                        Web original = *it;
                        webs.erase(it);

                        std::vector<int> linhas(original.lines.begin(), original.lines.end());
                        size_t meio = linhas.size() / 2;

                        Web w1, w2;
                        w1.id = nextWebId++;
                        w2.id = nextWebId++;
                        w1.lines.insert(linhas.begin(), linhas.begin() + meio);
                        w2.lines.insert(linhas.begin() + meio, linhas.end());

                        webs.push_back(w1);
                        webs.push_back(w2);
                        currentSplits++;
                    } else {
                        allocationFailed = true;
                    }
                } else {
                    allocationFailed = true;
                }
            } else {
                webToRegister.clear();
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
                    else allocationFailed = true;
                }
                if (!allocationFailed) success = true;
            }
        }
    }
    // ========================================================
    // TASK 2.4: ALGORITMO LIVRE (LINEAR SCAN - Poletto & Sarkar)
    // ========================================================
    else if (config.algorithm == "free") {
        std::cout << "-> A executar Algoritmo Livre: Linear Scan (Poletto & Sarkar)...\n";

        std::vector<Web> sortedWebs = webs;
        std::sort(sortedWebs.begin(), sortedWebs.end(), [](const Web& a, const Web& b) {
            return *a.lines.begin() < *b.lines.begin();
        });

        std::vector<Web> active;
        std::set<int> freeRegisters;
        for (int i = 0; i < N; ++i) freeRegisters.insert(i);

        for (const auto& w : sortedWebs) {
            int start_w = *w.lines.begin();
            int end_w = *w.lines.rbegin();

            // Expirar intervalos antigos
            for (auto it = active.begin(); it != active.end(); ) {
                if (*it->lines.rbegin() < start_w) {
                    freeRegisters.insert(webToRegister[it->id]);
                    it = active.erase(it);
                } else {
                    ++it;
                }
            }

            // Alocar ou Fazer Spilling
            if (active.size() == N) {
                auto spillIt = active.begin();
                int max_end = *spillIt->lines.rbegin();

                for (auto it = active.begin(); it != active.end(); ++it) {
                    if (*it->lines.rbegin() > max_end) {
                        max_end = *it->lines.rbegin();
                        spillIt = it;
                    }
                }

                if (max_end > end_w) {
                    std::cout << "   Linear Scan: Spilling Web " << spillIt->id
                              << " (Dura ate " << max_end << " | Roubando registo para o Web " << w.id << ").\n";
                    spilledWebs.insert(spillIt->id);
                    int freedReg = webToRegister[spillIt->id];
                    webToRegister.erase(spillIt->id);
                    active.erase(spillIt);

                    webToRegister[w.id] = freedReg;
                    active.push_back(w);
                } else {
                    std::cout << "   Linear Scan: Spilling Web " << w.id
                              << " (Dura demasiado tempo ate " << end_w << ").\n";
                    spilledWebs.insert(w.id);
                }
            } else {
                int reg = *freeRegisters.begin();
                freeRegisters.erase(freeRegisters.begin());
                webToRegister[w.id] = reg;
                active.push_back(w);
            }
        }
    }
    // ========================================================
    // TASKS 2.1 e 2.2: LÓGICA BASIC & SPILLING (Graph Coloring)
    // ========================================================
    else {
        Graph<int> interferenceGraph;
        for (const auto& web : webs) interferenceGraph.addVertex(web.id);
        for (size_t i = 0; i < webs.size(); ++i) {
            for (size_t j = i + 1; j < webs.size(); ++j) {
                if (checkOverlap(webs[i].lines, webs[j].lines)) {
                    interferenceGraph.addBidirectionalEdge(webs[i].id, webs[j].id, 0);
                }
            }
        }

        std::stack<int> S;
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
                    if (activeNodes.count(neighbor)) activeDegrees[neighbor]--;
                }
            } else {
                if (isSpilling && currentSpills < maxMods) {
                    int maxDegNode = -1, maxDeg = -1;
                    for (int node : activeNodes) {
                        if (activeDegrees[node] > maxDeg) { maxDeg = activeDegrees[node]; maxDegNode = node; }
                    }
                    std::cout << "-> Conflito! Spilling Web " << maxDegNode << " para memoria...\n";
                    spilledWebs.insert(maxDegNode);
                    activeNodes.erase(maxDegNode);
                    currentSpills++;
                    Vertex<int>* v = interferenceGraph.findVertex(maxDegNode);
                    for (auto edge : v->getAdj()) {
                        int neighbor = edge->getDest()->getInfo();
                        if (activeNodes.count(neighbor)) activeDegrees[neighbor]--;
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
    }

    // ========================================================
    // PASSO FINAL: ESCRITA DO FICHEIRO OUT.TXT
    // ========================================================
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
        std::cout << "\nWARNING: A alocacao falhou! A gravar tudo para memoria.\n";
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
    // Modo Batch (Script automático da professora)
    if (argc == 5 && std::string(argv[1]) == "-b") {
        runAllocation(argv[2], argv[3], argv[4], true);
        return 0;
    }

    // Modo Interativo (O teu menu)
    int choice;
    do {
        std::cout << "\n=== Ferramenta de Alocacao de Registos ===\n1. Executar Alocacao\n0. Sair\nEscolha: ";
        std::cin >> choice;
        if (choice == 1) {
            std::string ranges, regs, out;
            std::cout << "Ficheiro de ranges: "; std::cin >> ranges;
            std::cout << "Ficheiro de registos: "; std::cin >> regs;
            std::cout << "Nome do ficheiro de output (ex: out.txt): "; std::cin >> out;
            runAllocation(ranges, regs, out, false);
        }
    } while (choice != 0);
    return 0;
}