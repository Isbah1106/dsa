#include "Game.hpp"

#include "Draw.hpp"
#include "Theme.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <random>

namespace {
const float kW = static_cast<float>(theme::kWindowW);
const float kH = static_cast<float>(theme::kWindowH);
constexpr float kRepeatDelay = 0.2f;    // hold-to-move: delay before repeating
constexpr float kRepeatRate = 0.085f;   // ... and the interval between steps
constexpr float kWalkRate = 0.07f;      // auto-walk step interval
constexpr std::uint32_t kBackdropSeed = 20261018u;
}  // namespace

Game::Game(std::string exeDir) : exeDir_(std::move(exeDir)) {}

bool Game::init(std::string* error) {
    if (!fonts_.load(exeDir_, error)) return false;
    screens::setupTitleMenu(titleMenu_);
    screens::setupPauseMenu(pauseMenu_);
    screens::setupEndMenu(endMenu_);
    backdrop_.newRun(kBackdropSeed);
    backdrop_.revealAll();
    view_ = sf::View(sf::FloatRect({0.f, 0.f}, {kW, kH}));
    return true;
}

std::uint32_t Game::randomSeed() {
    // Six digits: short enough to read out, type back in and share.
    std::random_device device;
    auto ticks = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    return 100000u + (device() ^ ticks) % 900000u;
}

// ============================================================================ flow
void Game::startRun(std::uint32_t seed) {
    session_.newRun(seed);
    const Dungeon& d = session_.dungeon();
    playerVis_ = {static_cast<float>(d.xOf(session_.player().tile)), static_cast<float>(d.yOf(session_.player().tile))};
    hudState_ = HudState();
    hudState_.displayHp = hudState_.ghostHp = static_cast<float>(session_.player().stats.hp);
    overlay_ = OverlayMode::Off;
    walking_ = false;
    heldMove_ = Command::None;
    hoverTile_ = -1;
    playSeconds_ = 0.f;
    seedEntry_ = false;
    toast_ = Toast();
    setScreen(Screen::Exploring);
}

void Game::setScreen(Screen screen, bool fade) {
    screen_ = screen;
    if (fade) fade_ = 1.f;
}

void Game::openHelp() {
    helpReturn_ = screen_;
    screen_ = Screen::Help;
}

void Game::onMenu(int id) {
    switch (id) {
        case menu_id::NewGame:
        case menu_id::NewDungeon:
            startRun(randomSeed());
            break;
        case menu_id::PlaySeed:
            seedEntry_ = true;
            seedText_.clear();
            break;
        case menu_id::HowToPlay:
            openHelp();
            break;
        case menu_id::Quit:
            window_.close();
            break;
        case menu_id::Resume:
            setScreen(Screen::Exploring, false);
            break;
        case menu_id::Restart:
            startRun(session_.seed());
            break;
        case menu_id::MainMenu:
            titleMenu_.select(0);
            seedEntry_ = false;
            setScreen(Screen::Title);
            break;
        default:
            break;
    }
}

void Game::toast(const std::string& text, LogKind kind) {
    toast_.text = text;
    toast_.kind = kind;
    toast_.ttl = text.empty() ? 0.f : 2.6f;
}

// =========================================================================== input
void Game::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) {
        window_.close();
    } else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        applyLetterbox(resized->size);
    } else if (event.is<sf::Event::FocusLost>()) {
        focused_ = false;
        heldMove_ = Command::None;
    } else if (event.is<sf::Event::FocusGained>()) {
        focused_ = true;
    } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        onKey(key->code);
    } else if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
        onText(text->unicode);
    } else if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        onMouseMove(window_.mapPixelToCoords(moved->position, view_));
    } else if (const auto* button = event.getIf<sf::Event::MouseButtonPressed>()) {
        const sf::Vector2f p = window_.mapPixelToCoords(button->position, view_);
        onMouseMove(p);
        if (button->button == sf::Mouse::Button::Left) onMouseDown(p, true);
        else if (button->button == sf::Mouse::Button::Right) onMouseDown(p, false);
    }
}

