#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <array>
#include <cstdint>
#include <chess.hpp>

// Mirrors test.py / NN.py exactly:
//   - features are 'us' (side to move) vs 'them', not white/black
//   - squares rank-mirrored (sq ^ 56) when Black is to move
//   - castling ordered [us-KS, us-QS, them-KS, them-QS]
//   - ep file unaffected by mirroring (only rank flips)
//   - column 768 is the same PAD_IDX dump used during training
//   - network output is ALREADY side-to-move-relative cp
//
// Incremental accumulator design: rather than one stm-relative feature
// vector (which flips identity every ply and can't be updated cheaply),
// we maintain TWO fixed-perspective accumulators that never flip:
//   white_acc = W1_pieces @ features(White-is-us, no mirror)
//   black_acc = W1_pieces @ features(Black-is-us, mirrored)
// Both are updated by every move regardless of whose turn it is, since
// a move only ever changes 2-4 squares in EITHER fixed view. At eval
// time we just pick whichever accumulator matches the side to move --
// this is mathematically identical to a full recompute, just far
// cheaper, and needs no retraining since W1 is unchanged.

namespace nnue {

using namespace chess;

constexpr int IN_DIM = 781;
constexpr int H1 = 512;
constexpr int H2 = 256;
constexpr int H3 = 128;

// Written by export_weights.py at the start of nnue_weights.bin as
// (magic, IN_DIM, H1, H2, H3), each a little-endian 4-byte int. Lets
// load_weights() refuse a file whose shape doesn't match this build
// instead of silently reading a stale/mismatched binary as if it were
// valid (which previously would only fail once the file ran out of
// bytes, or worse, not fail at all and just produce garbage evals).
constexpr uint32_t WEIGHTS_MAGIC = 0x4E4E5545u;  // "NNUE" as bytes 'E','U','N','N' little-endian

struct Weights {
    std::vector<float> w1, b1;  // H1 x IN_DIM, H1
    std::vector<float> w2, b2;  // H2 x H1,     H2
    std::vector<float> w3, b3;  // H3 x H2,     H3
    std::vector<float> w4, b4;  // 1  x H3,     1
    std::vector<float> w1t_pieces;  // 768 x H1, transposed piece-slice of w1, for fast column access
    bool loaded = false;
};

inline Weights g_weights;

inline bool load_weights(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0;
    int32_t file_in_dim = 0, file_h1 = 0, file_h2 = 0, file_h3 = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    f.read(reinterpret_cast<char*>(&file_in_dim), sizeof(file_in_dim));
    f.read(reinterpret_cast<char*>(&file_h1), sizeof(file_h1));
    f.read(reinterpret_cast<char*>(&file_h2), sizeof(file_h2));
    f.read(reinterpret_cast<char*>(&file_h3), sizeof(file_h3));

    if (!f || magic != WEIGHTS_MAGIC || file_in_dim != IN_DIM ||
        file_h1 != H1 || file_h2 != H2 || file_h3 != H3) {
        return false;  // wrong/stale file -- caller falls back to classical_evaluate()
    }

    g_weights.w1.resize(static_cast<size_t>(H1) * IN_DIM);
    g_weights.b1.resize(H1);
    g_weights.w2.resize(static_cast<size_t>(H2) * H1);
    g_weights.b2.resize(H2);
    g_weights.w3.resize(static_cast<size_t>(H3) * H2);
    g_weights.b3.resize(H3);
    g_weights.w4.resize(static_cast<size_t>(1) * H3);
    g_weights.b4.resize(1);

    auto read = [&](std::vector<float>& v) {
        f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float)));
    };

    read(g_weights.w1); read(g_weights.b1);
    read(g_weights.w2); read(g_weights.b2);
    read(g_weights.w3); read(g_weights.b3);
    read(g_weights.w4); read(g_weights.b4);

    g_weights.loaded = static_cast<bool>(f);

    if (g_weights.loaded) {
        g_weights.w1t_pieces.resize(768 * static_cast<size_t>(H1));
        for (int feat = 0; feat < 768; feat++) {
            for (int o = 0; o < H1; o++) {
                g_weights.w1t_pieces[static_cast<size_t>(feat) * H1 + o] = g_weights.w1[static_cast<size_t>(o) * IN_DIM + feat];
            }
        }
    }

    return g_weights.loaded;
}

struct Accumulator {
    std::array<float, H1> v{};
};

struct NNUEState {
    Accumulator white_acc;  // fixed "White is us" view, never mirrors
    Accumulator black_acc;  // fixed "Black is us" view, always mirrors
    std::vector<std::pair<Accumulator, Accumulator>> stack;  // snapshots for undo
};

inline NNUEState g_state;

inline void add_feature(Accumulator& acc, int feat) {
    const float* col = &g_weights.w1t_pieces[static_cast<size_t>(feat) * H1];
    for (int o = 0; o < H1; o++) acc.v[o] += col[o];
}

