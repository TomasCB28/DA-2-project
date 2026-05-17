#include <iostream>
#include <fstream>
#include <algorithm>
#include <stack>
#include <map>
#include <vector>
#include <limits>
#include "RegisterAllocator.h"
#include "Graph.h"

using namespace std;

/**
 * @brief Verifica se duas Webs se sobrepõem.
 *
 *@param set1 elementos da primeira Web.
 * @param set2 elementos da segunda Web.
 * @return true Se as duas webs partilharem pelo menos um elemento, false caso contrário.
 *
 * @note Complexidade Temporal: O(M + N), onde M e N representam o número de elementos de cada web.
 */
bool checkOverlap(const set<LinePoint>& set1, const set<LinePoint>& set2) {
    vector<LinePoint> intersection;
    set_intersection(set1.begin(), set1.end(),
                     set2.begin(), set2.end(),
                     back_inserter(intersection));
    return !intersection.empty();
}

/**
 * @brief
 *
 * duas diferentes abordagens:
 * 1. LINEAR SCAN:
 * - Ordena as variáveis pelo momento em que nascem e avança no tempo.
 * - Se faltarem registos, sacrifica para a memória (spill) a variável
 * que acaba mais tarde (heurística de Poletto & Sarkar).
 *
* * 2. COLARAÇÃO DE GRAFOS:
 * - Grafo: Liga variáveis (nós) que colidem no tempo.
 * - Simplificação: Remove e empilha nós com grau < N.
 * - Bloqueios (se o grafo encravar):
 * > basic: Falha e desiste imediatamente.
 * > spilling: vai para a memória o nó com maior grau
 * > splitting: Corta a variável ao meio e reconstrói o grafo do zero.
 * - Seleção: Desempilha e atribui o primeiro registo livre.
 *
 * @note Complexidade Temporal:
 * - O(W log W) para a variante 'free' (Linear Scan), onde W é o número de Webs.
 * - O(W^3) no pior caso para Coloração de Grafos.
 */
