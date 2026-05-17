/**
 * @file main.cpp
 * @brief Algoritmos de Alocação de Registos para Compiladores.
 * * Este ficheiro contém a implementação do fluxo principal do alocador de
 * registos baseado no algoritmo de coloragem de grafos de Chaitin-Briggs.
 * Suporta as variantes: basic (T2.1), spilling inteligente (T2.2) e splitting (T2.3).
 * * Faculdade de Engenharia da Universidade do Porto (FEUP)
 * Disciplina: Desenho de Algoritmos (DA) - Spring 2026
 */

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

/**
 * @brief Verifica se duas webs se sobrepõem no tempo (interferem).
 * * @param set1 Conjunto de pontos de linha da primeira Web.
 * @param set2 Conjunto de pontos de linha da segunda Web.
 * @return true Se houver interseção entre as linhas de vida (interferência), false caso contrário.
 * * @note Complexidade Temporal: O(M + N), onde M e N são os tamanhos dos respetivos conjuntos.
 */
bool checkOverlap(const std::set<LinePoint>& set1, const std::set<LinePoint>& set2) {
    std::vector<LinePoint> intersection;
    std::set_intersection(set1.begin(), set1.end(),
                          set2.begin(), set2.end(),
                          std::back_inserter(intersection));
    return !intersection.empty();
}

/**
 * @brief Executa o algoritmo principal de alocação de registos.
 * * Controla todo o fluxo iterativo de construção do grafo de interferência,
 * simplificação por partição de graus, resolução de bloqueios (Spilling/Splitting)
 * e coloragem (Fase de Seleção). No final, exporta o mapeamento para o ficheiro de saída.
 * * @param rangesFile Caminho para o ficheiro de live ranges das variáveis.
 * @param registersFile Caminho para o ficheiro de configuração dos registos.
 * @param outputFile Caminho para o ficheiro onde será gravado o resultado.
 * @param isBatch Indicador de modo batch (true) ou modo interativo (false).
 * * @note Complexidade Temporal: O(W^3) no pior caso, onde W é o número de Webs.
 * A construção do grafo inicial e re-computação em caso de splits/spills domina o loop.
 */
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

    std::string finalPath = outputFile;
    int N = config.numRegisters;
    std::string algo = config.algorithm;
    int maxSplits = config.k;

    bool isSplitting = (algo == "splitting");
    int currentSplits = 0;
    bool allocationFailed = false;

    std::map<int, int> webToRegister;
    std::set<int> spilledWebs;

    // Loop Iterativo Principal do Otimizador
    while (true) {
        // 1. Construir o Grafo de Interferência
        // Complexidade: O(W^2 * L) onde L é o número de linhas da live range
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
        // Complexidade: O(W^2) para encontrar e remover nós com grau < N
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

                    // Atualizar de forma dinâmica o grau dos vizinhos ativos
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

        // 3. Resolução de Bloqueios (Spilling ou Splitting)
        if (stuck) {
            int targetNode = -1;

            if (algo == "spilling") {
                // ========================================================
                // HEURÍSTICA DE SPILLING INTELIGENTE (Tarefa T2.2)
                // ========================================================
                // Métrica de Chaitin: Custo (linhas vivas) / Grau Ativo
                // Escolhe o menor rácio para minimizar o custo global de spill na CPU
                double minSpillCostRatio = std::numeric_limits<double>::max();

                for (auto const& [node, deg] : activeDegrees) {
                    if (removedNodes.count(node) || spilledWebs.count(node)) continue;

                    int webSize = 1;
                    for (const auto& w : webs) {
                        if (w.id == node) {
                            webSize = w.lines.size();
                            break;
                        }
                    }

                    int degree = (deg > 0) ? deg : 1; // Prevenir divisão por zero
                    double spillCostRatio = static_cast<double>(webSize) / degree;

                    if (spillCostRatio < minSpillCostRatio) {
                        minSpillCostRatio = spillCostRatio;
                        targetNode = node;
                    }
                }
            } else {
                // ========================================================
                // CRITÉRIO GREEDY CRU (Tarefa T2.1 / T2.3)
                // ========================================================
                // Escolha clássica baseada puramente no maior grau absoluto
                int maxDeg = -1;
                for (auto const& [node, deg] : activeDegrees) {
                    if (removedNodes.count(node) || spilledWebs.count(node)) continue;
                    if (deg > maxDeg) {
                        maxDeg = deg;
                        targetNode = node;
                    }
                }
            }

            // Bloco de Splitting (Tarefa T2.3)
            if (isSplitting && currentSplits < maxSplits) {
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
                continue; // Reinicia o loop com o novo conjunto de webs alterado
            } else {
                spilledWebs.insert(targetNode);
                continue;
            }
        }

        // 4. Fase de Seleção (Coloragem Efetiva do Grafo)
        // Complexidade: O(W * R), onde R é o número de registos
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

    // 5. Escrita do Ficheiro de Output Estruturado
    // Complexidade: O(W * L)
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

/**
 * @brief Função main - Ponto de entrada da aplicação.
 * * Analisa os argumentos da linha de comandos (`argc` e `argv`) para decidir
 * se invoca o fluxo em Modo Batch de avaliação ou o Menu Interativo clássico.
 */
int main(int argc, char* argv[]) {
    // Modo Batch (Script de avaliação automática Unix de Submissão)
    if (argc >= 2 && std::string(argv[1]) == "-b") {
        if (argc != 5) {
            std::cerr << "Erro: Argumentos em falta para o modo batch.\nUso: myProg -b <ranges.txt> <registers.txt> <allocation.txt>" << std::endl;
            return 1;
        }
        runAllocation(argv[2], argv[3], argv[4], true);
        return 0;
    }

    // Modo Interativo (Menu de Utilizador)
    int choice = -1;
    do {
        std::cout << "\n=== Ferramenta de Alocacao de Registos ===\n1. Executar Alocacao\n0. Sair\nEscolha: ";
        if (!(std::cin >> choice)) {
            std::cerr << "Erro: Entrada de menu invalida!" << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (choice == 1)
        {
            std::string ranges, regs, out;
            std::cout << "Ficheiro de gamas: ex(datasets/ranges6.txt) ";
            std::cin >> ranges;
            std::cout << "Ficheiro de registos:ex(datasets/registers3.txt) ";
            std::cin >> regs;
            std::cout << "Nome do ficheiro de Saida: ";
            std::cin >> out;
        }
    } while (choice != 0);

    return 0;
}