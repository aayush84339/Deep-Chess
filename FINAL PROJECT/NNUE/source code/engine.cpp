/*
g++ -std=c++17 -O3 -march=native -DFORCE_CLASSICAL -I. engine.cpp -o classical
This is for the pure classical capabilities, nnue disabled. Keep chess.hpp, engine.cpp and 
nnue.hpp together in one directory.

For this classical bot:
RMSE: 228.845
MAE:  153.855

g++ -std=c++17 -O3 -march=native -I. engine.cpp -o nnue
This is for nnue enabled

For Nnue:
RMSE: 137.243
*/


#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tuple>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <filesystem>
#include <chess.hpp>
#include "nnue.hpp"
using namespace chess;
using namespace std;

#define loop(i,n) for(int i=0;i<n;i++)
#define all(v) v.begin(),v.end()
#define vMove vector<Move>

const int MATE_SCORE=1000000;
const int MATE_BOUND=MATE_SCORE-1000;   // MATE IN X walo ka isse jyada hoga
const int INF=1000000000;

const int MAX_PLY=64; //iterative deepening safe limit
const int PLAY_DEPTH=12; //fallback depth when no clock is given
const int ENDGAME_THRESHOLD = 2500; //itne centipawns have to come off the board before allowing king to do masti

const int EXACT=0;
const int LOWER=1;
const int UPPER=2;

const string START_FEN="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const string NNUE_WEIGHTS_FILE="nnue_weights.bin";

const int PIECE_VAL[6] = {100, 320, 330, 500, 900, 0}; // P N B R Q K

//stolen tables from ai
const int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
const int PST_KNIGHT[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};
const int PST_BISHOP[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};
const int PST_ROOK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};
const int PST_QUEEN[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};
const int PST_KING_MG[64] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};
const int PST_KING_EG[64] = {
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50
};
//

const int* PST[6] = {PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING_MG};//Fancy way to index the tables

// Flat, fixed-size TT slot. `key` stores the full zobrist hash so we can
// detect collisions (two different positions mapping to the same slot).
// No heap allocation, no growth, no pointer-chasing -- much more
// cache-friendly than a hash map.
struct TTEntry
{
    uint64_t key=0;
    int score=0;
    int depth=-1;
    int flag=0;
    Move best;
    int gen=-1;   // which search (real move) this entry was computed in -- see new_search()
};

const size_t TT_SIZE = size_t(1)<<22; // ~4M slots, fixed

// ---- extra positional feature weights (reasonable hand-picked defaults,
// not machine-tuned -- see chat for what each term does and why). All of
// this only runs on the classical fallback path, so it's fine for it to
// cost more per call than the NNUE forward pass; it's not the hot path
// when nnue_weights.bin is actually loaded.
const int BISHOP_PAIR_BONUS     = 30;
const int ROOK_OPEN_FILE_BONUS  = 20;
const int ROOK_SEMI_OPEN_BONUS  = 10;
const int DOUBLED_PAWN_PENALTY  = 10;
const int ISOLATED_PAWN_PENALTY = 15;
const int KING_SHIELD_PENALTY   = 10;
const int PASSED_PAWN_BONUS[8]  = {0, 5, 10, 20, 35, 60, 100, 150}; // indexed by ranks advanced from own 2nd rank
const int MOBILITY_WEIGHT[6]    = {0, 4, 4, 2, 1, 0};               // P N B R Q K,cp per reachable square
const int PHASE_WEIGHT[6]       = {0, 1, 1, 2, 4, 0};               // P N B R Q K
const int MAX_PHASE = 24; // 2 * (2*1 knight + 2*1 bishop + 2*2 rook + 1*4 queen), i.e. both sides at full material

