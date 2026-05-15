#include <iostream>
#include "RegisterAllocator.h"

void runAllocation(const std::string& rangesFile, const std::string& registersFile, const std::string& outputFile) {
    std::cout << "Loading data..." << std::endl;
    
    std::vector<Web> webs = parseRangesFile(rangesFile);
    Config config = parseRegistersFile(registersFile);

    std::cout << "Parsed " << webs.size() << " distinct webs." << std::endl;
    std::cout << "Config: " << config.numRegisters << " registers, Algorithm: " << config.algorithm << std::endl;

    // --- TODO: STEP 2 (Build Graph) and STEP 3 (Color Graph) GO HERE ---
    
    std::cout << "Results would be written to: " << outputFile << std::endl;
}

int main(int argc, char* argv[]) {
    // Check for Batch Mode: myProg -b ranges.txt registers.txt allocation.txt
    if (argc == 5 && std::string(argv[1]) == "-b") {
        std::string rangesFile = argv[2];
        std::string registersFile = argv[3];
        std::string outputFile = argv[4];
        
        runAllocation(rangesFile, registersFile, outputFile);
        return 0;
    }

    // Interactive Menu Mode
    int choice;
    do {
        std::cout << "\n=== Compiler Register Allocation Tool ===" << std::endl;
        std::cout << "1. Run Allocation" << std::endl;
        std::cout << "0. Exit" << std::endl;
        std::cout << "Enter choice: ";
        std::cin >> choice;

        if (choice == 1) {
            std::string ranges, regs, out;
            std::cout << "Enter ranges file path (e.g., ranges1.txt): ";
            std::cin >> ranges;
            std::cout << "Enter registers file path (e.g., registers2.txt): ";
            std::cin >> regs;
            std::cout << "Enter output file path (e.g., out.txt): ";
            std::cin >> out;

            runAllocation(ranges, regs, out);
        }
    } while (choice != 0);

    return 0;
}