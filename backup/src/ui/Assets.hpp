#pragma once

#include <SFML/Graphics/Font.hpp>

#include <string>

struct Fonts {
    sf::Font regular;
    sf::Font bold;
    sf::Font title;
    sf::Font mono;

    // Looks for assets/fonts next to the executable, then in parent folders.
    bool load(const std::string& exeDir, std::string* error = nullptr);
};
