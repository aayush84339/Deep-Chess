#ifndef ENGINE_H
#define ENGINE_H

#include "chess-library/include/chess.hpp"
#include <string>
#include <vector>
#include <limits>
#include <utility>

using namespace chess;

/**
 * @brief Chess Puzzle Solver Engine
 * 
 * Uses Minimax with Alpha-Beta pruning to solve mate-in-N puzzles.
 * Designed as an OOP class following SoC Week 3 guidelines.
 */
class Engine {
public:
    // Constructor: optionally set max search depth
    Engine(int maxDepth = 10);

    // Set the board position from a FEN string
    void setPosition(const std::string& fen);

    // Get the current board (const reference)
    const Board& getBoard() const;

    /**
     * @brief Solve a mate-in-N puzzle.
     * @param mateInN Number of moves to checkmate (e.g., 2 for mate-in-2)
     * @return A vector of Move objects representing the solution line,
     *         or an empty vector if no solution was found.
     * 
     * The solution alternates between the side-to-move's best move
     * and the opponent's best defense.
     */
    std::vector<Move> solve(int mateInN);

    /**
     * @brief Convert a solution line to a human-readable string.
     * @param solution The vector of moves from solve()
     * @return String like "Nf6+ gxf6 Bxf7#"
     */
    std::string solutionToString(const std::vector<Move>& solution);

    // Get the number of nodes searched in the last solve() call
    long long getNodesSearched() const;

private:
    Board board_;
    int maxDepth_;
    long long nodesSearched_;

    // Evaluation scores
    static constexpr int MATE_SCORE   = 100000;
    static constexpr int INF_SCORE    = std::numeric_limits<int>::max();

    /**
     * @brief Minimax with alpha-beta pruning for mate search.
     * @param depth      Remaining depth (in plies)
     * @param alpha      Alpha bound
     * @param beta       Beta bound
     * @param maximizing true if it's the puzzle-solver's turn (side that delivers mate)
     * @return evaluation score
     */
    int minimax(int depth, int alpha, int beta, bool maximizing);

    /**
     * @brief Find the best move at the root of the search tree.
     * @param depth      Search depth in plies
     * @param maximizing true if it's the puzzle-solver's turn
     * @return The best move found, or Move::NO_MOVE if none
     */
    Move findBestMove(int depth, bool maximizing);
};

#endif // ENGINE_H
