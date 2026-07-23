const boardEl = document.getElementById('board');
const statusEl = document.getElementById('status');
const newGameBtn = document.getElementById('newGameBtn');
const botBtns = document.querySelectorAll('.bot-btn');

const pieceMap = {
    'p': '♟', 'r': '♜', 'n': '♞', 'b': '♝', 'q': '♛', 'k': '♚',
    'P': '♙', 'R': '♖', 'N': '♘', 'B': '♗', 'Q': '♕', 'K': '♔'
};

let selectedSquare = null;
let currentBot = 'basic';
let currentGame = null;
let currentFen = null;
let legalMoveTargets = [];
let availableMoveUcis = [];

function setStatus(message) {
    statusEl.textContent = message;
}

function startGame(botKey) {
    currentBot = botKey;
    fetch('/api/new-game', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ bot: botKey })
    })
        .then(async (res) => {
            const data = await res.json();
            if (!res.ok) {
                throw new Error(data.error || 'Failed to start game.');
            }
            currentGame = data;
            currentFen = data.fen;
            selectedSquare = null;
            legalMoveTargets = [];
            availableMoveUcis = [];
            renderBoard();
            setStatus(`${botKey === 'basic' ? 'Basic Bot' : 'Advanced Bot'} is ready. You are White.`);
        })
        .catch((err) => {
            setStatus(err.message);
        });
}

function squareName(row, col) {
    const file = String.fromCharCode(97 + col);
    const rank = 8 - row;
    return file + rank;
}

function renderBoard() {
    boardEl.innerHTML = '';
    const fenParts = (currentFen || '8/8/8/8/8/8/8/8 w - - 0 1').split(' ');
    const rows = fenParts[0].split('/');

    for (let row = 0; row < 8; row += 1) {
        for (let col = 0; col < 8; col += 1) {
            const square = document.createElement('button');
            square.className = `square ${(row + col) % 2 === 0 ? 'light' : 'dark'}`;
            const sqName = squareName(row, col);
            square.dataset.square = sqName;
            square.type = 'button';

            let piece = null;
            let boardCol = 0;
            const boardRow = rows[row];
            for (const ch of boardRow) {
                if (/[1-8]/.test(ch)) {
                    boardCol += Number(ch);
                } else {
                    if (boardCol === col) {
                        piece = ch;
                        break;
                    }
                    boardCol += 1;
                }
            }

            if (piece) {
                square.textContent = pieceMap[piece] || '';
            }

            if (selectedSquare === sqName) {
                square.classList.add('selected');
            }

            if (legalMoveTargets.includes(sqName)) {
                square.classList.add('possible');
                const dot = document.createElement('span');
                dot.className = 'move-dot';
                square.appendChild(dot);
            }

            square.addEventListener('click', () => handleSquareClick(sqName));
            boardEl.appendChild(square);
        }
    }
}

function handleSquareClick(square) {
    if (!currentGame) {
        setStatus('Start a game first.');
        return;
    }

    if (currentGame.status === 'finished') {
        setStatus('This game is already finished. Start a new one.');
        return;
    }

    if (currentGame.turn !== 'w') {
        setStatus('Wait for the bot to move.');
        return;
    }

    if (!selectedSquare) {
        fetchLegalMoves(square);
        return;
    }

    if (selectedSquare === square) {
        selectedSquare = null;
        legalMoveTargets = [];
        renderBoard();
        return;
    }

    if (legalMoveTargets.includes(square)) {
        let move = selectedSquare + square;
        const matchingMoves = availableMoveUcis.filter((uci) => uci.startsWith(selectedSquare) && uci.slice(2, 4) === square);

        if (matchingMoves.length > 1) {
            const promotionChoice = window.prompt('Choose promotion piece: q / r / b / n', 'q');
            const normalizedChoice = (promotionChoice || 'q').trim().toLowerCase();
            if (!['q', 'r', 'b', 'n'].includes(normalizedChoice)) {
                setStatus('Promotion must be q, r, b, or n.');
                return;
            }
            const chosenMove = matchingMoves.find((uci) => uci.endsWith(normalizedChoice));
            if (chosenMove) {
                move = chosenMove;
            } else {
                setStatus('That promotion choice is not legal from this square.');
                return;
            }
        } else if (matchingMoves.length === 1) {
            move = matchingMoves[0];
        }

        selectedSquare = null;
        legalMoveTargets = [];
        availableMoveUcis = [];
        sendHumanMove(move);
        return;
    }

    fetchLegalMoves(square);
}

function fetchLegalMoves(square) {
    fetch('/api/legal-moves', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ game_id: currentGame.game_id, square })
    })
        .then(async (res) => {
            const data = await res.json();
            if (!res.ok) {
                throw new Error(data.error || 'Could not load legal moves.');
            }
            selectedSquare = square;
            availableMoveUcis = data.legal_moves || [];
            legalMoveTargets = [...new Set(availableMoveUcis.map((uci) => uci.slice(2, 4)))];
            renderBoard();
            setStatus(`Selected ${square}. Choose a highlighted target.`);
        })
        .catch((err) => {
            setStatus(err.message);
        });
}

function sendHumanMove(move) {
    fetch('/api/move', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ game_id: currentGame.game_id, move })
    })
        .then(async (res) => {
            const data = await res.json();
            if (!res.ok) {
                throw new Error(data.error || 'Move was not accepted.');
            }
            currentFen = data.fen;
            currentGame = data;
            selectedSquare = null;
            legalMoveTargets = [];
            availableMoveUcis = [];
            if (data.status === 'finished') {
                setStatus(data.message);
                renderBoard();
                return;
            }
            setStatus(data.message);
            renderBoard();
        })
        .catch((err) => {
            setStatus(err.message);
        });
}

botBtns.forEach((btn) => {
    btn.addEventListener('click', () => {
        botBtns.forEach((b) => b.classList.remove('active'));
        btn.classList.add('active');
        startGame(btn.dataset.bot);
    });
});

newGameBtn.addEventListener('click', () => {
    startGame(currentBot || 'basic');
});

setStatus('Pick Basic Bot or Advanced Bot to begin.');
startGame('basic');
