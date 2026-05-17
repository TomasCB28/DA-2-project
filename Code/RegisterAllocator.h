/**
 * @file RegisterAllocator.h
 * @brief Definição das estruturas de dados globais e assinaturas do parser.
 *
 * Este ficheiro contém as definições das estruturas fundamentais (LinePoint, Web, Config)
 * utilizadas pelo alocador de registos e declara as funções responsáveis pelo parsing.
 *
 * Faculdade de Engenharia da Universidade do Porto (FEUP)
 * Disciplina: Desenho de Algoritmos (DA) - Ano Letivo 2025/2026
 */

#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include <iostream>
#include <vector>
#include <string>
#include <set>

using namespace std;

/**
 * @brief Representa um ponto de execução específico (linha de código) na vida de uma variável.
 */
struct LinePoint {
    int number; ///< Número cardinal da linha de execução.
    char type;  ///< Tipo de evento: ' ' (neutro/uso), '+' (definição/início), '-' (fim de live range).

    /**
     * @brief Operador de ordenação estrita fraca para organização interna no std::set.
     * @param other Outro ponto de linha para comparação.
     * @return true Se o número da linha corrente for estritamente menor que o da outra.
     */
    bool operator<(const LinePoint& other) const {
        return number < other.number;
    }
};

/**
 * @brief Estrutura que define uma Web (unidade base de alocação que agrupa live ranges interligadas).
 */
struct Web {
    int id;                   ///< Identificador numérico único para representação como vértice no Grafo.
    string variableName;      ///< Nome literal da variável simbólica original (ex: "sum", "i").
    set<LinePoint> lines;     ///< Conjunto ordenado e único de linhas onde a variável se encontra ativa.
};

/**
 * @brief Estrutura de configuração carregada a partir dos parâmetros do sistema.
 */
struct Config {
    int numRegisters = 0;      ///< Quantidade total N de registos físicos disponíveis na arquitetura.
    string algorithm = "basic"; ///< Identificador da variante algorítmica ("basic", "spilling", "splitting").
    int k = 0;                 ///< Parâmetro numérico limite para a profundidade de partição (splitting).
};

/**
 * @brief Processa e extrai as configurações de hardware e software do ficheiro de registos.
 * @param filename Caminho relativo ou absoluto do ficheiro de configuração (ex: registers.txt).
 * @return Config Objeto preenchido com as diretivas de execução.
 * @note Complexidade Temporal: O(L), onde L é o número de linhas de texto processadas.
 */
Config parseRegistersFile(const string& filename);

/**
 * @brief Efetua a leitura das gamas e constrói as estruturas Web com resolução de fusões.
 * @param filename Caminho relativo ou absoluto do ficheiro de live ranges (ex: ranges.txt).
 * @return vector<Web> Coleção indexada contendo todas as variáveis simbólicas mapeadas em Webs prontas.
 * @note Complexidade Temporal: O(R * W * P * log P) no pior caso (ver detalhes em Parser.cpp).
 */
vector<Web> parseRangesFile(const string& filename);

#endif // REGISTER_ALLOCATOR_H