// Doubled/isolated/passed pawns for one side. At most 8 pawns, all
// bitboard ops -- cheap even run twice (once per side) per eval call.
int pawn_structure_score(Board &board, Color us)
{
    Bitboard ourPawns   = board.pieces(PieceType::PAWN, us);
    Bitboard theirPawns = board.pieces(PieceType::PAWN, ~us);
    int score = 0;

    loop(f,8)
    {
        int count = (ourPawns & attacks::MASK_FILE[f]).count();
        if(count>1) score -= DOUBLED_PAWN_PENALTY*(count-1);
    }

    Bitboard bb = ourPawns;
    while(!bb.empty())
    {
        int sq = bb.lsb();
        bb.clear(sq);
        int file = sq & 7;
        int rank = sq >> 3;

        bool hasNeighbor = false;
        for(int df=-1; df<=1; df+=2)
        {
            int nf = file+df;
            if(nf<0 || nf>7) continue;
            if(!(ourPawns & attacks::MASK_FILE[nf]).empty()) hasNeighbor=true;
        }
        if(!hasNeighbor) score -= ISOLATED_PAWN_PENALTY;

        bool passed = true;
        for(int df=-1; df<=1 && passed; df++)
        {
            int nf = file+df;
            if(nf<0 || nf>7) continue;

            // "ahead" mask -- ranks beyond ours, toward our promotion square.
            Bitboard aheadMask;
            if(us==Color::WHITE)
            {
                int shift = (rank+1)*8;
                aheadMask = (shift>=64) ? Bitboard(0ULL) : Bitboard(~((1ULL<<shift)-1));
            }
            else
            {
                int shift = rank*8;
                aheadMask = (shift<=0) ? Bitboard(0ULL) : Bitboard((1ULL<<shift)-1);
            }

            if(!(theirPawns & attacks::MASK_FILE[nf] & aheadMask).empty()) passed=false;
        }
        if(passed)
        {
            int advance = (us==Color::WHITE) ? rank : (7-rank);
            score += PASSED_PAWN_BONUS[advance];
        }
    }

    return score;
}

// Rough mobility: popcount of pseudo-attacked squares not occupied by our
// own pieces, weighted per piece type.
int mobility_score(Board &board, Color us)
{
    Bitboard occ    = board.occ();
    Bitboard ownOcc = board.us(us);
    int score = 0;

    Bitboard knights = board.pieces(PieceType::KNIGHT, us);
    while(!knights.empty())
    {
        int sq = knights.lsb(); knights.clear(sq);
        score += MOBILITY_WEIGHT[1] * (attacks::knight(Square(sq)) & ~ownOcc).count();
    }
    Bitboard bishops = board.pieces(PieceType::BISHOP, us);
    while(!bishops.empty())
    {
        int sq = bishops.lsb(); bishops.clear(sq);
        score += MOBILITY_WEIGHT[2] * (attacks::bishop(Square(sq), occ) & ~ownOcc).count();
    }
    Bitboard rooks = board.pieces(PieceType::ROOK, us);
    while(!rooks.empty())
    {
        int sq = rooks.lsb(); rooks.clear(sq);
        score += MOBILITY_WEIGHT[3] * (attacks::rook(Square(sq), occ) & ~ownOcc).count();
    }
    Bitboard queens = board.pieces(PieceType::QUEEN, us);
    while(!queens.empty())
    {
        int sq = queens.lsb(); queens.clear(sq);
        score += MOBILITY_WEIGHT[4] * (attacks::queen(Square(sq), occ) & ~ownOcc).count();
    }

    return score;
}

// Pawn shield directly in front of the king. Only meaningful before the
// position opens up into an endgame -- the caller weights this by phase.
int king_shield_penalty(Board &board, Color us)
{
    int king = board.pieces(PieceType::KING, us).lsb();
    int kf = king & 7;
    int kr = king >> 3;
    int shieldRank = (us==Color::WHITE) ? kr+1 : kr-1;
    if(shieldRank<0 || shieldRank>7) return 0;

    Bitboard ourPawns = board.pieces(PieceType::PAWN, us);
    int missing = 0;
    for(int df=-1; df<=1; df++)
    {
        int f = kf+df;
        if(f<0 || f>7) continue;
        int sq = shieldRank*8+f;
        if((ourPawns & Bitboard(1ULL<<sq)).empty()) missing++;
    }
    return missing*KING_SHIELD_PENALTY;
}