void Game::onText(char32_t unicode) {
    if (screen_ != Screen::Title || !seedEntry_) return;
    if (unicode >= U'0' && unicode <= U'9' && seedText_.size() < 9) seedText_ += static_cast<char>(unicode);
}

void Game::onKey(sf::Keyboard::Key key) {
    // Seed entry captures the keyboard: digits arrive through TextEntered.
    if (screen_ == Screen::Title && seedEntry_) {
        if (key == sf::Keyboard::Key::Escape) {
            seedEntry_ = false;
        } else if (key == sf::Keyboard::Key::Backspace) {
            if (!seedText_.empty()) seedText_.pop_back();
        } else if (key == sf::Keyboard::Key::Enter && !seedText_.empty()) {
            startRun(static_cast<std::uint32_t>(std::stoul(seedText_)));
        }
        return;
    }

    const Input in = input::translate(key);
    if (in.cmd == Command::None) return;

    switch (screen_) {
        case Screen::Title: {
            if (in.cmd == Command::Help) openHelp();
            else onMenu(titleMenu_.handleInput(in));
            break;
        }
        case Screen::Exploring:
            exploreInput(in);
            break;
        case Screen::Inventory:
        case Screen::BattleItems:
            inventoryAct(inventory_.handleInput(in, session_.inventory()));
            break;
        case Screen::Battle:
            battleEvent(battle_.handleInput(in, session_));
            break;
        case Screen::Paused:
            if (in.cmd == Command::Back) setScreen(Screen::Exploring, false);
            else if (in.cmd == Command::Help) openHelp();
            else onMenu(pauseMenu_.handleInput(in));
            break;
        case Screen::Help:
            if (in.cmd == Command::Back || in.cmd == Command::Help || in.cmd == Command::Confirm || in.cmd == Command::Step)
                screen_ = helpReturn_;
            break;
        case Screen::Victory:
        case Screen::Defeat:
            if (in.cmd == Command::Restart) startRun(session_.seed());
            else onMenu(endMenu_.handleInput(in));
            break;
    }
}

void Game::exploreInput(const Input& in) {
    if (input::isMove(in.cmd)) {
        tryMove(in.cmd);
        heldMove_ = in.cmd;
        repeatTimer_ = kRepeatDelay;
        return;
    }
    switch (in.cmd) {
        case Command::Confirm:
            if (session_.hasPlannedPath()) {
                walking_ = !walking_;
                walkTimer_ = 0.f;
            } else {
                toast("Click a tile to plot a route first.", LogKind::Info);
            }
            break;
        case Command::Step:
            if (session_.hasPlannedPath()) {
                walking_ = false;
                onMoveResult(session_.followPathStep());
            } else {
                toast("Click a tile to plot a route first.", LogKind::Info);
            }
            break;
        case Command::Back:
            if (walking_ || session_.hasPlannedPath()) {
                walking_ = false;
                session_.clearPath();
            } else {
                pauseMenu_.select(0);
                setScreen(Screen::Paused, false);
            }
            break;
        case Command::Inventory:
            walking_ = false;
            inventory_.open(InventoryMode::Explore);
            setScreen(Screen::Inventory, false);
            break;
        case Command::Help:
            walking_ = false;
            openHelp();
            break;
        case Command::Overlay:
            overlay_ = overlay_ == OverlayMode::Off ? OverlayMode::Search : overlay_ == OverlayMode::Search ? OverlayMode::Graph : OverlayMode::Off;
            toast(overlay_ == OverlayMode::Off      ? "Overlay off."
                  : overlay_ == OverlayMode::Search ? "Overlay: nodes expanded by the last A* search."
                                                    : "Overlay: dungeon graph (vertices and edges).",
                  LogKind::Info);
            break;
        case Command::PathToExit: {
            std::string feedback;
            const bool ok = session_.planPathToExit(&feedback);
            walking_ = false;
            toast(ok ? feedback + " Press Enter to walk." : feedback, ok ? LogKind::Success : LogKind::Warning);
            break;
        }
        case Command::Reveal:
            session_.revealAll();
            toast("Map revealed (demo aid).", LogKind::Arcane);
            break;
        default:
            break;
    }
}