void runAllocation(const string& rangesFile, const string& registersFile, string outputFile, bool isBatch) {
    cout << "Loading data..." << endl;

    vector<Web> webs = parseRangesFile(rangesFile);
    Config config = parseRegistersFile(registersFile);

    if (webs.empty() || config.numRegisters <= 0) {
        cerr << "Erro Execucao: Dados invalidos ou registos a 0." << endl;
        return;
    }

    string finalPath = outputFile;
    int N = config.numRegisters;
    string algo = config.algorithm;
    int maxSplits = config.k;

    bool isSplitting = (algo == "splitting");
    bool isSpilling = (algo == "spilling");

    map<int, int> webToRegister;
    set<int> spilledWebs;
    bool allocationFailed = false;


    if (algo == "free") {
        cout << "-> A executar Algoritmo Livre: Linear Scan (Poletto & Sarkar)..." << endl;

        vector<Web> sortedWebs = webs;
        sort(sortedWebs.begin(), sortedWebs.end(), [](const Web& a, const Web& b) {
            return a.lines.begin()->number < b.lines.begin()->number;
        });

        vector<Web> active;
        set<int> freeRegisters;
        for (int i = 0; i < N; ++i) freeRegisters.insert(i);

        for (const auto& w : sortedWebs) {
            int start_w = w.lines.begin()->number;
            int end_w = w.lines.rbegin()->number;


            for (auto it = active.begin(); it != active.end(); ) {
                if (it->lines.rbegin()->number < start_w) {
                    freeRegisters.insert(webToRegister[it->id]);
                    it = active.erase(it);
                } else {
                    ++it;
                }
            }


            if (active.size() == (size_t)N) {
                auto spillIt = active.begin();
                int max_end = spillIt->lines.rbegin()->number;

                for (auto it = active.begin(); it != active.end(); ++it) {
                    if (it->lines.rbegin()->number > max_end) {
                        max_end = it->lines.rbegin()->number;
                        spillIt = it;
                    }
                }

                if (max_end > end_w) {
                    cout << "   Linear Scan: Spilling Web " << spillIt->id
                         << " (Dura ate " << max_end << " | Roubando registo para o Web " << w.id << ").\n";
                    spilledWebs.insert(spillIt->id);
                    int freedReg = webToRegister[spillIt->id];
                    webToRegister.erase(spillIt->id);
                    active.erase(spillIt);

                    webToRegister[w.id] = freedReg;
                    active.push_back(w);
                } else {
                    cout << "   Linear Scan: Spilling Web " << w.id
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

    else {
        int currentSplits = 0;
        bool success = false;
        int nextWebId = 1000;

        while (!success && !allocationFailed) {
            Graph<int> interferenceGraph;
            for (const auto& web : webs) interferenceGraph.addVertex(web.id);

            for (size_t i = 0; i < webs.size(); ++i) {
                for (size_t j = i + 1; j < webs.size(); ++j) {
                    if (checkOverlap(webs[i].lines, webs[j].lines)) {
                        interferenceGraph.addBidirectionalEdge(webs[i].id, webs[j].id, 0.0);
                    }
                }
            }

            stack<int> S;
            set<int> activeNodes;
            map<int, int> activeDegrees;

            for (auto v : interferenceGraph.getVertexSet()) {
                if (spilledWebs.count(v->getInfo())) continue;
                activeNodes.insert(v->getInfo());
                int deg = 0;
                for(auto e : v->getAdj()) {
                    if(!spilledWebs.count(e->getDest()->getInfo())) deg++;
                }
                activeDegrees[v->getInfo()] = deg;
            }

            bool encravou = false;
            int targetNode = -1;

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
                        if (activeDegrees[node] > maxDeg) {
                            maxDeg = activeDegrees[node];
                            targetNode = node;
                        }
                    }
                    break;
                }
            }

            if (encravou) {
                if (isSplitting && currentSplits < maxSplits) {
                    auto it = find_if(webs.begin(), webs.end(), [targetNode](const Web& w){ return w.id == targetNode; });
                    if (it != webs.end() && it->lines.size() > 1) {
                        cout << "-> Conflito! A dividir o Web " << targetNode << " ao meio...\n";
                        Web original = *it;
                        webs.erase(it);

                        vector<LinePoint> linhas(original.lines.begin(), original.lines.end());
                        size_t meio = linhas.size() / 2;

                        Web w1, w2;
                        w1.id = nextWebId++; w1.variableName = original.variableName;
                        w2.id = nextWebId++; w2.variableName = original.variableName;
                        w1.lines.insert(linhas.begin(), linhas.begin() + meio);
                        w2.lines.insert(linhas.begin() + meio, linhas.end());

                        webs.push_back(w1);
                        webs.push_back(w2);
                        currentSplits++;
                    } else {
                        allocationFailed = true;
                    }
                } else if (isSpilling) {
                    cout << "-> Conflito! Spilling Web " << targetNode << " para memoria...\n";
                    spilledWebs.insert(targetNode);
                } else {
                    allocationFailed = true;
                }
            } else {
                webToRegister.clear();
                while (!S.empty()) {
                    int node = S.top(); S.pop();
                    set<int> usedColors;
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

    ofstream outFile(finalPath);
    if (!outFile.is_open()) {
        cerr << "Erro Execucao: Nao foi possivel criar o ficheiro de Saida." << endl;
        return;
    }

    outFile << "webs: " << webs.size() << "\n";
    for (const auto& web : webs) {
        outFile << "web" << web.id << ": ";
        for (auto it = web.lines.begin(); it != web.lines.end(); ++it) {
            outFile << it->number;
            if (it->type != ' ') outFile << it->type;
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
    cout << "SUCESSO! Ficheiro criado em: " << finalPath << endl;
}

/**
 *  @brief Ponto de entrada da aplicação.
 * Encarrega-se de gerir o fluxo da consola e direcionar os ficheiros.
 *
 * @param n Contador de argumentos passados via terminal.
 * @param args Vetor de strings contendo os parâmetros da linha de comandos.
 * @return int Retorna 0 em caso de sucesso, ou 1 se houver erros de parametrização.
 */
int main(int n, char* args[]) {
    if (n >= 2 && string(args[1]) == "-b") {
        if (n != 5) {
            cerr << "Erro: Argumentos em falta para o modo batch.\nUso: myProg -b <ranges.txt> <registers.txt> <allocation.txt>" << endl;
            return 1;
        }
        runAllocation(args[2], args[3], args[4], true);
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
            cout << "Ficheiro de gamas (ex: ../datasets/ranges1.txt): "; cin >> ranges;
            cout << "Ficheiro de registos (ex: ../datasets/registers1.txt): "; cin >> regs;
            cout << "Nome do ficheiro de Saida: "; cin >> out;
            runAllocation(ranges, regs, out, false);
        }
    } while (choice != 0);

    return 0;
}