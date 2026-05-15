cmake_minimum_required(VERSION 3.20)
project(DA2026_PRJ2)

# Definir o standard de C++
set(CMAKE_CXX_STANDARD 17)

# Ir buscar todos os ficheiros .cpp que estão dentro da pasta "code"
file(GLOB SOURCES "Code/*.cpp")

# Criar o executável
add_executable(DA2026_PRJ2 ${SOURCES})