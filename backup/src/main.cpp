// Dungeonbound: The Lost Realms — entry point.
//
//   dungeonbound                 play the game
//   dungeonbound --shots <dir>   render every screen to PNG files and exit

#include "ui/Game.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    std::string exeDir;
    if (argc > 0 && argv[0]) {
        std::error_code ec;
        const std::filesystem::path exe = std::filesystem::absolute(argv[0], ec);
        if (!ec) exeDir = exe.parent_path().string();
    }

    Game game(exeDir);
    std::string error;
    if (!game.init(&error)) {
        // Release builds have no console on Windows, so leave a log file as well.
        std::fprintf(stderr, "Dungeonbound could not start: %s\n", error.c_str());
        std::ofstream log("dungeonbound-error.log");
        log << "Dungeonbound could not start: " << error << "\n";
        return 1;
    }

    if (argc >= 3 && std::string(argv[1]) == "--shots") return game.captureScreens(argv[2]);
    return game.run();
}
