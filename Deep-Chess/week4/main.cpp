#include "Engine.h"
#include "UCI.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

// ============================================================================
// Interactive Play Mode — play against the engine in the terminal
// ============================================================================
void playInteractive() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  DeepChess Engine — Interactive Mode\n";
    std::cout << "========================================\n\n";

    Engine engine;
    engine.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    std::string input;
    int thinkTime = 3000; // 3 seconds per move

    std::cout << "You play as White. Enter moves in UCI format (e.g., e2e4).\n";
    std::cout << "Commands: 'quit', 'new', 'fen <fen>', 'depth <N>', 'time <ms>'\n\n";

    // Display board helper
    auto displayBoard = [&]() {
        const Board& board = engine.getBoard();
        std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
        for (int rank = 7; rank >= 0; --rank) {
            std::cout << (rank + 1) << " |";
            for (int file = 0; file < 8; ++file) {
                Square sq(file + rank * 8);
                Piece p = board.at(sq);
                std::string ps = static_cast<std::string>(p);
                if (ps == ".") ps = " ";
                std::cout << " " << ps << " |";
            }
            std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
        }
        std::cout << "    a   b   c   d   e   f   g   h\n\n";

        if (board.sideToMove() == Color::WHITE) {
            std::cout << "White to move\n";
        } else {
            std::cout << "Black to move\n";
        }
    };

    displayBoard();

    while (true) {
        std::cout << "\nYour move: ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input == "quit" || input == "exit") break;

        if (input == "new") {
            engine.newGame();
            engine.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            displayBoard();
            continue;
        }

        if (input.substr(0, 4) == "fen ") {
            engine.setPosition(input.substr(4));
            displayBoard();
            continue;
        }

        if (input.substr(0, 5) == "time ") {
            thinkTime = std::stoi(input.substr(5));
            std::cout << "Think time set to " << thinkTime << " ms\n";
            continue;
        }

        if (input.substr(0, 6) == "depth ") {
            // Search to specific depth (no time limit)
            int d = std::stoi(input.substr(6));
            engine.onSearchInfo = [](int depth, int score, long long nodes, int timeMs, const std::string& pv) {
                std::cout << "  depth " << depth << " score " << score
                          << " nodes " << nodes << " time " << timeMs << "ms"
                          << " pv " << pv << "\n";
            };
            Move best = engine.search(d, 0);
            if (best != Move::NO_MOVE) {
                std::cout << "\nEngine plays: " << uci::moveToUci(best) << "\n";
                Board temp = engine.getBoard();
                std::cout << "  (" << uci::moveToSan(temp, best) << ")\n";
            }
            continue;
        }

        // Try to parse as a UCI move
        try {
            Board& board = const_cast<Board&>(engine.getBoard());
            Move userMove = uci::uciToMove(board, input);

            // Check if the move is legal
            Movelist legalMoves;
            movegen::legalmoves(legalMoves, board);
            bool isLegal = false;
            for (int i = 0; i < static_cast<int>(legalMoves.size()); ++i) {
                if (legalMoves[i] == userMove) {
                    isLegal = true;
                    break;
                }
            }

            if (!isLegal) {
                std::cout << "Illegal move! Try again.\n";
                continue;
            }

            // Make user's move
            board.makeMove(userMove);
            displayBoard();

            // Check for game over
            Movelist movesAfter;
            movegen::legalmoves(movesAfter, board);
            if (movesAfter.empty()) {
                if (board.inCheck()) {
                    std::cout << "\nCheckmate! You win!\n";
                } else {
                    std::cout << "\nStalemate! Draw.\n";
                }
                break;
            }

            // Engine's turn
            std::cout << "\nEngine thinking (" << thinkTime << "ms)...\n";

            engine.onSearchInfo = [](int depth, int score, long long nodes, int timeMs, const std::string& pv) {
                std::cout << "  depth " << depth << " score " << score
                          << " nodes " << nodes << " time " << timeMs << "ms\n";
            };

            Move engineMove = engine.search(0, thinkTime);

            if (engineMove == Move::NO_MOVE) {
                std::cout << "Engine has no legal moves.\n";
                break;
            }

            std::cout << "\nEngine plays: " << uci::moveToUci(engineMove);
            Board temp = engine.getBoard();
            std::cout << " (" << uci::moveToSan(temp, engineMove) << ")\n";

            board.makeMove(engineMove);
            displayBoard();

            // Check for game over after engine's move
            Movelist movesAfterEngine;
            movegen::legalmoves(movesAfterEngine, board);
            if (movesAfterEngine.empty()) {
                if (board.inCheck()) {
                    std::cout << "\nCheckmate! Engine wins!\n";
                } else {
                    std::cout << "\nStalemate! Draw.\n";
                }
                break;
            }

        } catch (const std::exception& e) {
            std::cout << "Invalid input: " << e.what() << "\n";
            std::cout << "Enter a move in UCI format (e.g., e2e4) or a command.\n";
        }
    }
}