int classical_evaluate(Board &board)
{
    // ---- game phase (0=full endgame .. MAX_PHASE=full midgame) --------
    // Used to smoothly blend the king's midgame/endgame PST by material
    // remaining, instead of flipping between them at a hard threshold
    int phase = 0;
    loop(t,6)
    {
        if(t==0 || t==5) continue; // pawns/king don't count toward phase
        PieceType pt(static_cast<PieceType::underlying>(t));
        Bitboard bb = board.pieces(pt, Color::WHITE) | board.pieces(pt, Color::BLACK);
        phase += bb.count()*PHASE_WEIGHT[t];
    }
    if(phase>MAX_PHASE) phase=MAX_PHASE; // extra promoted queens etc. can exceed the "normal" max

    int score=0;
    loop(t,6)
    {
        PieceType pt(static_cast<PieceType::underlying>(t));
        loop(c,2)
        {
            Color col = c==0 ? Color::WHITE : Color::BLACK;
            Bitboard bb = board.pieces(pt, col);
            int v = PIECE_VAL[t];

            while(!bb.empty())
            {
                int sq = bb.lsb();
                bb.clear(sq);
                int idx = (col==Color::WHITE) ? (sq^56) : sq;

                int pstVal;
                if(t==5) // king -- smoothly blended MG/EG instead of a hard switch
                    pstVal = (PST_KING_MG[idx]*phase + PST_KING_EG[idx]*(MAX_PHASE-phase)) / MAX_PHASE;
                else
                    pstVal = PST[t][idx];

                if(col==Color::WHITE) score += v+pstVal;
                else                  score -= v+pstVal;
            }
        }
    }

    // ---- additional positional features --------------------------------
    score += BISHOP_PAIR_BONUS * ((board.pieces(PieceType::BISHOP, Color::WHITE).count()>=2) -
                                   (board.pieces(PieceType::BISHOP, Color::BLACK).count()>=2));

    {
        Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
        Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);

        Bitboard wr = board.pieces(PieceType::ROOK, Color::WHITE);
        while(!wr.empty())
        {
            int sq=wr.lsb(); wr.clear(sq);
            Bitboard fileMask = attacks::MASK_FILE[sq&7];
            bool ownPawn   = !(fileMask & whitePawns).empty();
            bool enemyPawn = !(fileMask & blackPawns).empty();
            if(!ownPawn && !enemyPawn) score += ROOK_OPEN_FILE_BONUS;
            else if(!ownPawn)          score += ROOK_SEMI_OPEN_BONUS;
        }
        Bitboard br = board.pieces(PieceType::ROOK, Color::BLACK);
        while(!br.empty())
        {
            int sq=br.lsb(); br.clear(sq);
            Bitboard fileMask = attacks::MASK_FILE[sq&7];
            bool ownPawn   = !(fileMask & blackPawns).empty();
            bool enemyPawn = !(fileMask & whitePawns).empty();
            if(!ownPawn && !enemyPawn) score -= ROOK_OPEN_FILE_BONUS;
            else if(!ownPawn)          score -= ROOK_SEMI_OPEN_BONUS;
        }
    }

    score += pawn_structure_score(board, Color::WHITE) - pawn_structure_score(board, Color::BLACK);
    score += mobility_score(board, Color::WHITE) - mobility_score(board, Color::BLACK);

    // king safety only really matters before the position opens up
    if(phase>MAX_PHASE/2)
    {
        score -= king_shield_penalty(board, Color::WHITE);
        score += king_shield_penalty(board, Color::BLACK);
    }

    if(board.sideToMove()==Color::BLACK) 
		score = -score;
    return score;
}

