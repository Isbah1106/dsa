#include "Assets.hpp"

#include <filesystem>

namespace {
bool tryOpen(sf::Font& font, const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    return font.openFromFile(path);
}
}  // namespace

bool Fonts::load(const std::string& exeDir, std::string* error) {
    namespace fs = std::filesystem;
    const fs::path base = exeDir.empty() ? fs::current_path() : fs::path(exeDir);
    const fs::path candidates[] = {
        base / "assets" / "fonts",
        base / ".." / "assets" / "fonts",
        base / ".." / ".." / "assets" / "fonts",
        fs::current_path() / "assets" / "fonts",
    };

    for (const fs::path& dir : candidates) {
        if (tryOpen(regular, dir / "DejaVuSans.ttf") && tryOpen(bold, dir / "DejaVuSans-Bold.ttf") &&
            tryOpen(title, dir / "DejaVuSerif-Bold.ttf") && tryOpen(mono, dir / "DejaVuSansMono.ttf")) {
            return true;
        }
    }

    // Last resort on Windows: system fonts, so the game still starts.
    const fs::path sys = "C:/Windows/Fonts";
    if (tryOpen(regular, sys / "segoeui.ttf") || tryOpen(regular, sys / "arial.ttf")) {
        if (!tryOpen(bold, sys / "segoeuib.ttf") && !tryOpen(bold, sys / "arialbd.ttf")) bold = regular;
        if (!tryOpen(title, sys / "georgiab.ttf")) title = bold;
        if (!tryOpen(mono, sys / "consola.ttf")) mono = regular;
        return true;
    }

    if (error) *error = "Could not find assets/fonts (DejaVuSans.ttf). Run the game from the build folder.";
    return false;
}
