#pragma once
// BattleSystem — turn-based combat driven by a binary heap (FR-B1 .. FR-B6).
//
// Every combatant has an initiative value (SPD). Acting costs 100 / SPD time
// units. The turn queue is a min-heap keyed on each combatant's next action
// time, so faster combatants surface more often. Ties go to whoever was
// scheduled first.

#include "../core/BinaryHeap.hpp"
#include "../core/DynamicArray.hpp"
#include "Entity.hpp"
#include "Inventory.hpp"
#include "Rng.hpp"

#include <string>

enum class BattleAction { Attack, Defend, UseItem, Escape };
enum class BattlePhase { Idle, PlayerTurn, EnemyTurn, Won, Lost, Escaped };
enum class Actor { Player = 0, Enemy = 1 };

enum class LogKind { Info, PlayerHit, EnemyHit, Heal, Warning, Success, Danger, Arcane };

struct LogLine {
    std::string text;
    LogKind kind = LogKind::Info;
};

struct TurnEvent {
    double time = 0.0;
    int seq = 0;
    Actor actor = Actor::Player;
};

struct TurnEventCompare {
    bool operator()(const TurnEvent& a, const TurnEvent& b) const {
        if (a.time != b.time) return a.time < b.time;
        return a.seq < b.seq;
    }
};

// Visual effect notifications for the UI (damage numbers, flashes).
struct BattleFx {
    enum class Type { Damage, Heal, Guard, Miss, Buff };
    Type type = Type::Damage;
    Actor target = Actor::Enemy;
    int amount = 0;
    bool critical = false;
};

struct ActionResult {
    bool accepted = false;  // false = rejected, turn NOT consumed (FR-B5)
    std::string feedback;
};

class BattleSystem {
public:
    using TurnQueue = BinaryHeap<TurnEvent, TurnEventCompare>;

    void begin(Player* player, Enemy* enemy, Inventory* inventory, Rng* rng);

    BattlePhase phase() const { return phase_; }
    bool isOver() const { return phase_ == BattlePhase::Won || phase_ == BattlePhase::Lost || phase_ == BattlePhase::Escaped; }
    bool active() const { return phase_ != BattlePhase::Idle; }

    ActionResult playerAct(BattleAction action, int itemIndex = -1);
    void enemyAct();  // call while phase() == EnemyTurn

    // Next `count` turns, predicted from a copy of the heap (current actor first).
    DynamicArray<TurnEvent> upcoming(int count) const;

    bool canEscape() const { return enemy_ && !enemy_->boss; }
    double escapeChance() const;
    void damageRange(const Stats& attacker, const Stats& defender, int& lo, int& hi) const;

    bool playerDefending() const { return playerDefending_; }
    bool enemyDefending() const { return enemyDefending_; }
    int round() const { return turnsTaken_; }
    int damageDealt() const { return damageDealt_; }
    int damageTaken() const { return damageTaken_; }
    int itemsUsed() const { return itemsUsed_; }
    double now() const { return now_; }
    const TurnQueue& queue() const { return queue_; }

    const Player* player() const { return player_; }
    const Enemy* enemy() const { return enemy_; }
    const DynamicArray<LogLine>& log() const { return log_; }

    bool popFx(BattleFx& out);

    static double interval(int speed) { return 100.0 / (speed < 1 ? 1 : speed); }

private:
    void schedule(Actor actor, double atTime);
    void advance();
    int rollDamage(const Stats& attacker, const Stats& defender, bool defending, double multiplier, double critChance, bool& crit);
    void addLog(const std::string& text, LogKind kind);
    void pushFx(BattleFx::Type type, Actor target, int amount, bool crit = false);

    Player* player_ = nullptr;
    Enemy* enemy_ = nullptr;
    Inventory* inventory_ = nullptr;
    Rng* rng_ = nullptr;

    TurnQueue queue_;
    BattlePhase phase_ = BattlePhase::Idle;
    Actor current_ = Actor::Player;
    double now_ = 0.0;
    int seq_ = 0;
    int turnsTaken_ = 0;
    bool playerDefending_ = false;
    bool enemyDefending_ = false;
    int damageDealt_ = 0;
    int damageTaken_ = 0;
    int itemsUsed_ = 0;

    DynamicArray<LogLine> log_;
    DynamicArray<BattleFx> fx_;
};