int evaluate(Board &board)
{
#ifdef FORCE_CLASSICAL
    return classical_evaluate(board);
#else
    if(!nnue::g_weights.loaded)
    {
        return classical_evaluate(board);
    }
    return nnue::nnue_evaluate(board);
#endif
}

int mvv_lva(Board &board, const Move &m)
{
    int killer=(int)board.at(m.from()).type();
    int victim;
    if(m.typeOf()==Move::ENPASSANT) 
		victim=0; 
    else                                   
		victim=(int)board.at(m.to()).type();
    if(victim>5) 
		victim=0;
    return PIECE_VAL[victim]*50-killer;
}

class Engine
{
public:
    long long nodes = 0;
    int tt_exact_cutoffs = 0;
    vector<TTEntry> tt;
    Move killers[MAX_PLY+1][2];
    int history[64][64];
    chrono::steady_clock::time_point start;
    int budget_ms = 0;
    bool timed = 0;
    bool stopped = 0;
    int generation = 0;

    Engine()
    {
        tt.assign(TT_SIZE, TTEntry{});
    }

    void clear_tt()
    {
        std::fill(tt.begin(), tt.end(), TTEntry{});
    }

    void new_search()
    {
        nodes = 0;
        tt_exact_cutoffs = 0;
        stopped = 0;
        generation++;   // marks every TT entry from here on as belonging to THIS search --
                        // see the TT probe in alpha_beta for why this matters
        memset(killers, 0, sizeof(killers));
        memset(history, 0, sizeof(history));
        start = chrono::steady_clock::now();
    }

    int elapsed_ms()
    {
        auto now = chrono::steady_clock::now();
        return (int)chrono::duration_cast<chrono::milliseconds>(now-start).count();
    }

    void check_time()
    {
        if( timed && ((nodes&2047)==0) && (elapsed_ms() >= budget_ms) )
            stopped = true;
    }

    // Allocation-free move ordering: scores moves into a fixed stack array,
    // then does a lazy selection sort directly on the Movelist (swap the
    // best remaining move into place each step). Since cutoffs usually land
    // on move 1-2, we often never even finish "sorting" the rest.
    void order_moves(Board &board, Movelist &moves, Move tt_move, int ply)
    {
        static thread_local int scores[chess::constants::MAX_MOVES];
        int n = moves.size();

        loop(i,n)
        {
            Move m = moves[i];
            if(m==tt_move)
                scores[i]=2000000000;
            else if(board.isCapture(m) || m.typeOf()==Move::PROMOTION)
                scores[i]=1000000000+mvv_lva(board,m);
            else if(m==killers[ply][0])
                scores[i]=900000000;
            else if(m==killers[ply][1])
                scores[i]=800000000;
            else
                scores[i]=history[m.from().index()][m.to().index()];
        }

        loop(i,n)
        {
            int best_j=i;
            loop(j,n)
            {
                if(j>i && scores[j]>scores[best_j]) best_j=j;
            }
            if(best_j!=i)
            {
                std::swap(moves[i],moves[best_j]);
                std::swap(scores[i],scores[best_j]);
            }
        }
    }

    void record_cutoff(Board &board, const Move &move, int depth, int ply)
    {
        if(board.isCapture(move) || move.typeOf()==Move::PROMOTION) return;
        if(killers[ply][0]!=move)
        {
            killers[ply][1]=killers[ply][0];
            killers[ply][0]=move;
        }
        history[move.from().index()][move.to().index()] += depth*depth;
    }

