/**
* @file RegisterAllocator.h
 * @brief Estruturas de dados e assinaturas para a alocação de registos.
 */

#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include <iostream>
#include <vector>
#include <string>
#include <set>

using namespace std;


struct LinePoint {
    int number;
    char type;  ///< neutro, inicio (+), fim(-).


    bool operator<(const LinePoint& other) const {
        return number < other.number;
    }
};

/**
 * @brief Estrutura das Webs
 */
struct Web {
    int id;
    string variableName;      ///< Nome web.
    set<LinePoint> lines;     ///< Conjunto de linhas onde a variável se encontra ativa.
};

/**
 * @brief Estrutura de configuração carregada a partir dos parâmetros do sistema.
 */
struct Config {
    int numRegisters = 0;      ///< Quantidade total N de registos físicos.
    string algorithm = "basic"; ///< Identificador da variante algorítmica ("basic", spilling, splitting ou free).
    int k = 0;                 ///< caso seja splitting, é o numero máximo de partições
};


Config parseRegistersFile(const string& filename);


vector<Web> parseRangesFile(const string& filename);

#endif // REGISTER_ALLOCATOR_H