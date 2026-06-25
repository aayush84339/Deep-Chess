#include "Engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <iomanip>

// ============================================================================
// JSON Puzzle Loader
//
// The mate_in_X.json files are simple JSON objects with:
//   key   = FEN string
//   value = expected solution string (for verification)
//
// We parse them manually (no external JSON library needed).
// ============================================================================

struct Puzzle {
    std::string fen;
    std::string expectedSolution;
};

/**
 * @brief Parses a simple JSON file of the form { "fen1": "sol1", "fen2": "sol2", ... }
 */
std::vector<Puzzle> loadPuzzlesFromJson(const std::string& filename) {
    std::vector<Puzzle> puzzles;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open puzzle file: " << filename << std::endl;
        return puzzles;
    }

    // Read the entire file into a string
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Simple JSON parser for {"key": "value", ...} format
    size_t pos = 0;
    while (pos < content.size()) {
        // Find the next key (quoted string)
        size_t keyStart = content.find('"', pos);
        if (keyStart == std::string::npos) break;
        keyStart++; // skip opening quote

        size_t keyEnd = content.find('"', keyStart);
        if (keyEnd == std::string::npos) break;

        std::string key = content.substr(keyStart, keyEnd - keyStart);

        // Find the value (quoted string after the colon)
        size_t colonPos = content.find(':', keyEnd);
        if (colonPos == std::string::npos) break;

        size_t valStart = content.find('"', colonPos);
        if (valStart == std::string::npos) break;
        valStart++; // skip opening quote

        size_t valEnd = content.find('"', valStart);
        if (valEnd == std::string::npos) break;

        std::string value = content.substr(valStart, valEnd - valStart);

        puzzles.push_back({key, value});
        pos = valEnd + 1;
    }

    return puzzles;
}

// ============================================================================
// Puzzle Solver — Batch Runner
// ============================================================================

void solvePuzzleBatch(const std::string& filename, int mateInN) {
    std::cout << "\n";
    std::cout << "========================================================\n";
    std::cout << "  Solving Mate-in-" << mateInN << " Puzzles\n";
    std::cout << "  File: " << filename << "\n";
    std::cout << "========================================================\n\n";

    auto puzzles = loadPuzzlesFromJson(filename);

    if (puzzles.empty()) {
        std::cout << "No puzzles loaded!\n";
        return;
    }

    std::cout << "Loaded " << puzzles.size() << " puzzles.\n\n";

    Engine engine(mateInN);
    int solved = 0;
    int failed = 0;
    long long totalNodes = 0;

    auto batchStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < puzzles.size(); ++i) {
        const auto& puzzle = puzzles[i];

        engine.setPosition(puzzle.fen);
        
        auto start = std::chrono::high_resolution_clock::now();
        auto solution = engine.solve(mateInN);
        auto end = std::chrono::high_resolution_clock::now();

        double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
        long long nodes = engine.getNodesSearched();
        totalNodes += nodes;

        std::string solStr = engine.solutionToString(solution);

        // Display result
        std::cout << "Puzzle " << std::setw(3) << (i + 1) << "/" << puzzles.size() << ": ";

        if (solution.empty()) {
            std::cout << "FAILED";
            failed++;
        } else {
            std::cout << "SOLVED";
            solved++;
        }

        std::cout << " | " << std::fixed << std::setprecision(1) << timeMs << " ms"
                  << " | " << nodes << " nodes"
                  << " | " << solStr << "\n";

        // Show expected solution for comparison
        std::cout << "         Expected: " << puzzle.expectedSolution << "\n";

        // Print FEN for failed puzzles for debugging
        if (solution.empty()) {
            std::cout << "         FEN: " << puzzle.fen << "\n";
        }

        std::cout << "\n";
    }

    auto batchEnd = std::chrono::high_resolution_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(batchEnd - batchStart).count();

    // Summary
    std::cout << "========================================================\n";
    std::cout << "  RESULTS SUMMARY\n";
    std::cout << "========================================================\n";
    std::cout << "  Total puzzles:  " << puzzles.size() << "\n";
    std::cout << "  Solved:         " << solved << "\n";
    std::cout << "  Failed:         " << failed << "\n";
    std::cout << "  Success rate:   " << std::fixed << std::setprecision(1) 
              << (100.0 * solved / puzzles.size()) << "%\n";
    std::cout << "  Total time:     " << std::fixed << std::setprecision(1) 
              << totalTimeMs << " ms\n";
    std::cout << "  Total nodes:    " << totalNodes << "\n";
    std::cout << "  Avg time/puzzle:" << std::fixed << std::setprecision(1)
              << (totalTimeMs / puzzles.size()) << " ms\n";
    std::cout << "========================================================\n\n";
}
