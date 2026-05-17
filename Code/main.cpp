/**
 * @file main.cpp
 * @brief Implementação do fluxo principal do alocador de registos.
 *
 * Contém a lógica iterativa de coloragem de grafos baseada no algoritmo de Chaitin-Briggs.
 * Suporta as variantes de alocação básica (T2.1), alocação com spilling inteligente (T2.2)
 * e alocação com splitting (T2.3).
 *
 * Faculdade de Engenharia da Universidade do Porto (FEUP)
 * Disciplina: Desenho de Algoritmos (DA) - Ano Letivo 2025/2026
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

using namespace std;

/**
 * @brief Deteta interseções temporárias entre duas Webs de variáveis.
 *
 * Executa uma operação de interseção linear ordenada entre as duas coleções
 * de pontos de linha para determinar se existe uma sobreposição (interferência).
 *
 * @param set1 Conjunto de pontos de linha da primeira Web.
 * @param set2 Conjunto de pontos de linha da segunda Web.
 * @return true Se as duas webs partilharem pelo menos um ponto de linha, false caso contrário.
 *
 * @note Complexidade Temporal: O(M + N), onde M e N representam o número de elementos em cada conjunto.
 */
bool checkOverlap(const set<LinePoint>& set1, const set<LinePoint>& set2) {
    vector<LinePoint> intersection;
    set_intersection(set1.begin(), set1.end(),
                     set2.begin(), set2.end(),
                     back_inserter(intersection));
    return !intersection.empty();
}

/**
 * @brief Orquestra o ciclo completo de alocação de registos por coloragem de grafos.
 *
 * Realiza o parse das entradas, monta dinamicamente o grafo de interferência,
 * efetua a simplificação heurística e aplica os mecanismos de spilling/splitting
 * em caso de bloqueio. Conclui com a atribuição de registos (fase de seleção) e gravação de dados.
 *
 * @param rangesFile Caminho relativo para o ficheiro com as gamas de vida (live ranges).
 * @param registersFile Caminho relativo para o ficheiro com a configuração de registos e algoritmo.
 * @param outputFile Nome ou caminho do ficheiro de texto a gerar com o resultado da alocação.
 * @param isBatch Define se a execução ocorre por script em segundo plano (true) ou interativamente (false).
 *
 * @note Complexidade Temporal: O(W^3) no pior caso, onde W é o número de Webs em processamento.
 * O loop repete-se a cada alteração estrutural feita pelos algoritmos de splitting ou spilling.
 */
void runAllocation(const string& rangesFile, const string& registersFile, string outputFile, bool isBatch) {
    cout << "Loading data..." << endl;

    vector<Web> webs = parseRangesFile(rangesFile);
    Config config = parseRegistersFile(registersFile);

    if (webs.empty()) {
        cerr << "Erro Execucao: Dados invalidos ou nenhuma Web encontrada." << endl;
        return;
    }
    if (config.numRegisters <= 0) {
        cerr << "Erro Execucao: Quantidade invalida de registos fornecida." << endl;
        return;
    }

    string finalPath = outputFile;
    int N = config.numRegisters;
    string algo = config.algorithm;
    int maxSplits = config.k;

    bool isSplitting = (algo == "splitting");
    int currentSplits = 0;
    bool allocationFailed = false;

    map<int, int> webToRegister;
    set<int> spilledWebs;

    while (true) {
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

        map<int, int> activeDegrees;
        map<int, set<int>> adjList;

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

        stack<int> simplifyStack;
        set<int> removedNodes;
        bool stuck = false;

        while (removedNodes.size() + spilledWebs.size() < webs.size()) {
            bool found = false;
            for (auto const& [node, deg] : activeDegrees) {
                if (removedNodes.count(node) || spilledWebs.count(node)) continue;

                if (deg < N) {
                    simplifyStack.push(node);
                    removedNodes.insert(node);

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

        if (stuck) {
            int targetNode = -1;

            if (algo == "spilling") {
                double minSpillCostRatio = numeric_limits<double>::max();

                for (auto const& [node, deg] : activeDegrees) {
                    if (removedNodes.count(node) || spilledWebs.count(node)) continue;

                    int webSize = 1;
                    for (const auto& w : webs) {
                        if (w.id == node) {
                            webSize = w.lines.size();
                            break;
                        }
                    }

                    int degree = (deg > 0) ? deg : 1;
                    double spillCostRatio = static_cast<double>(webSize) / degree;

                    if (spillCostRatio < minSpillCostRatio) {
                        minSpillCostRatio = spillCostRatio;
                        targetNode = node;
                    }
                }
            } else {
                int maxDeg = -1;
                for (auto const& [node, deg] : activeDegrees) {
                    if (removedNodes.count(node) || spilledWebs.count(node)) continue;
                    if (deg > maxDeg) {
                        maxDeg = deg;
                        targetNode = node;
                    }
                }
            }

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
                    vector<LinePoint> linhas(original.lines.begin(), original.lines.end());
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

        webToRegister.clear();
        while (!simplifyStack.empty()) {
            int node = simplifyStack.top();
            simplifyStack.pop();

            set<int> usedRegisters;
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

    ofstream outFile(finalPath);
    if (!outFile.is_open()) {
        cerr << "Erro Execucao: Nao foi possivel criar o ficheiro de saida." << std::endl;
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
            outFile << (next(it) == web.lines.end() ? "" : ",");
        }
        outFile << "\n";
    }

    if (allocationFailed) {
        cerr << "WARNING: A alocacao aos " << N << " registos fornecidos foi impossivel." << endl;
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
    cout << "SUCESSO! Ficheiro gerado." << endl;
}

/**
 * @brief Ponto de entrada da aplicação.
 *
 * Avalia os vetores de argumentos primitivos vindos do terminal. Se encontrar a flag `-b`,
 * direciona o processamento para o modo batch automático de testes; caso contrário,
 * abre o menu de texto interativo padrão para o utilizador.
 *
 * @param argc Contador de argumentos fornecidos via shell.
 * @param argv Vetor de strings contendo os argumentos literais da linha de comandos.
 * @return int Retorna 0 em caso de execução bem-sucedida, ou 1 se existirem erros de parametrização.
 *
 * @note Complexidade Temporal: O(1) no processamento inicial das opções de menu.
 */
int main(int argc, char* argv[]) {
    if (argc >= 2 && string(argv[1]) == "-b") {
        if (argc != 5) {
            cerr << "Erro: Argumentos em falta para o modo batch.\nUso: myProg -b <ranges.txt> <registers.txt> <allocation.txt>" << endl;
            return 1;
        }
        runAllocation(argv[2], argv[3], argv[4], true);
        return 0;
    }

    int choice = -1;
    do {
        cout << "\n=== Ferramenta de Alocacao de Registos ===\n1. Executar Alocacao\n0. Sair\nEscolha: ";
        if (!(cin >> choice)) {
            cerr << "Erro: Entrada de menu invalida!" << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        if (choice == 1) {
            string ranges, regs, out;
            cout << "Ficheiro de gamas (ex: datasets/ranges6.txt): ";
            cin >> ranges;
            cout << "Ficheiro de registos (ex: datasets/registers3.txt): ";
            cin >> regs;
            cout << "Nome do ficheiro de Saida: ";
            cin >> out;

            runAllocation(ranges, regs, out, false);
        }
    } while (choice != 0);

    return 0;
}