inline void remove_feature(Accumulator& acc, int feat) {
    const float* col = &g_weights.w1t_pieces[static_cast<size_t>(feat) * H1];
    for (int o = 0; o < H1; o++) acc.v[o] -= col[o];
}

// Feature index for a given fixed perspective ("us_is_white" = true means
// this is the white_acc view; false means the black_acc view).
inline int feature_index(Piece p, Square sq, bool us_is_white) {
    Color us = us_is_white ? Color::WHITE : Color::BLACK;
    int color_key = (p.color() == us) ? 0 : 1;
    int type_key = static_cast<int>(p.type());
    int mapped_sq = us_is_white ? sq.index() : (sq.index() ^ 56);
    return color_key * 6 * 64 + type_key * 64 + mapped_sq;
}

inline void add_piece(NNUEState& state, Piece p, Square sq) {
    add_feature(state.white_acc, feature_index(p, sq, true));
    add_feature(state.black_acc, feature_index(p, sq, false));
}

inline void remove_piece(NNUEState& state, Piece p, Square sq) {
    remove_feature(state.white_acc, feature_index(p, sq, true));
    remove_feature(state.black_acc, feature_index(p, sq, false));
}

// Full rebuild from scratch -- call this once when a brand new position is
// set (e.g. the "position" UCI command), not during search.
inline void full_refresh(Board& board, NNUEState& state) {
    if (!g_weights.loaded) return; // no transposed weight matrix to accumulate into

    state.white_acc = Accumulator{};
    state.black_acc = Accumulator{};
    state.stack.clear();

    for (int sq = 0; sq < 64; sq++) {
        Piece p = board.at(Square(sq));
        if (p == Piece::NONE) continue;
        add_piece(state, p, Square(sq));
    }
}

// Applies the incremental delta for `move`, which is about to be played on
// `board` (call this BEFORE board.makeMove(move), since it needs to read
// piece placement in the pre-move state). Pushes a snapshot so undo_move()
// can cheaply restore afterward.
inline void apply_move_delta(Board& board, const Move& move, NNUEState& state) {
    if (!g_weights.loaded) return; // classical_evaluate() fallback path -- no accumulator to maintain

    state.stack.emplace_back(state.white_acc, state.black_acc);

    Color stm = board.sideToMove();

    if (move.typeOf() == Move::CASTLING) {
        Piece king = board.at(move.from());
        Piece rook = board.at(move.to());
        bool king_side = move.to() > move.from();
        Square kingTo = Square::castling_king_square(king_side, stm);
        Square rookTo = Square::castling_rook_square(king_side, stm);

        remove_piece(state, king, move.from());
        remove_piece(state, rook, move.to());
        add_piece(state, king, kingTo);
        add_piece(state, rook, rookTo);

    } else if (move.typeOf() == Move::PROMOTION) {
        Piece pawn = board.at(move.from());
        Piece captured = board.at(move.to());
        Piece promoted = Piece(move.promotionType(), stm);

        remove_piece(state, pawn, move.from());
        if (captured != Piece::NONE) remove_piece(state, captured, move.to());
        add_piece(state, promoted, move.to());

    } else if (move.typeOf() == Move::ENPASSANT) {
        Piece pawn = board.at(move.from());
        Square capturedSq = move.to().ep_square();
        Piece capturedPawn = board.at(capturedSq);

        remove_piece(state, pawn, move.from());
        remove_piece(state, capturedPawn, capturedSq);
        add_piece(state, pawn, move.to());

    } else {
        Piece piece = board.at(move.from());
        Piece captured = board.at(move.to());

        remove_piece(state, piece, move.from());
        if (captured != Piece::NONE) remove_piece(state, captured, move.to());
        add_piece(state, piece, move.to());
    }
}

// Call this after board.unmakeMove(move) to restore the accumulators to
// their pre-move state (cheap snapshot restore, no reverse-delta logic
// needed -- much less error-prone than re-deriving undo math per move type).
inline void undo_move(NNUEState& state) {
    if (!g_weights.loaded) return; // matches apply_move_delta's no-op -- nothing was pushed

    state.white_acc = state.stack.back().first;
    state.black_acc = state.stack.back().second;
    state.stack.pop_back();
}

inline void linear_relu(const float* in, int in_dim, const float* w, const float* b, float* out, int out_dim, bool relu) {
    for (int o = 0; o < out_dim; o++) {
        float sum = b[o];
        const float* wrow = w + static_cast<size_t>(o) * in_dim;
        for (int i = 0; i < in_dim; i++) {
            sum += wrow[i] * in[i];
        }
        out[o] = relu ? std::max(0.0f, sum) : sum;
    }
}