    int quiescence(Board &board,int alpha,int beta,int ply)//first complete trade sequence then only return eval
    {
        nodes++;
        check_time();
        if(stopped) 
			return 0;
        // isRepetition(1) -- NOT the default isRepetition() (count=2, i.e.
        // literal 3-fold) -- per chess.hpp's own doc comment: "set this to
        // 1 if you are writing a chess engine." Treating the 2nd occurrence
        // as drawish for search purposes (rather than waiting for a full
        // 3-fold within this exact search tree) matters a lot here because
        // the TT persists across moves (only cleared on ucinewgame): a
        // score cached while a position had only repeated once can get
        // reused via TT cutoff on a LATER move, short-circuiting the search
        // before it ever reaches the deeper node where a 3rd real
        // occurrence would otherwise be detected. Using count=1 closes
        // that gap by flagging the draw a move earlier, before it can hide
        // behind a stale cutoff.
        if(board.isHalfMoveDraw() || board.isRepetition(1))
            return 0;

        bool in_check = board.inCheck();
        Movelist moves;
        int best;

        if(in_check) 
        {
            movegen::legalmoves(moves, board);
            if(moves.empty()) 
				return -(MATE_SCORE-ply); //checks if checkmate 
            best=-INF; // all possibilities open
        }
        else
        {
            int stand = evaluate(board);
            movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves,board);

            // An empty capture list usually just means "stand-pat is our
            // answer" -- but if there are actually zero legal moves of any
            // kind, this is a real stalemate and must score as a draw, not
            // whatever the raw eval says. anylegalmoves() early-exits on
            // the first legal move it finds, so this costs nothing extra
            // in the overwhelmingly common non-stalemate case, and we only
            // even call it when the capture list is empty.
            if(moves.empty() && !movegen::anylegalmoves(board)) 
				return 0;

            if(stand>=beta) 
				return stand;               
            if(stand>alpha) 
				alpha=stand;
            best=stand;
        }

        order_moves(board,moves,Move::NO_MOVE,min(ply,MAX_PLY));

