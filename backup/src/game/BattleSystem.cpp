#include "BattleSystem.hpp"

#include <cmath>

void BattleSystem::begin(Player* player, Enemy* enemy, Inventory* inventory, Rng* rng) {
    player_ = player;
    enemy_ = enemy;
    inventory_ = inventory;
    rng_ = rng;
    queue_.clear();
    log_.clear();
    fx_.clear();
    now_ = 0.0;
    seq_ = 0;
    turnsTaken_ = 0;
    playerDefending_ = enemyDefending_ = false;
    damageDealt_ = damageTaken_ = itemsUsed_ = 0;

    const EnemyInfo& info = enemyInfo(enemy_->kind);
    addLog(std::string(info.name) + ", " + info.title + ", blocks your way!", enemy_->boss ? LogKind::Danger : LogKind::Warning);

    // Initial schedule: each combatant's first action time comes from its initiative.
    schedule(Actor::Player, interval(player_->stats.speed));
    schedule(Actor::Enemy, interval(enemy_->stats.speed));
    advance();
}

void BattleSystem::schedule(Actor actor, double atTime) { queue_.push({atTime, seq_++, actor}); }

void BattleSystem::advance() {
    if (player_->stats.hp <= 0) {
        phase_ = BattlePhase::Lost;
        addLog("You collapse to the cold stone floor...", LogKind::Danger);
        return;
    }
    if (enemy_->stats.hp <= 0) {
        phase_ = BattlePhase::Won;
        addLog(std::string("The ") + enemy_->name() + " is defeated!", LogKind::Success);
        return;
    }
    TurnEvent next;
    if (!queue_.tryPop(next)) {  // should never happen; fail safe
        schedule(Actor::Player, now_ + interval(player_->stats.speed));
        next = queue_.pop();
    }
    now_ = next.time;
    current_ = next.actor;
    // A guard lasts until that combatant's next turn begins.
    if (current_ == Actor::Player) {
        playerDefending_ = false;
        phase_ = BattlePhase::PlayerTurn;
    } else {
        enemyDefending_ = false;
        phase_ = BattlePhase::EnemyTurn;
    }
}

int BattleSystem::rollDamage(const Stats& attacker, const Stats& defender, bool defending, double multiplier, double critChance, bool& crit) {
    double raw = attacker.attack + rng_->range(-2, 3);
    crit = rng_->chance(critChance);
    raw *= multiplier * (crit ? 1.6 : 1.0);
    double dmg = raw - defender.defense;
    if (defending) dmg *= 0.5;
    int result = static_cast<int>(std::lround(dmg));
    return result < 1 ? 1 : result;
}

void BattleSystem::damageRange(const Stats& attacker, const Stats& defender, int& lo, int& hi) const {
    lo = attacker.attack - 2 - defender.defense;
    hi = attacker.attack + 3 - defender.defense;
    if (lo < 1) lo = 1;
    if (hi < 1) hi = 1;
}

double BattleSystem::escapeChance() const {
    if (!canEscape()) return 0.0;
    double c = 0.5 + (player_->stats.speed - enemy_->stats.speed) * 0.04;
    if (c < 0.2) c = 0.2;
    if (c > 0.9) c = 0.9;
    return c;
}

