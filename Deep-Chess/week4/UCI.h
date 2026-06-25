#ifndef UCI_H
#define UCI_H

#include "Engine.h"
#include <string>

/**
 * @brief UCI (Universal Chess Interface) protocol handler.
 *
 * Allows the engine to communicate with any UCI-compatible chess GUI
 * (Arena, CuteChess, Lichess, etc.).
 *
 * Protocol reference: http://wbec-ridderkerk.nl/html/UCIProtocol.html
 */
class UCI {
public:
    UCI();

    /**
     * @brief Start the UCI input loop.
     * Reads commands from stdin and responds on stdout.
     * Blocks until "quit" is received.
     */
    void loop();

private:
    Engine engine_;

    // Handle individual UCI commands
    void handleUci();
    void handleIsReady();
    void handleNewGame();
    void handlePosition(const std::string& line);
    void handleGo(const std::string& line);
    void handleQuit();
};

#endif // UCI_H
