#include "UCI.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Constructor
// ============================================================================
UCI::UCI() {}

// ============================================================================
// UCI Loop — Main I/O loop
// ============================================================================
void UCI::loop() {
    std::string line;

    while (std::getline(std::cin, line)) {
        // Trim trailing whitespace/carriage return
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }

        if (line.empty()) continue;

        // Parse the first token
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "uci") {
            handleUci();
        } else if (token == "isready") {
            handleIsReady();
        } else if (token == "ucinewgame") {
            handleNewGame();
        } else if (token == "position") {
            handlePosition(line);
        } else if (token == "go") {
            handleGo(line);
        } else if (token == "quit") {
            handleQuit();
            return;
        } else if (token == "d" || token == "display") {
            // Non-standard: display the board (useful for debugging)
            std::cout << board_to_string(engine_.getBoard()) << std::endl;
        }
        // Ignore unknown commands
    }
}

// ============================================================================
// UCI Commands
// ============================================================================

void UCI::handleUci() {
    std::cout << "id name DeepChess-Week4" << std::endl;
    std::cout << "id author IITB-SoC" << std::endl;
    // No options for now
    std::cout << "uciok" << std::endl;
}

void UCI::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCI::handleNewGame() {
    engine_.newGame();
    engine_.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

// ============================================================================
// Position Command
//
// Formats:
//   position startpos
//   position startpos moves e2e4 e7e5 ...
//   position fen <fen> moves e2e4 ...
// ============================================================================
void UCI::handlePosition(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token; // consume "position"
    iss >> token;

    std::string fen;

    if (token == "startpos") {
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        // Check for "moves" token
        if (iss >> token && token != "moves") {
            // unexpected, ignore
        }
    } else if (token == "fen") {
        // Read the full FEN (6 fields)
        std::string fenParts;
        for (int i = 0; i < 6 && iss >> token; ++i) {
            if (token == "moves") {
                // FEN may have fewer parts; "moves" starts the move list
                break;
            }
            if (!fenParts.empty()) fenParts += " ";
            fenParts += token;
        }
        fen = fenParts;

        // If the last token read wasn't "moves", try to read it
        if (token != "moves") {
            // May or may not have moves
            if (iss >> token && token != "moves") {
                // Part of FEN? This shouldn't happen with a valid 6-field FEN
            }
        }
    } else {
        return; // Unknown format
    }

    // Collect all moves
    std::vector<std::string> moves;
    while (iss >> token) {
        moves.push_back(token);
    }

    // Set position
    engine_.setPosition(fen, moves);
}

// ============================================================================
// Go Command
//
// Formats:
//   go                        (search with default settings)
//   go depth N                (search to depth N)
//   go movetime N             (search for N milliseconds)
//   go wtime N btime N        (time controls)
//   go wtime N btime N winc N binc N
//   go infinite               (search until "stop")
// ============================================================================
void UCI::handleGo(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token; // consume "go"

    int depth = 0;
    int movetime = 0;
    int wtime = 0, btime = 0;
    int winc = 0, binc = 0;
    bool infinite = false;

    while (iss >> token) {
        if (token == "depth") {
            iss >> depth;
        } else if (token == "movetime") {
            iss >> movetime;
        } else if (token == "wtime") {
            iss >> wtime;
        } else if (token == "btime") {
            iss >> btime;
        } else if (token == "winc") {
            iss >> winc;
        } else if (token == "binc") {
            iss >> binc;
        } else if (token == "infinite") {
            infinite = true;
        }
    }

    // Calculate time allocation
    int timeLimitMs = 0;

    if (movetime > 0) {
        timeLimitMs = movetime;
    } else if (wtime > 0 || btime > 0) {
        // Simple time management: use 1/30th of remaining time + increment
        bool isWhite = (engine_.getBoard().sideToMove() == Color::WHITE);
        int ourTime = isWhite ? wtime : btime;
        int ourInc  = isWhite ? winc  : binc;

        timeLimitMs = ourTime / 30 + ourInc / 2;
        // Don't use more than 80% of remaining time
        if (timeLimitMs > ourTime * 4 / 5) {
            timeLimitMs = ourTime * 4 / 5;
        }
        // Minimum 100ms
        if (timeLimitMs < 100 && ourTime > 200) {
            timeLimitMs = 100;
        }
    } else if (!infinite && depth == 0) {
        // Default: search for 5 seconds
        timeLimitMs = 5000;
    }

    if (infinite) {
        timeLimitMs = 0;
    }

    if (depth == 0) depth = 64; // No depth limit

    // Set up info callback for UCI output
    engine_.onSearchInfo = [](int d, int score, long long nodes, int timeMs, const std::string& pv) {
        std::cout << "info depth " << d
                  << " score cp " << score
                  << " nodes " << nodes
                  << " time " << timeMs;
        if (timeMs > 0) {
            std::cout << " nps " << (nodes * 1000 / (timeMs > 0 ? timeMs : 1));
        }
        if (!pv.empty()) {
            std::cout << " pv " << pv;
        }
        std::cout << std::endl;
    };

    // Search
    Move bestMove = engine_.search(depth, timeLimitMs);

    // Output best move
    if (bestMove == Move::NO_MOVE) {
        std::cout << "bestmove 0000" << std::endl;
    } else {
        std::cout << "bestmove " << uci::moveToUci(bestMove) << std::endl;
    }
}

void UCI::handleQuit() {
    // Nothing to clean up
}

// ============================================================================
// Helper: Board display (for debug "d" command)
// ============================================================================
std::string board_to_string(const Board& board) {
    std::string result;
    result += "\n  +---+---+---+---+---+---+---+---+\n";
    for (int rank = 7; rank >= 0; --rank) {
        result += std::to_string(rank + 1) + " |";
        for (int file = 0; file < 8; ++file) {
            Square sq(file + rank * 8);
            Piece p = board.at(sq);
            std::string ps = static_cast<std::string>(p);
            if (ps == ".") ps = " ";
            result += " " + ps + " |";
        }
        result += "\n  +---+---+---+---+---+---+---+---+\n";
    }
    result += "    a   b   c   d   e   f   g   h\n";
    result += "\nFEN: " + board.getFen() + "\n";
    result += "Side: " + std::string(board.sideToMove() == Color::WHITE ? "White" : "Black") + "\n";
    return result;
}
