import chess
import chess.engine
from pathlib import Path

BASE = Path("/home/aayush/chessAI/Deep-Chess/FINAL PROJECT")

ENGINE_PATHS = {
    "classical": BASE / "classical/source code/classical",
    "nnue": BASE / "NNUE/source code/nnue",
    "stockfish": Path("/usr/games/stockfish"),
}

PLAYER1 = "classical"
PLAYER2 = "stockfish"
PLAYER1_COLOR = chess.WHITE
LIMIT = chess.engine.Limit(depth=4)


def open_engine(name: str):
    path = ENGINE_PATHS[name]
    cwd = str(path.parent)
    return chess.engine.SimpleEngine.popen_uci(str(path), cwd=cwd)


board = chess.Board()
engine1 = open_engine(PLAYER1)
engine2 = open_engine(PLAYER2)

try:
    move_number = 1
    while not board.is_game_over():
        if board.turn == PLAYER1_COLOR:
            move = engine1.play(board, LIMIT).move
            engine_name = PLAYER1
        else:
            move = engine2.play(board, LIMIT).move
            engine_name = PLAYER2

        board.push(move)
        print(f"Move {move_number}: {engine_name} played {move.uci()}")
        print(board)
        print()
        move_number += 1
finally:
    engine1.quit()
    engine2.quit()

print("Final result:", board.result(claim_draw=True))