void Game::tryMove(Command c) {
    // Taking manual control cancels any plotted route.
    walking_ = false;
    session_.clearPath();
    onMoveResult(session_.move(input::dx(c), input::dy(c)));
}

void Game::onMoveResult(MoveResult r) {
    switch (r) {
        case MoveResult::BattleStarted:
            heldMove_ = Command::None;
            walking_ = false;
            battle_.begin(session_);
            setScreen(Screen::Battle);
            break;
        case MoveResult::ReachedExit:
            heldMove_ = Command::None;
            walking_ = false;
            endMenu_.select(0);
            setScreen(Screen::Victory);
            break;
        case MoveResult::PickedUp:
            toast(session_.messages().back().text, LogKind::Success);
            break;
        case MoveResult::InventoryFull:
            toast(session_.messages().back().text, LogKind::Warning);
            break;
        default:
            break;
    }
}

void Game::inventoryAct(InventoryAction action) {
    const bool inBattle = screen_ == Screen::BattleItems;
    switch (action) {
        case InventoryAction::Close:
            setScreen(inBattle ? Screen::Battle : Screen::Exploring, false);
            break;
        case InventoryAction::Use:
            if (inBattle) {
                // Rejected uses give feedback and keep the turn (FR-B5).
                const ActionResult r = session_.battle().playerAct(BattleAction::UseItem, inventory_.selected());
                if (r.accepted) {
                    battle_.onPlayerActed();
                    setScreen(Screen::Battle, false);
                } else {
                    inventory_.setFeedback(r.feedback, false);
                }
            } else {
                const ItemUseResult r = session_.useItem(inventory_.selected());
                inventory_.setFeedback(r.message, r.success);
                inventory_.clampSelection(session_.inventory());
            }
            break;
        case InventoryAction::Discard: {
            std::string feedback;
            const bool ok = session_.discardItem(inventory_.selected(), &feedback);
            inventory_.setFeedback(feedback, ok);
            inventory_.clampSelection(session_.inventory());
            break;
        }
        case InventoryAction::None:
            break;
    }
}

void Game::battleEvent(BattleUiEvent event) {
    if (event == BattleUiEvent::OpenItems) {
        inventory_.open(InventoryMode::Battle);
        setScreen(Screen::BattleItems, false);
    } else if (event == BattleUiEvent::Finished) {
        leaveBattle();
    }
}

void Game::leaveBattle() {
    const BattlePhase outcome = session_.battle().phase();
    int levels = 0;
    const int xp = session_.finishBattle(&levels);
    if (session_.phase() == SessionPhase::Defeat) {
        endMenu_.select(1);  // "Retry This Dungeon"
        setScreen(Screen::Defeat);
        return;
    }
    setScreen(Screen::Exploring);
    if (outcome == BattlePhase::Won) {
        if (levels > 0) toast("+" + std::to_string(xp) + " XP  ·  Level up! You are now level " + std::to_string(session_.player().level) + ".", LogKind::Arcane);
        else toast("+" + std::to_string(xp) + " XP", LogKind::Success);
    } else if (outcome == BattlePhase::Escaped) {
        toast("You got away.", LogKind::Info);
    }
}

void Game::onMouseMove(sf::Vector2f p) {
    mouse_ = p;
    switch (screen_) {
        case Screen::Title:
            if (!seedEntry_) titleMenu_.handleHover(p);
            break;
        case Screen::Paused: pauseMenu_.handleHover(p); break;
        case Screen::Victory:
        case Screen::Defeat: endMenu_.handleHover(p); break;
        case Screen::Inventory:
        case Screen::BattleItems: inventory_.handleHover(p); break;
        case Screen::Battle: battle_.handleHover(p); break;
        default: break;
    }
}