// Adds the contribution of the 13 non-piece features (pad, castle, ep) --
// these are cheap enough to just recompute fresh every call, no need to
// accumulate them incrementally.
inline void add_extra_contribution(Board& board, std::array<float, H1>& h1_pre) {
    Color us = board.sideToMove();
    Color them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;

    int num_pieces = board.occ().count();

    auto add_col = [&](int feat) {
        for (int o = 0; o < H1; o++) h1_pre[o] += g_weights.w1[static_cast<size_t>(o) * IN_DIM + feat];
    };

    if (num_pieces < 32) add_col(768);

    bool us_ks = board.castlingRights().has(us, Board::CastlingRights::Side::KING_SIDE);
    bool us_qs = board.castlingRights().has(us, Board::CastlingRights::Side::QUEEN_SIDE);
    bool them_ks = board.castlingRights().has(them, Board::CastlingRights::Side::KING_SIDE);
    bool them_qs = board.castlingRights().has(them, Board::CastlingRights::Side::QUEEN_SIDE);

    if (us_ks) add_col(769);
    if (us_qs) add_col(770);
    if (them_ks) add_col(771);
    if (them_qs) add_col(772);

    Square ep = board.enpassantSq();
    if (ep != Square::NO_SQ) {
        add_col(773 + ep.file());
    }
}

// Fast path: uses the maintained accumulators. Requires state to already be
// in sync with `board` (i.e. full_refresh() was called at the start of this
// position, and every subsequent makeMove/unmakeMove was paired with
// apply_move_delta/undo_move).
inline int nnue_evaluate_incremental(Board& board, NNUEState& state) {
    std::array<float, H1> h1_pre = (board.sideToMove() == Color::WHITE) ? state.white_acc.v : state.black_acc.v;

    add_extra_contribution(board, h1_pre);

    std::array<float, H1> h1;
    for (int o = 0; o < H1; o++) h1[o] = std::max(0.0f, h1_pre[o] + g_weights.b1[o]);

    static thread_local float h2[H2];
    static thread_local float h3[H3];
    float out[1];

    linear_relu(h1.data(), H1, g_weights.w2.data(), g_weights.b2.data(), h2, H2, true);
    linear_relu(h2, H2, g_weights.w3.data(), g_weights.b3.data(), h3, H3, true);
    linear_relu(h3, H3, g_weights.w4.data(), g_weights.b4.data(), out, 1, false);

    return static_cast<int>(std::lround(out[0]));
}

// Slow path kept for reference / correctness-checking against the
// incremental path, and as an emergency fallback that needs no accumulator
// state at all.
inline void build_features(Board& board, float* x) {
    std::fill(x, x + IN_DIM, 0.0f);

    Color us = board.sideToMove();
    Color them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
    int num_pieces = 0;

    for (int sq = 0; sq < 64; sq++) {
        Piece p = board.at(Square(sq));
        if (p == Piece::NONE) continue;
        num_pieces++;

        int color_key = (p.color() == us) ? 0 : 1;
        int type_key = static_cast<int>(p.type());
        int mapped_sq = (us == Color::WHITE) ? sq : (sq ^ 56);

        x[color_key * 6 * 64 + type_key * 64 + mapped_sq] = 1.0f;
    }

    if (num_pieces < 32) x[768] = 1.0f;

    x[769] = board.castlingRights().has(us, Board::CastlingRights::Side::KING_SIDE) ? 1.0f : 0.0f;
    x[770] = board.castlingRights().has(us, Board::CastlingRights::Side::QUEEN_SIDE) ? 1.0f : 0.0f;
    x[771] = board.castlingRights().has(them, Board::CastlingRights::Side::KING_SIDE) ? 1.0f : 0.0f;
    x[772] = board.castlingRights().has(them, Board::CastlingRights::Side::QUEEN_SIDE) ? 1.0f : 0.0f;

    Square ep = board.enpassantSq();
    if (ep != Square::NO_SQ) x[773 + ep.file()] = 1.0f;
}

inline int nnue_evaluate_full(Board& board) {
    static thread_local float x[IN_DIM];
    static thread_local float h1[H1];
    static thread_local float h2[H2];
    static thread_local float h3[H3];
    float out[1];

    build_features(board, x);

    linear_relu(x, IN_DIM, g_weights.w1.data(), g_weights.b1.data(), h1, H1, true);
    linear_relu(h1, H1, g_weights.w2.data(), g_weights.b2.data(), h2, H2, true);
    linear_relu(h2, H2, g_weights.w3.data(), g_weights.b3.data(), h3, H3, true);
    linear_relu(h3, H3, g_weights.w4.data(), g_weights.b4.data(), out, 1, false);

    return static_cast<int>(std::lround(out[0]));
}

// evaluate() as called by search: uses the fast incremental path.
inline int nnue_evaluate(Board& board) {
    return nnue_evaluate_incremental(board, g_state);
}

}  // namespace nnue
