#include "Engine.h"
#include <iostream>
#include <algorithm>

// ============================================================================
// Constructor
// ============================================================================
Engine::Engine(int maxDepth)
    : maxDepth_(maxDepth), nodesSearched_(0) {}

// ============================================================================
// Position Setup
// ============================================================================
void Engine::setPosition(const std::string& fen) {
    board_ = Board(fen);
}

const Board& Engine::getBoard() const {
    return board_;
}

long long Engine::getNodesSearched() const {
    return nodesSearched_;
}

// ============================================================================
// Minimax with Alpha-Beta Pruning (mate-finder variant)
// ============================================================================
//
// In a mate puzzle the "maximizing" player is the side that delivers checkmate.
// We assign:
//   +MATE_SCORE  (minus depth offset) for checkmate in favour of the solver
//   -MATE_SCORE  (plus  depth offset) for checkmate against the solver
//    0           for stalemate / draw / depth exhausted
//
// The depth offset rewards shorter mates (checkmate sooner = higher score).
// ============================================================================

int Engine::minimax(int depth, int alpha, int beta, bool maximizing) {
    nodesSearched_++;

    // Generate all legal moves
    Movelist moves;
    movegen::legalmoves(moves, board_);

    // Terminal node checks
    if (moves.empty()) {
        if (board_.inCheck()) {
            // Checkmate! The side to move is mated.
            // If maximizing, it means the solver got mated (bad).
            // If minimizing, it means the opponent got mated (good for solver).
            if (maximizing) {
                return -MATE_SCORE + (maxDepth_ * 2 - depth); // solver is mated (bad)
            } else {
                return MATE_SCORE - (maxDepth_ * 2 - depth);  // opponent is mated (good)
            }
        }
        // Stalemate
        return 0;
    }

    // Depth exhausted — no mate found in given depth
    if (depth <= 0) {
        return 0;
    }

    if (maximizing) {
        int bestScore = -INF_SCORE;
        for (int i = 0; i < moves.size(); ++i) {
            board_.makeMove(moves[i]);
            int score = minimax(depth - 1, alpha, beta, false);
            board_.unmakeMove(moves[i]);

            bestScore = std::max(bestScore, score);
            alpha = std::max(alpha, score);
            if (beta <= alpha) break; // Beta cutoff
        }
        return bestScore;
    } else {
        int bestScore = INF_SCORE;
        for (int i = 0; i < moves.size(); ++i) {
            board_.makeMove(moves[i]);
            int score = minimax(depth - 1, alpha, beta, true);
            board_.unmakeMove(moves[i]);

            bestScore = std::min(bestScore, score);
            beta = std::min(beta, score);
            if (beta <= alpha) break; // Alpha cutoff
        }
        return bestScore;
    }
}

// ============================================================================
// Find Best Move at root
// ============================================================================
Move Engine::findBestMove(int depth, bool maximizing) {
    Movelist moves;
    movegen::legalmoves(moves, board_);

    if (moves.empty()) return Move(Move::NO_MOVE);

    Move bestMove(Move::NO_MOVE);

    if (maximizing) {
        int bestScore = -INF_SCORE;
        for (int i = 0; i < moves.size(); ++i) {
            board_.makeMove(moves[i]);
            int score = minimax(depth - 1, -INF_SCORE, INF_SCORE, false);
            board_.unmakeMove(moves[i]);

            if (score > bestScore) {
                bestScore = score;
                bestMove  = moves[i];
            }
        }
    } else {
        int bestScore = INF_SCORE;
        for (int i = 0; i < moves.size(); ++i) {
            board_.makeMove(moves[i]);
            int score = minimax(depth - 1, -INF_SCORE, INF_SCORE, true);
            board_.unmakeMove(moves[i]);

            if (score < bestScore) {
                bestScore = score;
                bestMove  = moves[i];
            }
        }
    }

    return bestMove;
}

// ============================================================================
// Solve a mate-in-N puzzle
// ============================================================================
//
// A mate-in-N has (2*N - 1) half-moves (plies):
//   mate-in-2 => 3 plies  (move, response, mating move)
//   mate-in-3 => 5 plies
//   mate-in-4 => 7 plies
//
// We search at depth = 2*N - 1 plies.
// The first move is always the "maximizing" player's (the puzzle solver).
//
// We reconstruct the full principal variation (PV) by replaying
// the best moves from each side.
// ============================================================================

std::vector<Move> Engine::solve(int mateInN) {
    nodesSearched_ = 0;
    int searchDepth = 2 * mateInN - 1; // plies

    std::vector<Move> pv; // principal variation

    // Save a copy of the board to restore later
    Board savedBoard = board_;

    for (int ply = 0; ply < searchDepth; ++ply) {
        bool maximizing = (ply % 2 == 0); // solver moves on even plies
        int remainingDepth = searchDepth - ply;

        Move best = findBestMove(remainingDepth, maximizing);

        if (best == Move(Move::NO_MOVE)) {
            // Could not find a continuation — puzzle unsolvable at this depth?
            break;
        }

        pv.push_back(best);
        board_.makeMove(best);

        // Check if we've reached checkmate
        Movelist legalMoves;
        movegen::legalmoves(legalMoves, board_);
        if (legalMoves.empty() && board_.inCheck()) {
            // Checkmate achieved!
            break;
        }
    }

    // Restore original board state
    board_ = savedBoard;

    return pv;
}

// ============================================================================
// Convert solution to human-readable SAN string
// ============================================================================
std::string Engine::solutionToString(const std::vector<Move>& solution) {
    if (solution.empty()) return "(no solution found)";

    std::string result;
    Board tempBoard = board_;

    for (size_t i = 0; i < solution.size(); ++i) {
        if (i > 0) result += " ";
        result += uci::moveToSan(tempBoard, solution[i]);
        tempBoard.makeMove(solution[i]);
    }

    return result;
}