void Game::onMouseDown(sf::Vector2f p, bool left) {
    switch (screen_) {
        case Screen::Title:
            if (left && !seedEntry_) onMenu(titleMenu_.handleClick(p));
            break;
        case Screen::Exploring: {
            if (!left) {  // right click clears the route
                walking_ = false;
                session_.clearPath();
                break;
            }
            // The pack card in the sidebar opens the inventory.
            if (sf::FloatRect({theme::kSideX, 254.f}, {theme::kSideW, 128.f}).contains(p)) {
                walking_ = false;
                inventory_.open(InventoryMode::Explore);
                setScreen(Screen::Inventory, false);
                break;
            }
            const int tile = tileAtPoint(p);
            if (tile < 0) break;
            if (session_.hasPlannedPath() && tile == session_.pathTarget()) {
                walking_ = true;  // clicking the destination again starts walking
                walkTimer_ = 0.f;
                break;
            }
            std::string feedback;
            walking_ = false;
            const bool ok = session_.planPath(tile, &feedback);
            toast(ok ? feedback + " Press Enter or click again to walk." : feedback, ok ? LogKind::Success : LogKind::Warning);
            break;
        }
        case Screen::Inventory:
        case Screen::BattleItems:
            if (left) inventoryAct(inventory_.handleClick(p, session_.inventory()));
            break;
        case Screen::Battle:
            if (left) battleEvent(battle_.handleClick(p, session_));
            break;
        case Screen::Paused:
            if (left) onMenu(pauseMenu_.handleClick(p));
            break;
        case Screen::Help:
            if (left) screen_ = helpReturn_;
            break;
        case Screen::Victory:
        case Screen::Defeat:
            if (left) onMenu(endMenu_.handleClick(p));
            break;
    }
}

int Game::tileAtPoint(sf::Vector2f p) const {
    const Dungeon& d = session_.dungeon();
    const float fx = (p.x - theme::kMapX) / theme::kTile;
    const float fy = (p.y - theme::kMapY) / theme::kTile;
    if (fx < 0.f || fy < 0.f) return -1;
    const int x = static_cast<int>(fx), y = static_cast<int>(fy);
    return d.inBounds(x, y) ? d.index(x, y) : -1;
}

// ========================================================================== update
void Game::update(float dt) {
    time_ += dt;
    if (fade_ > 0.f) fade_ -= dt * 3.4f;
    if (toast_.ttl > 0.f) toast_.ttl -= dt;

    switch (screen_) {
        case Screen::Exploring:
            playSeconds_ += dt;
            updateExplore(dt);
            break;
        case Screen::Inventory:
            playSeconds_ += dt;
            inventory_.update(dt);
            break;
        case Screen::Battle:
            playSeconds_ += dt;
            battle_.update(dt, session_);
            break;
        case Screen::BattleItems:
            playSeconds_ += dt;
            inventory_.update(dt);
            battle_.update(dt, session_);
            break;
        default:
            break;
    }

    if (!session_.hasRun()) return;

    // Smooth the player marker and the health bar toward their real values.
    const Dungeon& d = session_.dungeon();
    const sf::Vector2f target{static_cast<float>(d.xOf(session_.player().tile)), static_cast<float>(d.yOf(session_.player().tile))};
    const float k = std::min(1.f, dt * 22.f);
    playerVis_ += (target - playerVis_) * k;

    const float hp = static_cast<float>(session_.player().stats.hp);
    hudState_.displayHp += (hp - hudState_.displayHp) * std::min(1.f, dt * 10.f);
    if (hudState_.ghostHp > hudState_.displayHp) {
        hudState_.ghostHp -= dt * 28.f;
        if (hudState_.ghostHp < hudState_.displayHp) hudState_.ghostHp = hudState_.displayHp;
    } else {
        hudState_.ghostHp = hudState_.displayHp;
    }
    hudState_.time = time_;
    hudState_.playSeconds = playSeconds_;
    hudState_.overlay = overlay_;
    hudState_.walking = walking_;
}

void Game::updateExplore(float dt) {
    // Hold a direction to keep walking.
    if (heldMove_ != Command::None) {
        if (!focused_ || !input::isHeld(heldMove_)) {
            heldMove_ = Command::None;
        } else {
            repeatTimer_ -= dt;
            if (repeatTimer_ <= 0.f) {
                repeatTimer_ = kRepeatRate;
                tryMove(heldMove_);
            }
        }
    }
    if (screen_ != Screen::Exploring) return;  // the step above may have started a battle

    // Follow the plotted A* route one tile at a time (FR-A4).
    if (walking_) {
        if (!session_.hasPlannedPath()) {
            walking_ = false;
        } else {
            walkTimer_ -= dt;
            if (walkTimer_ <= 0.f) {
                walkTimer_ = kWalkRate;
                onMoveResult(session_.followPathStep());
                if (!session_.hasPlannedPath()) walking_ = false;
            }
        }
    }
    hoverTile_ = tileAtPoint(mouse_);
}

