#!/usr/bin/env python3
import json
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

import chess
import chess.engine

BASE_DIR = Path(__file__).resolve().parent
PROJECT_DIR = BASE_DIR.parent
ENGINE_PATHS = {
    "basic": PROJECT_DIR / "classical/source code/classical",
    "advanced": PROJECT_DIR / "NNUE/source code/nnue",
}

GAMES = {}
LOCK = threading.Lock()


class GameSession:
    def __init__(self, bot_key: str):
        self.game_id = uuid.uuid4().hex
        self.bot_key = bot_key
        self.board = chess.Board()
        self.engine = chess.engine.SimpleEngine.popen_uci(
            str(ENGINE_PATHS[bot_key]),
            cwd=str(ENGINE_PATHS[bot_key].parent),
        )
        self.bot_color = chess.BLACK

    def legal_targets_for_square(self, square_text: str):
        square = chess.parse_square(square_text)
        piece = self.board.piece_at(square)
        if piece is None or piece.color != chess.WHITE:
            raise ValueError("Pick one of your own pieces.")

        return [move.uci() for move in self.board.legal_moves if move.from_square == square]

    def play_human_move(self, move_text: str):
        if len(move_text) == 4:
            promotion_candidate = chess.Move.from_uci(move_text + 'q')
            if promotion_candidate in self.board.legal_moves:
                move_text += 'q'

        move = chess.Move.from_uci(move_text)
        if move not in self.board.legal_moves:
            raise ValueError("That move is not legal for the current position.")

        self.board.push(move)
        human_check_text = self._check_suffix()

        if self.board.is_game_over():
            return self._status_payload(self._result_message() + human_check_text)

        result = self.engine.play(self.board, chess.engine.Limit(depth=4))
        self.board.push(result.move)
        bot_check_text = self._check_suffix()

        if self.board.is_game_over():
            return self._status_payload(self._result_message() + bot_check_text)

        return self._status_payload("Bot moved." + bot_check_text)

    def _check_suffix(self):
        if self.board.is_check():
            return " Check!"
        return ""

    def _result_message(self):
        outcome = self.board.outcome()
        if outcome is None:
            return "Game finished."

        winner = outcome.winner
        termination = outcome.termination

        if termination == chess.Termination.CHECKMATE:
            if winner == chess.WHITE:
                return "Checkmate! You won the game."
            if winner == chess.BLACK:
                return "Checkmate! Bot won the game."
            return "Checkmate! Draw game."

        if termination == chess.Termination.STALEMATE:
            return "Stalemate! Draw game."

        if termination == chess.Termination.THREEFOLD_REPETITION:
            return "Draw by repetition."

        if termination == chess.Termination.FIFTY_MOVES:
            return "Draw by fifty-move rule."

        if termination == chess.Termination.INSUFFICIENT_MATERIAL:
            return "Draw by insufficient material."

        return "Game finished."

    def _status_payload(self, message: str):
        return {
            "game_id": self.game_id,
            "bot": self.bot_key,
            "fen": self.board.fen(),
            "turn": 'w' if self.board.turn == chess.WHITE else 'b',
            "status": "finished" if self.board.is_game_over() else "playing",
            "message": message,
            "legal_moves": [],
        }

    def close(self):
        try:
            self.engine.quit()
        except Exception:
            pass


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path in ("/", "/index.html"):
            self._serve_file(BASE_DIR / "index.html", "text/html; charset=utf-8")
            return
        if parsed.path == "/styles.css":
            self._serve_file(BASE_DIR / "styles.css", "text/css; charset=utf-8")
            return
        if parsed.path == "/script.js":
            self._serve_file(BASE_DIR / "script.js", "application/javascript; charset=utf-8")
            return
        self._send_json(404, {"error": "not found"})

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path not in {"/api/new-game", "/api/legal-moves", "/api/move"}:
            self._send_json(404, {"error": "not found"})
            return

        try:
            content_len = int(self.headers.get("Content-Length", "0"))
            body = json.loads(self.rfile.read(content_len).decode("utf-8"))
        except Exception:
            self._send_json(400, {"error": "invalid json body"})
            return

        if parsed.path == "/api/new-game":
            bot_key = body.get("bot", "basic")
            if bot_key not in ENGINE_PATHS:
                self._send_json(400, {"error": "unknown bot"})
                return

            game = GameSession(bot_key)
            with LOCK:
                GAMES[game.game_id] = game

            self._send_json(200, {
                "game_id": game.game_id,
                "bot": bot_key,
                "fen": game.board.fen(),
                "turn": 'w',
                "status": "playing",
                "message": f"New game started. You are White against {bot_key.capitalize()} Bot.",
                "legal_moves": [],
            })
            return

        if parsed.path == "/api/legal-moves":
            game_id = body.get("game_id")
            square_text = body.get("square")
            if not game_id or not square_text:
                self._send_json(400, {"error": "game_id and square are required"})
                return

            with LOCK:
                game = GAMES.get(game_id)
            if not game:
                self._send_json(404, {"error": "game not found"})
                return

            try:
                legal_moves = game.legal_targets_for_square(square_text)
                self._send_json(200, {
                    "game_id": game.game_id,
                    "legal_moves": legal_moves,
                })
            except Exception as exc:
                self._send_json(400, {"error": str(exc)})
            return

        game_id = body.get("game_id")
        move_text = body.get("move")
        if not game_id or not move_text:
            self._send_json(400, {"error": "game_id and move are required"})
            return

        with LOCK:
            game = GAMES.get(game_id)
        if not game:
            self._send_json(404, {"error": "game not found"})
            return

        try:
            payload = game.play_human_move(move_text)
            if payload["status"] == "finished":
                self._send_json(200, payload)
                with LOCK:
                    game.close()
                    GAMES.pop(game_id, None)
                return
            self._send_json(200, payload)
        except Exception as exc:
            self._send_json(400, {"error": str(exc)})

    def _serve_file(self, file_path: Path, content_type: str):
        try:
            data = file_path.read_bytes()
        except FileNotFoundError:
            self._send_json(404, {"error": "file not found"})
            return

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _send_json(self, status_code: int, payload):
        content = json.dumps(payload).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, format, *args):
        return


if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", 8000), Handler)
    print("Serving chess website at http://127.0.0.1:8000")
    server.serve_forever()
