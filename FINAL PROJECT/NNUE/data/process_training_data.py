import time
import numpy as np
import pandas as pd
import torch
import chess
from multiprocessing import Pool, cpu_count

FILES = [
    "FilteredEvals.csv",
    "FilteredEvals1.csv",
    "FilteredEvals2.csv",
    "FilteredEvals3.csv"
]

PIECE_INDICES_FILE = "piece_indices.pt"
CASTLE_FILE = "castle.pt"
EP_FILE = "ep.pt"
TARGETS_FILE = "targets.pt"

MAX_PIECES = 32
PAD_IDX = 768           #dummy "no piece" slot, deliberately NOT reused by any other feature
NUM_FEATURES = 768      #6 piece types * 2 (us/them) * 64 squares

CP_CLIP = 1000.0        #clip evaluation before training to remove outliers

BATCH_SIZE = 2000       #rows handed to each worker per task
NUM_WORKERS = max(1,cpu_count()-1) #if possible will leave one core free for OS and other tasks


def board_to_features(board):
    """
    Board is mirrored when Black is to move (idea is that chess is mostly symmetric)
    Castling rights=[us-kingside, us-queenside, them-kingside, them-queenside]
    """
    
    us = board.turn
    them = not us
    indices = []

    for square, piece in board.piece_map().items():
        color_key=0 if piece.color == us else 1
        type_key=piece.piece_type-1  # PAWN=0 ... KING=5 
        sq = square if us == chess.WHITE else chess.square_mirror(square)
        indices.append(color_key*6*64 + type_key*64 + sq)

    while len(indices)<MAX_PIECES:
        indices.append(PAD_IDX)
    
    castle=(
        board.has_kingside_castling_rights(us),
        board.has_queenside_castling_rights(us),
        board.has_kingside_castling_rights(them),
        board.has_queenside_castling_rights(them)
    )
    
    ep=[False]*8 #en passant squares
    if board.ep_square is not None:
        ep[chess.square_file(board.ep_square)]=True
    return indices,castle,ep


def process_batch(args):
    fens,evals = args

    n=len(fens)

    idx_arr = np.full((n,MAX_PIECES), PAD_IDX, dtype=np.int64)
    castle_arr = np.zeros((n,4), dtype=bool)
    ep_arr = np.zeros((n,8), dtype=bool)
    target_arr = np.empty(n, dtype=np.float32)

    for i in range(n):
        board = chess.Board(fens[i])
        indices, castle, ep = board_to_features(board)
        idx_arr[i] = indices
        castle_arr[i] = castle
        ep_arr[i] = ep

        # depth-0 evals: plain centipawn floats, no mate-score strings to worry about
        eval_cp = float(evals[i])

        if board.turn == chess.BLACK:
            eval_cp = -eval_cp

        target_arr[i] = max(-CP_CLIP, min(CP_CLIP, eval_cp))

    return idx_arr,castle_arr,ep_arr,target_arr


def chunk_list(lst, size):
    for i in range(0, len(lst), size):
        yield lst[i:i + size]


def main():

    print("Loading CSV files...")

    all_fens = []
    all_evals = []

    for file in FILES:
        print(f"Reading {file}")
        df = pd.read_csv(file, usecols=["FEN", "Evaluation"])
        all_fens.extend(df["FEN"].tolist())
        all_evals.extend(df["Evaluation"].tolist())

    total = len(all_fens)
    print(f"Total positions: {total}")
    print(f"Using {NUM_WORKERS} worker processes")

    batches = list(zip(
        chunk_list(all_fens, BATCH_SIZE),
        chunk_list(all_evals, BATCH_SIZE)
    ))

    idx_parts = []
    castle_parts = []
    ep_parts = []
    target_parts = []

    start = time.time()
    done_rows = 0
    next_percent = 1

    with Pool(NUM_WORKERS) as pool:

        for idx_arr, castle_arr, ep_arr, target_arr in pool.imap(process_batch, batches):

            idx_parts.append(idx_arr)
            castle_parts.append(castle_arr)
            ep_parts.append(ep_arr)
            target_parts.append(target_arr)

            done_rows += idx_arr.shape[0]

            progress = done_rows * 100 // total

            while progress >= next_percent:

                elapsed = time.time()-start
                speed = done_rows/max(elapsed, 1e-9)
                eta = (total - done_rows)/max(speed, 1e-9)

                print(f"{next_percent:3d}% processed={done_rows:,}/{total:,} ETA={eta:.1f}s")

                next_percent += 1

    print("\nConcatenating...")

    piece_indices = torch.from_numpy(np.concatenate(idx_parts, axis=0))
    castle_t = torch.from_numpy(np.concatenate(castle_parts, axis=0))
    ep_t = torch.from_numpy(np.concatenate(ep_parts, axis=0))
    targets_t = torch.from_numpy(np.concatenate(target_parts, axis=0))

    print("Saving...")

    torch.save(piece_indices, PIECE_INDICES_FILE)
    torch.save(castle_t, CASTLE_FILE)
    torch.save(ep_t, EP_FILE)
    torch.save(targets_t, TARGETS_FILE)

    print("Done!")

    print("piece_indices :", piece_indices.shape)
    print("castle        :", castle_t.shape)
    print("ep            :", ep_t.shape)
    print("targets       :", targets_t.shape)


if __name__ == "__main__":
    main()