// ========================================================================== render
void Game::render(sf::RenderTarget& t) {
    switch (screen_) {
        case Screen::Title:
            renderTitle(t);
            break;
        case Screen::Exploring:
            renderExplore(t, true, false);
            break;
        case Screen::Inventory:
            renderExplore(t, false, false);
            inventory_.draw(t, fonts_, session_.inventory(), session_.player());
            break;
        case Screen::Battle:
            battle_.draw(t, fonts_, session_);
            break;
        case Screen::BattleItems:
            battle_.draw(t, fonts_, session_);
            inventory_.draw(t, fonts_, session_.inventory(), session_.player());
            break;
        case Screen::Paused:
            renderExplore(t, false, false);
            screens::drawPause(t, fonts_, pauseMenu_, session_, playSeconds_);
            break;
        case Screen::Help:
            if (helpReturn_ == Screen::Title) renderTitle(t);
            else renderExplore(t, false, false);
            screens::drawHelp(t, fonts_, time_);
            break;
        case Screen::Victory:
        case Screen::Defeat:
            // The whole dungeon is revealed behind the summary.
            renderExplore(t, false, true);
            screens::drawEnd(t, fonts_, endMenu_, session_, screen_ == Screen::Victory, playSeconds_, time_);
            break;
    }
    if (fade_ > 0.f) draw::rect(t, {{0.f, 0.f}, {kW, kH}}, sf::Color(6, 8, 13, static_cast<std::uint8_t>(255.f * std::min(1.f, fade_))));
}

void Game::renderTitle(sf::RenderTarget& t) {
    draw::rect(t, {{0.f, 0.f}, {kW, kH}}, theme::bg);
    MapView v;
    v.tile = 36.4f;  // 37 tiles span the window width
    v.origin = {-3.f, -28.f + 26.f * std::sin(time_ * 0.16f)};
    v.fog = false;
    v.showPlayer = false;
    v.time = time_;
    v.dimAll = 0.9f;
    renderer_.drawMap(t, backdrop_, v);
    screens::drawTitle(t, fonts_, titleMenu_, seedEntry_, seedText_, time_);
}

void Game::renderExplore(sf::RenderTarget& t, bool interactive, bool reveal) {
    hud_.drawBackdrop(t);
    hud_.drawTopBar(t, fonts_, session_, hudState_);
    hud_.drawMapFrame(t, session_);

    MapView v;
    v.origin = {theme::kMapX, theme::kMapY};
    v.tile = theme::kTile;
    v.fog = !reveal;
    v.overlay = overlay_;
    v.hoverTile = interactive ? hoverTile_ : -1;
    v.playerPos = playerVis_;
    v.time = time_;
    renderer_.drawMap(t, session_, v);

    hud_.drawSidebar(t, fonts_, session_, hudState_);
    hud_.drawJournal(t, fonts_, session_);
    if (interactive) {
        renderToast(t);
        renderer_.drawHoverTooltip(t, fonts_, session_, v, mouse_);
    }
}

void Game::renderToast(sf::RenderTarget& t) {
    if (toast_.ttl <= 0.f || toast_.text.empty()) return;
    const float k = toast_.ttl < 0.4f ? toast_.ttl / 0.4f : 1.f;
    const int a = static_cast<int>(255.f * k);
    const sf::FloatRect frame = Hud::mapFrame(session_);
    const float w = draw::textWidth(fonts_.regular, toast_.text, 13) + 50.f;
    const float cx = frame.position.x + frame.size.x / 2.f;
    const float cy = frame.position.y + frame.size.y - 34.f + (1.f - k) * 8.f;
    const sf::FloatRect r{{cx - w / 2.f, cy - 17.f}, {w, 34.f}};
    draw::roundedRect(t, {{r.position.x, r.position.y + 4.f}, r.size}, 17.f, sf::Color(0, 0, 0, static_cast<std::uint8_t>(110.f * k)));
    draw::roundedRect(t, r, 17.f, sf::Color(24, 28, 41, static_cast<std::uint8_t>(246.f * k)), draw::withAlpha(theme::borderHi, a), 1.f);
    draw::circle(t, {r.position.x + 20.f, cy}, 4.f, draw::withAlpha(Hud::logColor(toast_.kind), a));
    draw::text(t, fonts_.regular, toast_.text, 13, {r.position.x + 34.f, cy}, draw::withAlpha(theme::text, a));
}