ActionResult BattleSystem::playerAct(BattleAction action, int itemIndex) {
    ActionResult r;
    if (phase_ != BattlePhase::PlayerTurn) {
        r.feedback = "It is not your turn.";
        return r;
    }

    switch (action) {
        case BattleAction::Attack: {
            bool crit = false;
            int dmg = rollDamage(player_->stats, enemy_->stats, enemyDefending_, 1.0, 0.12, crit);
            enemy_->stats.hp -= dmg;
            if (enemy_->stats.hp < 0) enemy_->stats.hp = 0;
            damageDealt_ += dmg;
            pushFx(BattleFx::Type::Damage, Actor::Enemy, dmg, crit);
            addLog(std::string(crit ? "Critical hit! " : "") + "You strike the " + enemy_->name() + " for " + std::to_string(dmg) + " damage.",
                   LogKind::PlayerHit);
            break;
        }
        case BattleAction::Defend:
            playerDefending_ = true;
            pushFx(BattleFx::Type::Guard, Actor::Player, 0);
            addLog("You raise your guard. Incoming damage is halved.", LogKind::Info);
            break;
        case BattleAction::UseItem: {
            if (!inventory_ || inventory_->isEmpty()) {
                r.feedback = "Your pack is empty. Choose another action.";
                return r;
            }
            if (!inventory_->isValidIndex(itemIndex)) {
                r.feedback = "Select a valid item first.";
                return r;
            }
            ItemType type = inventory_->at(itemIndex).type;
            ItemUseResult use = applyItemEffect(type, *player_, &enemy_->stats, enemy_->boss);
            if (!use.success) {
                r.feedback = use.message;
                return r;
            }
            if (use.consumed) inventory_->remove(itemIndex);
            ++itemsUsed_;
            if (use.healed > 0) pushFx(BattleFx::Type::Heal, Actor::Player, use.healed);
            if (use.damage > 0) {
                damageDealt_ += use.damage;
                pushFx(BattleFx::Type::Damage, Actor::Enemy, use.damage);
            }
            if (use.healed == 0 && use.damage == 0 && !use.escape) pushFx(BattleFx::Type::Buff, Actor::Player, 0);
            addLog(use.message, use.healed > 0 ? LogKind::Heal : LogKind::Arcane);
            if (use.escape) {
                ++turnsTaken_;
                phase_ = BattlePhase::Escaped;
                r.accepted = true;
                return r;
            }
            break;
        }
        case BattleAction::Escape: {
            if (!canEscape()) {
                r.feedback = "The Warden blocks every exit. There is no escape!";
                return r;
            }
            if (rng_->chance(escapeChance())) {
                ++turnsTaken_;
                addLog("You slip away into the shadows.", LogKind::Success);
                phase_ = BattlePhase::Escaped;
                r.accepted = true;
                return r;
            }
            pushFx(BattleFx::Type::Miss, Actor::Player, 0);
            addLog("You try to flee, but the " + std::string(enemy_->name()) + " cuts you off!", LogKind::Warning);
            break;
        }
    }

    ++turnsTaken_;
    r.accepted = true;
    schedule(Actor::Player, now_ + interval(player_->stats.speed));
    advance();
    return r;
}

void BattleSystem::enemyAct() {
    if (phase_ != BattlePhase::EnemyTurn) return;
    const EnemyInfo& info = enemyInfo(enemy_->kind);

    bool lowHealth = enemy_->stats.hp * 100 < enemy_->stats.maxHp * 35;
    if (lowHealth && rng_->chance(0.2)) {
        enemyDefending_ = true;
        pushFx(BattleFx::Type::Guard, Actor::Enemy, 0);
        addLog(std::string("The ") + enemy_->name() + " braces itself.", LogKind::Info);
    } else {
        bool special = rng_->chance(info.specialChance);
        double mult = special ? info.specialMultiplier : 1.0;
        bool crit = false;
        int dmg = rollDamage(enemy_->stats, player_->stats, playerDefending_, mult, 0.06, crit);
        player_->stats.hp -= dmg;
        if (player_->stats.hp < 0) player_->stats.hp = 0;
        damageTaken_ += dmg;
        pushFx(BattleFx::Type::Damage, Actor::Player, dmg, crit || special);
        std::string verb = special ? std::string(" unleashes ") + info.specialName + " for " : " hits you for ";
        addLog(std::string("The ") + enemy_->name() + verb + std::to_string(dmg) + " damage" + (playerDefending_ ? " (guarded)." : "."),
               LogKind::EnemyHit);
    }

    ++turnsTaken_;
    schedule(Actor::Enemy, now_ + interval(enemy_->stats.speed));
    advance();
}

DynamicArray<TurnEvent> BattleSystem::upcoming(int count) const {
    DynamicArray<TurnEvent> result;
    if (!player_ || !enemy_ || count <= 0) return result;
    if (phase_ != BattlePhase::PlayerTurn && phase_ != BattlePhase::EnemyTurn) return result;

    // Simulate on a copy so the real queue is untouched.
    TurnQueue sim = queue_;
    int seq = seq_;
    result.push_back({now_, -1, current_});
    int currentSpeed = current_ == Actor::Player ? player_->stats.speed : enemy_->stats.speed;
    sim.push({now_ + interval(currentSpeed), seq++, current_});

    while (result.size() < count && !sim.empty()) {
        TurnEvent ev = sim.pop();
        result.push_back(ev);
        int speed = ev.actor == Actor::Player ? player_->stats.speed : enemy_->stats.speed;
        sim.push({ev.time + interval(speed), seq++, ev.actor});
    }
    return result;
}

bool BattleSystem::popFx(BattleFx& out) {
    if (fx_.empty()) return false;
    out = fx_[0];
    fx_.removeAt(0);
    return true;
}

void BattleSystem::addLog(const std::string& text, LogKind kind) {
    log_.push_back({text, kind});
    if (log_.size() > 40) log_.removeAt(0);
}

void BattleSystem::pushFx(BattleFx::Type type, Actor target, int amount, bool crit) {
    BattleFx fx;
    fx.type = type;
    fx.target = target;
    fx.amount = amount;
    fx.critical = crit;
    fx_.push_back(fx);
}
