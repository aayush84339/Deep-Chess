#ifndef ENGINE_H
#define ENGINE_H

#include "../week3/chess-library/include/chess.hpp"
#include <string>
#include <vector>
#include <limits>
#include <utility>
#include <functional>
#include <chrono>
#include <cstdint>
#include <algorithm>

using namespace chess;

// ============================================================================
// Transposition Table Entry
// ============================================================================
enum TTFlag : uint8_t {
    TT_EXACT = 0, // PV node — score is exact
    TT_ALPHA = 1, // All node — score is upper bound (failed low)
    TT_BETA  = 2  // Cut node — score is lower bound (failed high)
};

struct TTEntry {
    uint64_t key   = 0;
    int      depth = 0;
    int      score = 0;
    TTFlag   flag  = TT_EXACT;
    Move     best  = Move::NO_MOVE;
};

// ============================================================================
// Chess Engine
// ============================================================================
class Engine {
public:
    Engine();

    // Set the board from a FEN string
    void setPosition(const std::string& fen);

    // Set position from FEN + apply a list of UCI moves
    void setPosition(const std::string& fen, const std::vector<std::string>& moves);

    // Clear transposition table and history for a new game
    void newGame();

    // Get the current board
    const Board& getBoard() const;

    /**
     * @brief Search for the best move.
     * @param maxDepth  Maximum search depth (in plies). 0 = use default (64).
     * @param timeLimitMs  Time limit in milliseconds. 0 = no limit.
     * @return The best move found.
     *
     * Uses iterative deepening with alpha-beta pruning, quiescence search,
     * transposition table, move ordering, and check extensions.
     */
    Move search(int maxDepth = 0, int timeLimitMs = 0);

    // Get search statistics
    long long getNodesSearched() const;
    int getLastScore() const;
    int getLastDepth() const;

    // Info callback: (depth, score, nodes, timeMs, pvString)
    std::function<void(int, int, long long, int, const std::string&)> onSearchInfo;

private:
    Board board_;

    // Search stats
    long long nodesSearched_;
    int lastScore_;
    int lastDepth_;

    // Time management
    std::chrono::time_point<std::chrono::high_resolution_clock> searchStart_;
    int timeLimitMs_;
    bool timeUp_;

    // Transposition table (fixed size)
    static constexpr int TT_SIZE = 1 << 20; // ~1M entries ≈ 32MB
    std::vector<TTEntry> tt_;

    // Killer moves: 2 slots per ply
    static constexpr int MAX_PLY = 128;
    Move killers_[MAX_PLY][2];

    // History heuristic: [color][from_sq][to_sq]
    int history_[2][64][64];

    // Evaluation scores
    static constexpr int MATE_SCORE = 100000;
    static constexpr int INF_SCORE  = 100001;
    static constexpr int DRAW_SCORE = 0;

    // ========================================================================
    // Search
    // ========================================================================

    /**
     * @brief Negamax with alpha-beta pruning.
     * Always returns score from the perspective of the side to move.
     */
    int negamax(int depth, int alpha, int beta, int ply);

    /**
     * @brief Quiescence search — only explores captures at depth 0
     * to avoid the horizon effect.
     */
    int quiescence(int alpha, int beta, int ply);

    // Check if we've exceeded the time limit
    bool shouldStop() const;

    // ========================================================================
    // Move Ordering
    // ========================================================================

    /**
     * @brief Score a move for ordering purposes.
     * Higher score = searched first.
     */
    int scoreMove(const Move& move, const Move& ttMove, int ply) const;

    /**
     * @brief Sort moves in-place by score (descending).
     */
    void orderMoves(Movelist& moves, const Move& ttMove, int ply) const;

    // ========================================================================
    // Evaluation
    // ========================================================================

    /**
     * @brief Static evaluation of the current board position.
     * Returns score from the perspective of the side to move.
     * Positive = good for side to move.
     */
    int evaluate() const;

    /**
     * @brief Calculate game phase (0 = endgame, 256 = opening).
     * Used to interpolate between middlegame and endgame piece-square tables.
     */
    int gamePhase() const;

    /**
     * @brief Endgame mop-up: push enemy king to edge and bring our king close.
     */
    int mopUpEval(Color strong, Color weak) const;

    // ========================================================================
    // Piece-Square Tables (PSTs)
    // ========================================================================

    // Material values in centipawns
    static constexpr int PIECE_VALUES[7] = {
        100,  // PAWN
        320,  // KNIGHT
        330,  // BISHOP
        500,  // ROOK
        900,  // QUEEN
        20000, // KING
        0     // NONE
    };

    // Phase weights for game phase calculation
    static constexpr int PHASE_WEIGHTS[6] = {
        0,  // PAWN
        1,  // KNIGHT
        1,  // BISHOP
        2,  // ROOK
        4,  // QUEEN
        0   // KING
    };
    static constexpr int TOTAL_PHASE = 4*1 + 4*1 + 4*2 + 2*4; // = 24

    // Middlegame PSTs (from white's perspective, a1=index 0)
    static const int PST_MG[6][64];

    // Endgame PSTs
    static const int PST_EG[6][64];

    // ========================================================================
    // Transposition Table helpers
    // ========================================================================
    void ttStore(uint64_t key, int depth, int score, TTFlag flag, Move best, int ply);
    bool ttProbe(uint64_t key, int depth, int alpha, int beta, int& score, Move& best, int ply) const;
};

#endif // ENGINE_H