// ============================================================================ loop
void Game::applyLetterbox(sf::Vector2u windowSize) {
    if (windowSize.x == 0 || windowSize.y == 0) return;
    // Keep the 1340x780 layout's aspect ratio; unused window area stays dark.
    const float sx = static_cast<float>(windowSize.x) / kW;
    const float sy = static_cast<float>(windowSize.y) / kH;
    const float s = std::min(sx, sy);
    const float vw = kW * s / static_cast<float>(windowSize.x);
    const float vh = kH * s / static_cast<float>(windowSize.y);
    view_.setViewport(sf::FloatRect({(1.f - vw) / 2.f, (1.f - vh) / 2.f}, {vw, vh}));
}

int Game::run() {
    // Fit smaller laptop screens by scaling the window, never the layout.
    const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    float scale = std::min({1.f, (static_cast<float>(desktop.size.x) - 40.f) / kW, (static_cast<float>(desktop.size.y) - 110.f) / kH});
    if (scale < 0.5f) scale = 0.5f;
    const sf::Vector2u size{static_cast<unsigned>(kW * scale), static_cast<unsigned>(kH * scale)};

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window_.create(sf::VideoMode(size), "Dungeonbound: The Lost Realms", sf::Style::Default, sf::State::Windowed, settings);
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    applyLetterbox(window_.getSize());

    // Window icon: the exit portal, drawn with the same vector art as the game.
    {
        sf::RenderTexture icon;
        if (icon.resize({64u, 64u})) {
            icon.clear(sf::Color::Transparent);
            draw::portalIcon(icon, {32.f, 32.f}, 2.3f, 0.6f);
            icon.display();
            const sf::Image image = icon.getTexture().copyToImage();
            window_.setIcon(image.getSize(), image.getPixelsPtr());
        }
    }

    sf::Clock clock;
    while (window_.isOpen()) {
        while (const std::optional event = window_.pollEvent()) handleEvent(*event);
        if (!window_.isOpen()) break;

        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f;  // avoid a huge step after a stall or window drag
        update(dt);

        window_.clear(sf::Color(6, 8, 13));
        window_.setView(view_);
        render(window_);
        window_.display();
    }
    return 0;
}

// ===================================================================== screenshots
bool Game::debugWalkTo(int tile) {
    const Dungeon& d = session_.dungeon();
    if (!d.isValidIndex(tile) || session_.phase() != SessionPhase::Exploring) return false;
    DynamicArray<bool> passable(d.tileCount(), true);
    for (const Enemy& e : session_.enemies()) {
        if (e.alive && e.tile != tile) passable[e.tile] = false;
    }
    if (tile != d.exit) passable[d.exit] = false;
    const PathResult route = Pathfinder::findPath(d.graph(), d.width(), session_.player().tile, tile, &passable);
    if (!route.found) return false;
    for (int i = 1; i < route.path.size(); ++i) {
        const int from = route.path[i - 1], to = route.path[i];
        const MoveResult r = session_.move(d.xOf(to) - d.xOf(from), d.yOf(to) - d.yOf(from));
        if (r == MoveResult::BattleStarted || r == MoveResult::ReachedExit) {
            onMoveResult(r);
            return true;
        }
    }
    return true;
}