        loop(i,moves.size())
        {
            Move move = moves[i];
            nnue::apply_move_delta(board,move,nnue::g_state);
            board.makeMove(move);
            int score = -quiescence(board,-beta,-alpha,ply+1);
            board.unmakeMove(move);
            nnue::undo_move(nnue::g_state);
            if(stopped) 
				return 0;
            if(score>best)  
				best=score;
            if(score>alpha) 
				alpha=score;
            if(alpha>=beta) 
				break;
        }
        return best;
    }

    int alpha_beta(Board &board,int depth,int alpha,int beta,int ply)
    {
        nodes++;
        check_time();
        if(stopped) 
			return 0;

        int alpha_orig = alpha;

        // Repetition/50-move check MUST happen before the TT probe below.
        // A repeated position shares its zobrist hash with the earlier
        // (pre-repetition) visit that stored a real, non-draw score in the
        // TT. Probing the TT first lets that stale EXACT entry short-circuit
        // the search and mask the draw every time the position recurs --
        // which is exactly why a search can loop a forced repetition while
        // still reporting a non-zero eval.
        if(ply>0 && (board.isHalfMoveDraw() || board.isRepetition(1)))
            return 0;

        uint64_t h = board.hash();
        size_t slot = h & (TT_SIZE-1);

        Move tt_move = Move::NO_MOVE;
        TTEntry &e = tt[slot];
        if(e.key==h)
        {
            tt_move = e.best;   // safe to reuse for move-ordering regardless of age

            // Cutoffs are only trusted from THIS search (matching generation).
            // The ordering fix above (repetition check before TT probe) stops
            // a node from directly returning a stale score for ITSELF, but it
            // can't stop an ANCESTOR's stale cutoff from skipping past this
            // node's subtree entirely, never giving it the chance to detect
            // its own repetition in the first place. Since the TT persists
            // across real moves (only new_search() bumps the generation, no
            // full clear), an entry cached during an earlier move -- when a
            // position hadn't repeated yet -- could otherwise get reused on
            // a later move after that same position actually has, silently
            // masking the draw. Restricting cutoffs to the current
            // generation closes that gap while keeping full cutoff benefit
            // across all depths of THIS move's iterative deepening (they
            // all share one generation).
            if(e.gen==generation && e.depth>=depth && ply>0)
            {
                int s = e.score;
                if(s>MATE_BOUND) 
					s -= ply;
                if(s<-MATE_BOUND) 
					s += ply;
                if(e.flag==EXACT) 
				{ 
					tt_exact_cutoffs++;
					return s; 
				}
                if(e.flag==LOWER) 
					alpha = max(alpha, s);
                else if(e.flag==UPPER) 
					beta = min(beta,s);
                if(alpha>=beta) 
					return s;
            }
        }

        if(depth<=0) 
            return quiescence(board,alpha,beta,ply);

        bool in_check = board.inCheck();

        Movelist moves;
        movegen::legalmoves(moves,board);

        if(moves.empty()) //mate or stalemate
        {
			if(in_check)
				return -(MATE_SCORE-ply);
			else
				return 0; 
        }

        /*Null-move pruning is "If I give my opponent a free move and I'm still winning, this
        branch is so good they'd never allow it so lets prune it." This is not applied when 
		in checknear the root, near mate scores (unsafe), and when the side to move has no 
		non-pawn material (zugzwang risk).
		*/

        if(!in_check && depth>=3 && ply>0 && beta<MATE_BOUND && board.hasNonPawnMaterial(board.sideToMove()))
        {
            int R = (depth>=6)?3:2;
            board.makeNullMove();
            int null_score=-alpha_beta(board,depth-1-R,-beta,-beta+1,ply+1);
            board.unmakeNullMove();
            if(stopped) 
				return 0;
            if(null_score>=beta) 
				return beta;
        }

        order_moves(board,moves,tt_move,min(ply,MAX_PLY));

        int best_score=-INF;
        Move best_move=moves[0];

        loop(i,moves.size())
        {
            Move move = moves[i];
            bool is_quiet = !(board.isCapture(move) || move.typeOf()==Move::PROMOTION);

            nnue::apply_move_delta(board,move,nnue::g_state);
            board.makeMove(move);

            bool gives_check = board.inCheck();

            int score;

            if(i==0)
            {
                // First move (tt_move / best capture from order_moves) is
                // our presumed best -- search it with the full window.
                score = -alpha_beta(board,depth-1,-beta,-alpha,ply+1);
            }
            else
            {
                int search_depth = depth-1;
                // ---- Late move reductions -------------------------------
                // Moves sorted late by order_moves are unlikely to be best.
                // Search them shallower first as part of the scout below;
                // only pay for a full-depth re-search if they unexpectedly
                // beat alpha.
                bool reduced = (i>=4 && depth>=3 && is_quiet && !gives_check && move!=tt_move);
                if(reduced) 
					search_depth -= 1;

                // ---- PVS scout -------------------------------------------
                // Every non-first move gets a cheap null-window probe first:
                // it can only prove "no better than alpha" or "better than
                // alpha", which is all we need to justify a full-window
                // re-search. This avoids paying full-window cost on moves
                // that order_moves already ranked as unlikely to be best.
                score = -alpha_beta(board, search_depth, -alpha-1, -alpha, ply+1);

                // Reduced scout beat alpha -- re-verify at full depth
                // (still null window) before trusting it enough to widen.
                if(reduced && score>alpha)
                    score = -alpha_beta(board, depth-1, -alpha-1, -alpha, ply+1);

                if(score>alpha && score<beta)
                    score = -alpha_beta(board, depth-1, -beta, -alpha, ply+1);
            }

            board.unmakeMove(move);
            nnue::undo_move(nnue::g_state);
            if(stopped) 
				return 0;

            if(score>best_score)
            {
                best_score=score;
                best_move=move;
            }
            alpha = max(alpha,best_score);
            if(alpha >= beta)
            {
                record_cutoff(board,move,depth,min(ply,MAX_PLY));
                break;
            }
        }

        // ---- TT store (mate scores converted to be node-relative) ----------
		int flag=-1;
		if((best_score<=alpha_orig))
			flag=UPPER;
		else if(best_score>=beta)
			flag=LOWER;
		else
			flag=EXACT;

        int stored = best_score;
        if(stored>MATE_BOUND) 
			stored += ply;
        if(stored<-MATE_BOUND) 
			stored-=ply;

        // Depth-preferred replacement: don't let a shallow probe evict a
        // deeper, more expensive entry it happens to hash-collide with.
        // Always replace on a genuinely new position (different key), a
        // stale entry from an earlier move (older generation -- otherwise a
        // deep old entry could permanently block a fresh one for a
        // frequently-recurring position, e.g. in a long shuffling
        // endgame), or an equal/deeper re-search within the same search.
        if(e.key!=h || e.gen!=generation || depth>=e.depth)
            e = TTEntry{h,stored,depth,flag,best_move,generation};

        return best_score;
    }
};

