#include "Engine.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

// Declare the batch solver function from PuzzleSolver.cpp
extern void solvePuzzleBatch(const std::string& filename, int mateInN);

// ============================================================================
// Interactive single-puzzle mode
// ============================================================================
void solveInteractive() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  DeepChess Puzzle Solver — Interactive\n";
    std::cout << "========================================\n\n";

    std::string fen;
    int mateInN;

    std::cout << "Enter FEN: ";
    std::getline(std::cin, fen);

    std::cout << "Mate in N (enter N): ";
    std::cin >> mateInN;

    if (mateInN < 1 || mateInN > 6) {
        std::cout << "Warning: Large N values may be slow. Proceeding...\n";
    }

    Engine engine(mateInN);
    engine.setPosition(fen);

    std::cout << "\nSearching for mate-in-" << mateInN << "...\n";

    auto start = std::chrono::high_resolution_clock::now();
    auto solution = engine.solve(mateInN);
    auto end = std::chrono::high_resolution_clock::now();

    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();

    if (solution.empty()) {
        std::cout << "\nNo mate-in-" << mateInN << " found from this position.\n";
    } else {
        std::cout << "\nSolution found!\n";
        std::cout << "Moves: " << engine.solutionToString(solution) << "\n";
    }

    std::cout << "Time:  " << std::fixed << std::setprecision(2) << timeMs << " ms\n";
    std::cout << "Nodes: " << engine.getNodesSearched() << "\n\n";
}

// ============================================================================
// Main — Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << R"(
  ____                    ____ _                   
 |  _ \  ___  ___ _ __  / ___| |__   ___  ___ ___ 
 | | | |/ _ \/ _ \ '_ \| |   | '_ \ / _ \/ __/ __|
 | |_| |  __/  __/ |_) | |___| | | |  __/\__ \__ \
 |____/ \___|\___| .__/ \____|_| |_|\___||___/___/
                 |_|                               
    Week 3 — Chess Puzzle Solver Engine (IITB SoC)
)" << std::endl;

    if (argc > 1) {
        std::string arg1 = argv[1];

        if (arg1 == "--interactive" || arg1 == "-i") {
            solveInteractive();
            return 0;
        }

        if (arg1 == "--help" || arg1 == "-h") {
            std::cout << "Usage:\n";
            std::cout << "  PuzzleSolver                     Run all mate-in-2 puzzles (default)\n";
            std::cout << "  PuzzleSolver --all               Run mate-in-2, 3, and 4 puzzles\n";
            std::cout << "  PuzzleSolver --mate2             Run mate-in-2 puzzles only\n";
            std::cout << "  PuzzleSolver --mate3             Run mate-in-3 puzzles only\n";
            std::cout << "  PuzzleSolver --mate4             Run mate-in-4 puzzles only\n";
            std::cout << "  PuzzleSolver --interactive | -i  Solve a single puzzle interactively\n";
            std::cout << "  PuzzleSolver --help | -h         Show this help\n";
            return 0;
        }

        if (arg1 == "--all") {
            solvePuzzleBatch("mate_in_2.json", 2);
            solvePuzzleBatch("mate_in_3.json", 3);
            solvePuzzleBatch("mate_in_4.json", 4);
            return 0;
        }

        if (arg1 == "--mate2") {
            solvePuzzleBatch("mate_in_2.json", 2);
            return 0;
        }

        if (arg1 == "--mate3") {
            solvePuzzleBatch("mate_in_3.json", 3);
            return 0;
        }

        if (arg1 == "--mate4") {
            solvePuzzleBatch("mate_in_4.json", 4);
            return 0;
        }

        std::cerr << "Unknown argument: " << arg1 << "\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }

    // Default: solve mate-in-2 puzzles (fastest, good demo)
    solvePuzzleBatch("mate_in_2.json", 2);

    return 0;
}