int Game::captureScreens(const std::string& outDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(outDir, ec);

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    sf::RenderTexture target;
    if (!target.resize({theme::kWindowW, theme::kWindowH}, settings)) return 1;

    int failures = 0;
    auto tick = [this](int frames) {
        for (int i = 0; i < frames; ++i) update(1.f / 60.f);
    };
    auto shot = [&](const char* name) {
        tick(45);
        fade_ = 0.f;
        target.clear(sf::Color(6, 8, 13));
        render(target);
        target.display();
        if (!target.getTexture().copyToImage().saveToFile(fs::path(outDir) / name)) ++failures;
    };
    auto tileCentre = [this](int tile) {
        const Dungeon& d = session_.dungeon();
        return sf::Vector2f{theme::kMapX + (d.xOf(tile) + 0.5f) * theme::kTile, theme::kMapY + (d.yOf(tile) + 0.5f) * theme::kTile};
    };

    // --- Title ---------------------------------------------------------------------
    shot("01-title.png");
    seedEntry_ = true;
    seedText_ = "12345";
    titleMenu_.select(1);
    shot("02-title-seed.png");

    // --- Exploration ---------------------------------------------------------------
    startRun(12345);
    const Dungeon& d = session_.dungeon();
    const int home = d.start;
    for (int i = 0; i < 4 && i < session_.floorItems().size(); ++i) debugWalkTo(session_.floorItems()[i].tile);
    for (int i = 1; i < d.rooms().size() && i < 4; ++i) {
        const Room& room = d.rooms()[i];
        debugWalkTo(d.index(room.x + room.w / 2, room.y + room.h / 2));
    }
    session_.mutablePlayer().stats.hp = 71;
    std::string feedback;
    if (session_.planPath(home, &feedback)) toast(feedback + " Press Enter or click again to walk.", LogKind::Success);
    for (const Enemy& e : session_.enemies()) {
        if (e.alive && session_.isVisible(e.tile)) mouse_ = tileCentre(e.tile);
    }
    shot("03-explore.png");
    mouse_ = {-100.f, -100.f};
    toast_.ttl = 0.f;
    overlay_ = OverlayMode::Search;
    shot("04-overlay-astar.png");
    overlay_ = OverlayMode::Graph;
    shot("05-overlay-graph.png");
    overlay_ = OverlayMode::Off;

    // --- Inventory -----------------------------------------------------------------
    inventory_.open(InventoryMode::Explore);
    setScreen(Screen::Inventory, false);
    shot("06-inventory.png");
    setScreen(Screen::Exploring, false);

    // --- Battle --------------------------------------------------------------------
    for (const Enemy& e : session_.enemies()) {
        if (e.alive && !e.boss && session_.phase() == SessionPhase::Exploring) {
            session_.clearPath();
            debugWalkTo(e.tile);
        }
    }
    if (screen_ == Screen::Battle) {
        tick(90);
        for (int round = 0; round < 2 && !session_.battle().isOver(); ++round) {
            Input attack;
            attack.cmd = Command::Digit;
            attack.digit = round == 0 ? 1 : 2;
            battleEvent(battle_.handleInput(attack, session_));
            tick(110);
        }
        shot("07-battle.png");
        if (!session_.battle().isOver() && !session_.inventory().isEmpty()) {
            inventory_.open(InventoryMode::Battle);
            setScreen(Screen::BattleItems, false);
            shot("08-battle-items.png");
            setScreen(Screen::Battle, false);
        }
        for (int guard = 0; guard < 80 && !session_.battle().isOver(); ++guard) {
            Input attack;
            attack.cmd = Command::Digit;
            attack.digit = 1;
            battleEvent(battle_.handleInput(attack, session_));
            tick(70);
        }
        tick(40);
        shot("09-battle-result.png");
        leaveBattle();
    }

    // --- Overlays and end screens ----------------------------------------------------
    if (screen_ == Screen::Exploring) {
        pauseMenu_.select(0);
        setScreen(Screen::Paused, false);
        shot("10-pause.png");
        setScreen(Screen::Exploring, false);
        openHelp();
        shot("11-help.png");
        screen_ = Screen::Exploring;
    }
    endMenu_.select(0);
    setScreen(Screen::Victory, false);
    shot("12-victory.png");
    endMenu_.select(1);
    setScreen(Screen::Defeat, false);
    shot("13-defeat.png");
    return failures == 0 ? 0 : 1;
}