// UCI score string: "cp x" for normal scores, "mate y" for forced mates.
string uci_score(int score)
{
    if(abs(score)>MATE_BOUND)
    {
        int plies=MATE_SCORE-abs(score);
        int moves=(plies+1)/2;
        return "mate "+to_string(score>0?moves:-moves);
    }
    return "cp "+to_string(score);
}

// alpha_beta no longer builds a PV in-search (that required per-node vector
// allocation via pv.clear()/push_back()/insert()). Instead we walk
// tt[hash].best move-by-move from the root after each depth completes --
// this reflects the real full PV since every position along the best line
// has an entry by the time a depth finishes.
vMove extract_pv(Board board, Engine &engine, int max_len)
{
    vMove pv;
    vector<uint64_t> seen; // guards against loops from repetition or a hash collision

    for(int i=0;i<max_len;i++)
    {
        uint64_t h = board.hash();
        if(std::find(seen.begin(),seen.end(),h)!=seen.end()) 
			break;
        seen.push_back(h);

        TTEntry &e = engine.tt[h & (TT_SIZE-1)];
        if(e.key!=h) 
			break;

        Move best = e.best;

        Movelist ml;
        movegen::legalmoves(ml,board);
        bool legal=false;
        for(auto m:ml) 
			if(m==best){ legal=true; break; }
        if(!legal) 
			break; // stale/collided entry -- don't trust it

        pv.push_back(best);
        board.makeMove(best);
    }

    return pv;
}

//try all depths from 1 to i, stop if seems wont finish depth
Move search_best(Engine &engine,Board &board,int max_depth,int budget_ms,bool timed)
{
    engine.timed=timed;
    engine.budget_ms=budget_ms;
    engine.new_search();

    Movelist root;
    movegen::legalmoves(root,board);
    if(root.empty()) 
		return Move::NO_MOVE;

    Move best=root[0];

    for(int depth=1; depth<=max_depth; depth++)
    {
        int score=engine.alpha_beta(board,depth,-INF,INF,0);
        if(engine.stopped) 
			break;          

        vMove full_pv = extract_pv(board,engine,depth);

        if(!full_pv.empty())    
			best=full_pv[0];

        // Evaluation is always shown relative to white to avoid confusion
        int display_score = (board.sideToMove()==Color::WHITE) ? score : -score;

        // tell the GUI what we're thinking
        cout << "info depth " << depth<<" score "<< uci_score(display_score)
             << " nodes " << engine.nodes
             << " time " << engine.elapsed_ms()
             << " pv";
        for(auto m:full_pv) 
			cout<<" "<< uci::moveToUci(m);
        cout<<"\n"<<flush;

        if(abs(score)>MATE_BOUND) 
			break; //found a forced mate, stop early

        //don't start a depth we can't finish
        if(timed && engine.elapsed_ms()*2>=budget_ms) 
			break;
    }
    return best;
}

