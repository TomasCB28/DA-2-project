
#include "RegisterAllocator.h"
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

/**
 * @brief verifica se dois conjuntos de pontos se sobrepõem.
 *
 * Utiliza a ordenação nativa da estrutura set para calcular a interseção linear
 * entre duas gamas de tempo de vida de variáveis.
 *
 * @param set1 Primeiro conjunto de pontos
 * @param set2 Segundo conjunto de pontos
 * @return true Se houver sobreposição em pelo menos uma linha, false caso contrário.
 *
 * @note Complexidade Temporal: O(M + N), M e N são os tamanhos dos conjuntos de pontos.
 */
bool hasOverlap(const set<LinePoint>& set1, const set<LinePoint>& set2) {
    vector<LinePoint> intersection;
    set_intersection(set1.begin(), set1.end(),
                     set2.begin(), set2.end(),
                     back_inserter(intersection));
    return !intersection.empty();
}

/**
 * @brief
 * le os ficheiros com estrutura do tipo register,
 * retira o número de "registers" presentes, qual algoritmo aplicar (basic, spill, split (no split tambem o número máximo de cortes) ou free)
 *
 * @param filename Caminho do ficheiro.
 */
Config parseRegistersFile(const string& filename) {
    Config config;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << endl;
        return config;
    }

    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        string key;
        getline(ss, key, ':');

        if (key == "registers") {
            ss >> config.numRegisters;
        } else if (key == "algorithm") {
            string algoStr;
            getline(ss, algoStr);
            algoStr.erase(0, algoStr.find_first_not_of(" \t"));

            size_t commaPos = algoStr.find(',');
            if (commaPos != string::npos) {
                config.algorithm = algoStr.substr(0, commaPos);
                config.k = stoi(algoStr.substr(commaPos + 1));
            } else {
                config.algorithm = algoStr;
            }
        }
    }
    return config;
}

/**
 * @brief Efetua o parse dos ranges e constrói as Webs iniciais.
 *
 * Lê linha a linha as variáveis e os seus intervalos de atividade. caso haja sobreposiçao de ranges da mesma variável estes sao fundidos.
 * @param filename Caminho do ficheiro de texto contendo as live ranges das variáveis.
 * @return vector<Web> Vetor estruturado contendo todas as webs.
 *

 */
vector<Web> parseRangesFile(const string& filename) {
    vector<Web> webs;
    ifstream file(filename);
    string line;
    int nextId = 0;

    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << endl;
        return webs;
    }

    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        string varName, rangesStr;

        getline(ss, varName, ':');
        getline(ss, rangesStr);

        varName.erase(remove_if(varName.begin(), varName.end(), ::isspace), varName.end());

        set<LinePoint> currentLines;
        stringstream rangeStream(rangesStr);
        string token;

        while (getline(rangeStream, token, ',')) {
            token.erase(remove_if(token.begin(), token.end(), ::isspace), token.end());
            if (token.empty()) continue;

            LinePoint lp;
            lp.type = ' ';

            if (token.back() == '+') {
                lp.type = '+';
                token.pop_back();
            } else if (token.back() == '-') {
                lp.type = '-';
                token.pop_back();
            }

            lp.number = stoi(token);
            currentLines.insert(lp);
        }

        bool merged = false;
        for (auto& web : webs) {
            if (web.variableName == varName && hasOverlap(web.lines, currentLines)) {
                for (const auto& newLine : currentLines) {
                    auto it = web.lines.find(newLine);
                    if (it != web.lines.end()) {
                        if ((it->type == '-' && newLine.type == '+') || (it->type == '+' && newLine.type == '-')) {
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