// ============================================================================
// Quick self-test
// ============================================================================
void selfTest() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  DeepChess Engine — Self Test\n";
    std::cout << "========================================\n\n";

    Engine engine;

    // Test 1: Starting position
    {
        engine.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::cout << "Test 1: Starting position (depth 6)\n";
        auto start = std::chrono::high_resolution_clock::now();
        Move best = engine.search(6, 0);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "  Best move: " << uci::moveToUci(best) << "\n";
        std::cout << "  Score: " << engine.getLastScore() << " cp\n";
        std::cout << "  Nodes: " << engine.getNodesSearched() << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(1) << ms << " ms\n";
        std::cout << "  NPS: " << static_cast<long long>(engine.getNodesSearched() / (ms / 1000.0)) << "\n\n";
    }

    // Test 2: Mate in 1
    {
        engine.newGame();
        engine.setPosition("6k1/5ppp/8/8/8/8/8/R3K3 w Q - 0 1");
        std::cout << "Test 2: Mate in 1 (Ra8#)\n";
        Move best = engine.search(4, 0);
        Board temp = engine.getBoard();
        std::cout << "  Best move: " << uci::moveToUci(best)
                  << " (" << uci::moveToSan(temp, best) << ")\n";
        std::cout << "  Score: " << engine.getLastScore() << " cp\n\n";
    }

    // Test 3: Mate in 2
    {
        engine.newGame();
        engine.setPosition("2bqkbn1/2pppp2/np2N3/r3P1p1/p2N2B1/5Q2/PPPPKPP1/RNB2r2 w - - 0 1");
        std::cout << "Test 3: Mate in 2 (Qf7#)\n";
        Move best = engine.search(5, 0);
        Board temp = engine.getBoard();
        std::cout << "  Best move: " << uci::moveToUci(best)
                  << " (" << uci::moveToSan(temp, best) << ")\n";
        std::cout << "  Score: " << engine.getLastScore() << " cp\n\n";
    }

    // Test 4: Tactical position
    {
        engine.newGame();
        engine.setPosition("r1bqkb1r/pppppppp/2n2n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
        std::cout << "Test 4: Scotch Game position (depth 6)\n";
        auto start = std::chrono::high_resolution_clock::now();
        Move best = engine.search(6, 0);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        Board temp = engine.getBoard();
        std::cout << "  Best move: " << uci::moveToUci(best)
                  << " (" << uci::moveToSan(temp, best) << ")\n";
        std::cout << "  Score: " << engine.getLastScore() << " cp\n";
        std::cout << "  Nodes: " << engine.getNodesSearched() << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(1) << ms << " ms\n\n";
    }

    std::cout << "All tests completed.\n\n";
}

// ============================================================================
// Main — Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg1 = argv[1];

        if (arg1 == "--play" || arg1 == "-p") {
            playInteractive();
            return 0;
        }

        if (arg1 == "--test" || arg1 == "-t") {
            selfTest();
            return 0;
        }

        if (arg1 == "--help" || arg1 == "-h") {
            std::cout << R"(
  ____                    ____ _
 |  _ \  ___  ___ _ __  / ___| |__   ___  ___ ___
 | | | |/ _ \/ _ \ '_ \| |   | '_ \ / _ \/ __/ __|
 | |_| |  __/  __/ |_) | |___| | | |  __/\__ \__ \
 |____/ \___|\___| .__/ \____|_| |_|\___||___/___/
                  |_|
     Week 4 — Chess Engine (IITB SoC)
)" << std::endl;
            std::cout << "Usage:\n";
            std::cout << "  DeepChess                     Start UCI mode (for chess GUIs)\n";
            std::cout << "  DeepChess --play | -p         Play interactively in terminal\n";
            std::cout << "  DeepChess --test | -t         Run self-test\n";
            std::cout << "  DeepChess --help | -h         Show this help\n";
            return 0;
        }

        std::cerr << "Unknown argument: " << arg1 << "\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }

    // Default: UCI mode
    UCI uci;
    uci.loop();

    return 0;
}