// ----------------------------------------------------------------------------
//  UCI loop (stolen from ai)
// ----------------------------------------------------------------------------
void uci_loop()
{
    Board board(START_FEN);
    Engine engine;           
    string line;

    while(getline(cin,line))
    {
        istringstream iss(line);
        string token;
        iss>>token;

        if(token=="uci")
        {
            cout<<"id name MyEngine\n";
            cout<<"id author hp\n";
            cout<<"uciok\n"<<flush;
        }
        else if(token=="isready")
        {
            cout<<"readyok\n"<<flush;
        }
        else if(token=="ucinewgame")
        {
            board=Board(START_FEN);
            engine.clear_tt();
            nnue::full_refresh(board,nnue::g_state);
        }
        else if(token=="position")
        {
            string sub;
            iss>>sub;
            if(sub=="startpos")
            {
                board=Board(START_FEN);
            }
            else if(sub=="fen")
            {
                string fen,part;
                for(int i=0;i<6 && iss>>part;i++)
                    fen += (i? " ":"") + part;
                board=Board(fen);
            }
            string mv;
            while(iss>>mv)
            {
                if(mv=="moves") 
					continue;
                Movelist ml;
                movegen::legalmoves(ml,board);
                for(auto m:ml)
                    if(uci::moveToUci(m)==mv){ 
						board.makeMove(m); 
						break; 
					}
            }
            nnue::full_refresh(board,nnue::g_state);
        }
        else if(token=="go")
        {
            // parse the clock
            int wtime=-1, btime=-1, winc=0, binc=0, movetime=-1, depth=-1, movestogo=-1;
            string t;
            while(iss>>t)
            {
                if     (t=="wtime")     iss>>wtime;
                else if(t=="btime")     iss>>btime;
                else if(t=="winc")      iss>>winc;
                else if(t=="binc")      iss>>binc;
                else if(t=="movetime")  iss>>movetime;
                else if(t=="depth")     iss>>depth;
                else if(t=="movestogo") iss>>movestogo;
            }

            int  max_depth = MAX_PLY;
            int  budget    = 0;
            bool timed     = true;

            if(movetime>0)
            {
                budget = movetime - 30; 
            }
            else if(wtime>=0 || btime>=0)
            {
                bool white = (board.sideToMove()==Color::WHITE);
                int  mytime = white ? wtime : btime;
                int  myinc  = white ? winc  : binc;
                if(mytime<0) 
					mytime=0;

                int  togo = (movestogo>0) ? movestogo : 25;
                budget = mytime/togo + myinc/2 - 30;

                int safety_cap = mytime/2;
				//if format is 40 moves in 5 min, then it gives issue on last move.
                budget=min(budget,safety_cap);
            }
            else
            {
                // no clock info -> behave like the old fixed-depth engine
                timed = false;
                max_depth = (depth>0) ? depth : PLAY_DEPTH;
            }

            if(depth>0)
				max_depth = min(max_depth, depth);
            if(timed && budget < 5) 
				budget = 5;            // never go below 5ms

            Move best = search_best(engine, board, max_depth, budget, timed);
            if(best==Move::NO_MOVE)
            {
                Movelist ml;
                movegen::legalmoves(ml,board);
                if(!ml.empty()) best = ml[0];
            }
            cout<<"bestmove "<<uci::moveToUci(best)<<"\n"<<flush;
        }
        else if(token=="quit")
        {
            break;
        }
    }
}


string resolve_weights_path(const char* argv0)
{
	/*
	This is to deal with the path issue that had happened because cutechess is in a different directory so if 
	it launches this program from that directory then issue. 
	*/

    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path exe_path = fs::absolute(fs::path(argv0), ec);
    if(!ec)
    {
        fs::path candidate = exe_path.parent_path() / NNUE_WEIGHTS_FILE;
        if(fs::exists(candidate, ec) && !ec)
            return candidate.string();
    }
    return NNUE_WEIGHTS_FILE;  
}

int main(int argc, char** argv)
{
    string weights_path = (argc > 0) ? resolve_weights_path(argv[0]) : NNUE_WEIGHTS_FILE;

    if(!nnue::load_weights(weights_path))
    {
        cerr << "warning: failed to load " << weights_path
             << " -- falling back to classical_evaluate()\n";
    }
    else
    {
        Board startBoard(START_FEN);
        nnue::full_refresh(startBoard,nnue::g_state);
    }

    uci_loop();
}
