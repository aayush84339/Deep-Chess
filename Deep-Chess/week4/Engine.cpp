#include "Engine.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

// ============================================================================
// Piece-Square Tables (PSTs)
// ============================================================================
//
// From white's perspective. Index 0 = a1, index 63 = h8.
// For black, we flip the square vertically (sq ^ 56).
//
// Values from the Simplified Evaluation Function (Chess Programming Wiki).
// ============================================================================

// Middlegame PSTs
const int Engine::PST_MG[6][64] = {
    // PAWN (index 0)
    {
         0,   0,   0,   0,   0,   0,   0,   0,
         5,  10,  10, -20, -20,  10,  10,   5,
         5,  -5, -10,   0,   0, -10,  -5,   5,
         0,   0,   0,  20,  20,   0,   0,   0,
         5,   5,  10,  25,  25,  10,   5,   5,
        10,  10,  20,  30,  30,  20,  10,  10,
        50,  50,  50,  50,  50,  50,  50,  50,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // KNIGHT (index 1)
    {
       -50, -40, -30, -30, -30, -30, -40, -50,
       -40, -20,   0,   0,   0,   0, -20, -40,
       -30,   0,  10,  15,  15,  10,   0, -30,
       -30,   5,  15,  20,  20,  15,   5, -30,
       -30,   0,  15,  20,  20,  15,   0, -30,
       -30,   5,  10,  15,  15,  10,   5, -30,
       -40, -20,   0,   5,   5,   0, -20, -40,
       -50, -40, -30, -30, -30, -30, -40, -50
    },
    // BISHOP (index 2)
    {
       -20, -10, -10, -10, -10, -10, -10, -20,
       -10,   5,   0,   0,   0,   0,   5, -10,
       -10,  10,  10,  10,  10,  10,  10, -10,
       -10,   0,  10,  10,  10,  10,   0, -10,
       -10,   5,   5,  10,  10,   5,   5, -10,
       -10,   0,   5,  10,  10,   5,   0, -10,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -20, -10, -10, -10, -10, -10, -10, -20
    },
    // ROOK (index 3)
    {
         0,   0,   0,   5,   5,   0,   0,   0,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
         5,  10,  10,  10,  10,  10,  10,   5,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // QUEEN (index 4)
    {
       -20, -10, -10,  -5,  -5, -10, -10, -20,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -10,   0,   5,   5,   5,   5,   0, -10,
        -5,   0,   5,   5,   5,   5,   0,  -5,
         0,   0,   5,   5,   5,   5,   0,  -5,
       -10,   5,   5,   5,   5,   5,   0, -10,
       -10,   0,   5,   0,   0,   0,   0, -10,
       -20, -10, -10,  -5,  -5, -10, -10, -20
    },
    // KING middlegame (index 5) — stay safe, castle early
    {
        20,  30,  10,   0,   0,  10,  30,  20,
        20,  20,   0,   0,   0,   0,  20,  20,
       -10, -20, -20, -20, -20, -20, -20, -10,
       -20, -30, -30, -40, -40, -30, -30, -20,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30
    }
};

// Endgame PSTs
const int Engine::PST_EG[6][64] = {
    // PAWN endgame — push pawns forward!
    {
         0,   0,   0,   0,   0,   0,   0,   0,
        10,  10,  10,  10,  10,  10,  10,  10,
        20,  20,  20,  20,  20,  20,  20,  20,
        30,  30,  30,  30,  30,  30,  30,  30,
        50,  50,  50,  50,  50,  50,  50,  50,
        80,  80,  80,  80,  80,  80,  80,  80,
        120, 120, 120, 120, 120, 120, 120, 120,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // KNIGHT endgame (same as middlegame)
    {
       -50, -40, -30, -30, -30, -30, -40, -50,
       -40, -20,   0,   0,   0,   0, -20, -40,
       -30,   0,  10,  15,  15,  10,   0, -30,
       -30,   5,  15,  20,  20,  15,   5, -30,
       -30,   0,  15,  20,  20,  15,   0, -30,
       -30,   5,  10,  15,  15,  10,   5, -30,
       -40, -20,   0,   5,   5,   0, -20, -40,
       -50, -40, -30, -30, -30, -30, -40, -50
    },
    // BISHOP endgame (same as middlegame)
    {
       -20, -10, -10, -10, -10, -10, -10, -20,
       -10,   5,   0,   0,   0,   0,   5, -10,
       -10,  10,  10,  10,  10,  10,  10, -10,
       -10,   0,  10,  10,  10,  10,   0, -10,
       -10,   5,   5,  10,  10,   5,   5, -10,
       -10,   0,   5,  10,  10,   5,   0, -10,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -20, -10, -10, -10, -10, -10, -10, -20
    },
    // ROOK endgame (same as middlegame)
    {
         0,   0,   0,   5,   5,   0,   0,   0,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
         5,  10,  10,  10,  10,  10,  10,   5,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // QUEEN endgame (same as middlegame)
    {
       -20, -10, -10,  -5,  -5, -10, -10, -20,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -10,   0,   5,   5,   5,   5,   0, -10,
        -5,   0,   5,   5,   5,   5,   0,  -5,
         0,   0,   5,   5,   5,   5,   0,  -5,
       -10,   5,   5,   5,   5,   5,   0, -10,
       -10,   0,   5,   0,   0,   0,   0, -10,
       -20, -10, -10,  -5,  -5, -10, -10, -20
    },
    // KING endgame — go to center!
    {
       -50, -30, -30, -30, -30, -30, -30, -50,
       -30, -30,   0,   0,   0,   0, -30, -30,
       -30, -10,  20,  30,  30,  20, -10, -30,
       -30, -10,  30,  40,  40,  30, -10, -30,
       -30, -10,  30,  40,  40,  30, -10, -30,
       -30, -10,  20,  30,  30,  20, -10, -30,
       -30, -20, -10,   0,   0, -10, -20, -30,
       -50, -40, -30, -20, -20, -30, -40, -50
    }
};

// ============================================================================
// Constructor
// ============================================================================
Engine::Engine()
    : nodesSearched_(0), lastScore_(0), lastDepth_(0),
      timeLimitMs_(0), timeUp_(false) {
    tt_.resize(TT_SIZE);
    newGame();
}

// ============================================================================
// Position Setup
// ============================================================================
void Engine::setPosition(const std::string& fen) {
    board_ = Board(fen);
}

void Engine::setPosition(const std::string& fen, const std::vector<std::string>& moves) {
    board_ = Board(fen);
    for (const auto& moveStr : moves) {
        Move m = uci::uciToMove(board_, moveStr);
        board_.makeMove(m);
    }
}

void Engine::newGame() {
    std::fill(tt_.begin(), tt_.end(), TTEntry{});
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
}

const Board& Engine::getBoard() const {
    return board_;
}

long long Engine::getNodesSearched() const {
    return nodesSearched_;
}

int Engine::getLastScore() const {
    return lastScore_;
}

int Engine::getLastDepth() const {
    return lastDepth_;
}

// ============================================================================
// Transposition Table
// ============================================================================
void Engine::ttStore(uint64_t key, int depth, int score, TTFlag flag, Move best, int ply) {
    int index = static_cast<int>(key % TT_SIZE);
    TTEntry& entry = tt_[index];

    // Adjust mate scores for storage (make them relative to root, not current ply)
    int storedScore = score;
    if (storedScore > MATE_SCORE - 500) storedScore += ply;
    if (storedScore < -MATE_SCORE + 500) storedScore -= ply;

    // Always replace (simple replacement scheme)
    // Only replace if new depth >= stored depth, or different position
    if (entry.key != key || depth >= entry.depth) {
        entry.key   = key;
        entry.depth = depth;
        entry.score = storedScore;
        entry.flag  = flag;
        entry.best  = best;
    }
}

bool Engine::ttProbe(uint64_t key, int depth, int alpha, int beta, int& score, Move& best, int ply) const {
    int index = static_cast<int>(key % TT_SIZE);
    const TTEntry& entry = tt_[index];

    if (entry.key != key) return false;

    best = entry.best;

    if (entry.depth >= depth) {
        score = entry.score;

        // Adjust mate scores back to current ply
        if (score > MATE_SCORE - 500) score -= ply;
        if (score < -MATE_SCORE + 500) score += ply;

        if (entry.flag == TT_EXACT) return true;
        if (entry.flag == TT_ALPHA && score <= alpha) { score = alpha; return true; }
        if (entry.flag == TT_BETA  && score >= beta)  { score = beta;  return true; }
    }

    return false;
}

// ============================================================================
// Game Phase
// ============================================================================
int Engine::gamePhase() const {
    int phase = 0;
    for (int pt = 0; pt < 5; ++pt) { // PAWN..QUEEN
        Bitboard bb = board_.pieces(PieceType(static_cast<PieceType::underlying>(pt)));
        phase += bb.count() * PHASE_WEIGHTS[pt];
    }
    // Clamp to TOTAL_PHASE
    if (phase > TOTAL_PHASE) phase = TOTAL_PHASE;
    return phase;
}

// ============================================================================
// Evaluation
// ============================================================================
int Engine::evaluate() const {
    int mgScore = 0;
    int egScore = 0;

    // For each piece type
    for (int pt = 0; pt < 6; ++pt) {
        PieceType pieceType(static_cast<PieceType::underlying>(pt));

        // White pieces
        Bitboard whitePieces = board_.pieces(pieceType, Color::WHITE);
        while (whitePieces) {
            int sq = whitePieces.pop();
            mgScore += PIECE_VALUES[pt] + PST_MG[pt][sq];
            egScore += PIECE_VALUES[pt] + PST_EG[pt][sq];
        }

        // Black pieces (flip square for PST lookup)
        Bitboard blackPieces = board_.pieces(pieceType, Color::BLACK);
        while (blackPieces) {
            int sq = blackPieces.pop();
            int flipped = sq ^ 56; // Flip vertically for black
            mgScore -= PIECE_VALUES[pt] + PST_MG[pt][flipped];
            egScore -= PIECE_VALUES[pt] + PST_EG[pt][flipped];
        }
    }

    // Bishop pair bonus
    if (board_.pieces(PieceType::BISHOP, Color::WHITE).count() >= 2)
        mgScore += 30;
    if (board_.pieces(PieceType::BISHOP, Color::BLACK).count() >= 2)
        mgScore -= 30;

    // Interpolate between middlegame and endgame
    int phase = gamePhase();
    int score = (mgScore * phase + egScore * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    // Endgame mop-up evaluation (when one side has large material advantage)
    int whiteMaterial = 0, blackMaterial = 0;
    for (int pt = 0; pt < 5; ++pt) {
        PieceType pieceType(static_cast<PieceType::underlying>(pt));
        whiteMaterial += board_.pieces(pieceType, Color::WHITE).count() * PIECE_VALUES[pt];
        blackMaterial += board_.pieces(pieceType, Color::BLACK).count() * PIECE_VALUES[pt];
    }

    // If one side is up by a rook+ worth of material, add mop-up score
    if (whiteMaterial - blackMaterial > 400) {
        score += mopUpEval(Color::WHITE, Color::BLACK);
    } else if (blackMaterial - whiteMaterial > 400) {
        score -= mopUpEval(Color::BLACK, Color::WHITE);
    }

    // Return from the perspective of the side to move
    return (board_.sideToMove() == Color::WHITE) ? score : -score;
}

// ============================================================================
// Mop-up Evaluation
// ============================================================================
int Engine::mopUpEval(Color strong, Color weak) const {
    int eval = 0;

    Square weakKingSq = board_.kingSq(weak);
    Square strongKingSq = board_.kingSq(strong);

    // Push enemy king to edge
    int weakKingFile = weakKingSq.file();
    int weakKingRank = weakKingSq.rank();
    int fileDist = std::max(3 - weakKingFile, weakKingFile - 4);
    int rankDist = std::max(3 - weakKingRank, weakKingRank - 4);
    int edgeDistance = fileDist + rankDist;
    eval += edgeDistance * 10;

    // Bring strong king closer to weak king
    int kingDistance = Square::distance(strongKingSq, weakKingSq);
    eval += (7 - kingDistance) * 5;

    return eval;
}

// ============================================================================
// Move Ordering
// ============================================================================
int Engine::scoreMove(const Move& move, const Move& ttMove, int ply) const {
    // TT move gets highest priority
    if (move == ttMove) return 1000000;

    int score = 0;

    // Captures: MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
    if (board_.isCapture(move)) {
        PieceType victim = board_.at<PieceType>(move.to());
        PieceType attacker = board_.at<PieceType>(move.from());

        if (move.typeOf() == Move::ENPASSANT) {
            victim = PieceType::PAWN;
        }

        // victim value * 10 - attacker value (to maximize victim, minimize attacker)
        int victimVal = (victim != PieceType::NONE) ? PIECE_VALUES[static_cast<int>(victim)] : 100;
        int attackerVal = (attacker != PieceType::NONE) ? PIECE_VALUES[static_cast<int>(attacker)] : 100;
        score = 100000 + victimVal * 10 - attackerVal;
        return score;
    }

    // Promotions
    if (move.typeOf() == Move::PROMOTION) {
        score = 90000 + PIECE_VALUES[static_cast<int>(move.promotionType())];
        return score;
    }

    // Killer moves
    if (ply < MAX_PLY) {
        if (move == killers_[ply][0]) return 80000;
        if (move == killers_[ply][1]) return 79000;
    }

    // History heuristic
    int color = static_cast<int>(board_.sideToMove());
    score = history_[color][move.from().index()][move.to().index()];

    return score;
}

void Engine::orderMoves(Movelist& moves, const Move& ttMove, int ply) const {
    // Simple selection sort (good enough for typical move counts of ~30)
    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
        int bestIdx = i;
        int bestScore = scoreMove(moves[i], ttMove, ply);
        for (int j = i + 1; j < static_cast<int>(moves.size()); ++j) {
            int s = scoreMove(moves[j], ttMove, ply);
            if (s > bestScore) {
                bestScore = s;
                bestIdx = j;
            }
        }
        if (bestIdx != i) {
            std::swap(moves[i], moves[bestIdx]);
        }
    }
}

// ============================================================================
// Time Check
// ============================================================================
bool Engine::shouldStop() const {
    if (timeLimitMs_ <= 0) return false;
    if (nodesSearched_ % 2048 != 0) return false; // Check every 2048 nodes

    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - searchStart_).count();
    return elapsed >= timeLimitMs_;
}

// ============================================================================
// Quiescence Search
// ============================================================================
int Engine::quiescence(int alpha, int beta, int ply) {
    nodesSearched_++;

    // Stand pat: assume we can at least achieve the static eval
    int standPat = evaluate();
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    // Generate only captures
    Movelist captures;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(captures, board_);

    // Order captures by MVV-LVA
    orderMoves(captures, Move::NO_MOVE, ply);

    for (int i = 0; i < static_cast<int>(captures.size()); ++i) {
        if (timeUp_) return 0;

        board_.makeMove(captures[i]);
        int score = -quiescence(-beta, -alpha, ply + 1);
        board_.unmakeMove(captures[i]);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// ============================================================================
// Negamax with Alpha-Beta Pruning
// ============================================================================
int Engine::negamax(int depth, int alpha, int beta, int ply) {
    // Check time
    if (shouldStop()) {
        timeUp_ = true;
        return 0;
    }

    nodesSearched_++;

    // Draw detection
    if (ply > 0) {
        if (board_.isRepetition(1) || board_.isHalfMoveDraw()) {
            return DRAW_SCORE;
        }
        if (board_.isInsufficientMaterial()) {
            return DRAW_SCORE;
        }
    }

    // Quiescence search at depth 0
    if (depth <= 0) {
        return quiescence(alpha, beta, ply);
    }

    // Probe transposition table
    uint64_t key = board_.hash();
    Move ttMove = Move::NO_MOVE;
    int ttScore;
    if (ply > 0 && ttProbe(key, depth, alpha, beta, ttScore, ttMove, ply)) {
        return ttScore;
    }

    // Generate all legal moves
    Movelist moves;
    movegen::legalmoves(moves, board_);

    // Terminal: checkmate or stalemate
    if (moves.empty()) {
        if (board_.inCheck()) {
            // Checkmate — return negative mate score (we are mated)
            return -MATE_SCORE + ply;
        }
        // Stalemate
        return DRAW_SCORE;
    }

    // Check extension: search one ply deeper when in check
    if (board_.inCheck()) {
        depth++;
    }

    // Order moves
    orderMoves(moves, ttMove, ply);

    Move bestMove = moves[0];
    TTFlag ttFlag = TT_ALPHA;

    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
        if (timeUp_) return 0;

        board_.makeMove(moves[i]);
        int score = -negamax(depth - 1, -beta, -alpha, ply + 1);
        board_.unmakeMove(moves[i]);

        if (timeUp_) return 0;

        if (score > alpha) {
            alpha = score;
            bestMove = moves[i];
            ttFlag = TT_EXACT;

            if (score >= beta) {
                // Beta cutoff — store killer moves and history
                if (!board_.isCapture(moves[i])) {
                    // Update killer moves
                    if (ply < MAX_PLY) {
                        killers_[ply][1] = killers_[ply][0];
                        killers_[ply][0] = moves[i];
                    }
                    // Update history heuristic
                    int color = static_cast<int>(board_.sideToMove());
                    history_[color][moves[i].from().index()][moves[i].to().index()] += depth * depth;
                }

                // Store in TT
                ttStore(key, depth, beta, TT_BETA, bestMove, ply);
                return beta;
            }
        }
    }

    // Store result in TT
    ttStore(key, depth, alpha, ttFlag, bestMove, ply);

    return alpha;
}

// ============================================================================
// Iterative Deepening Search
// ============================================================================
Move Engine::search(int maxDepth, int timeLimitMs) {
    nodesSearched_ = 0;
    lastScore_ = 0;
    lastDepth_ = 0;
    timeUp_ = false;

    if (maxDepth <= 0) maxDepth = 64;
    timeLimitMs_ = timeLimitMs;
    searchStart_ = std::chrono::high_resolution_clock::now();

    // Clear killer moves for new search
    std::memset(killers_, 0, sizeof(killers_));

    // Reduce history values (age them, don't clear completely)
    for (int c = 0; c < 2; c++)
        for (int f = 0; f < 64; f++)
            for (int t = 0; t < 64; t++)
                history_[c][f][t] /= 2;

    Move bestMove = Move::NO_MOVE;

    // Generate legal moves at root — if only one move, return immediately
    Movelist rootMoves;
    movegen::legalmoves(rootMoves, board_);
    if (rootMoves.empty()) return Move::NO_MOVE;
    if (rootMoves.size() == 1) return rootMoves[0];

    bestMove = rootMoves[0];

    // Iterative deepening loop
    for (int depth = 1; depth <= maxDepth; ++depth) {
        int score = negamax(depth, -INF_SCORE, INF_SCORE, 0);

        if (timeUp_) break;

        // Extract best move from TT
        uint64_t key = board_.hash();
        int idx = static_cast<int>(key % TT_SIZE);
        if (tt_[idx].key == key && tt_[idx].best != Move::NO_MOVE) {
            bestMove = tt_[idx].best;
        }

        lastScore_ = score;
        lastDepth_ = depth;

        // Build PV string
        std::string pvStr;
        Board tempBoard = board_;
        for (int d = 0; d < depth; ++d) {
            uint64_t h = tempBoard.hash();
            int ti = static_cast<int>(h % TT_SIZE);
            if (tt_[ti].key == h && tt_[ti].best != Move::NO_MOVE) {
                if (!pvStr.empty()) pvStr += " ";
                pvStr += uci::moveToUci(tt_[ti].best);
                tempBoard.makeMove(tt_[ti].best);
            } else {
                break;
            }
        }

        // Report info
        auto now = std::chrono::high_resolution_clock::now();
        int elapsedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - searchStart_).count());

        if (onSearchInfo) {
            onSearchInfo(depth, score, nodesSearched_, elapsedMs, pvStr);
        }

        // If we found a forced mate, stop searching
        if (score > MATE_SCORE - 500 || score < -MATE_SCORE + 500) {
            break;
        }

        // If more than half the time has been used, don't start a new depth
        if (timeLimitMs_ > 0 && elapsedMs > timeLimitMs_ / 2) {
            break;
        }
    }

    return bestMove;
}
