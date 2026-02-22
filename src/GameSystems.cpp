#include "GameSystems.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

namespace mon {
namespace {

ItemType ParseType(const std::string& text)
{
    if (text == "Weapon") return ItemType::Weapon;
    if (text == "Armor") return ItemType::Armor;
    if (text == "Quest") return ItemType::Quest;
    return ItemType::Consumable;
}

std::optional<std::string> CaptureString(const std::string& source, const std::string& key)
{
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1) {
        return match[1].str();
    }
    return std::nullopt;
}

std::optional<int> CaptureInt(const std::string& source, const std::string& key)
{
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1) {
        return std::stoi(match[1].str());
    }
    return std::nullopt;
}

} // namespace

bool ItemDatabase::LoadFromJson(const std::string& path)
{
    mItems.clear();

    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string json = buffer.str();

    const std::regex objectPattern("\\{[^\\{\\}]*\\}");
    auto begin = std::sregex_iterator(json.begin(), json.end(), objectPattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        const std::string itemObj = it->str();

        auto id = CaptureString(itemObj, "id");
        auto name = CaptureString(itemObj, "displayName");
        if (!id || !name) {
            continue;
        }

        ItemDefinition def;
        def.id = *id;
        def.displayName = *name;
        def.type = ParseType(CaptureString(itemObj, "type").value_or("Consumable"));
        def.maxStack = CaptureInt(itemObj, "maxStack").value_or(1);
        def.value = CaptureInt(itemObj, "value").value_or(0);
        def.rarity = CaptureString(itemObj, "rarity").value_or("Common");

        mItems[def.id] = def;
    }

    return !mItems.empty();
}

const ItemDefinition* ItemDatabase::Find(const std::string& id) const
{
    auto it = mItems.find(id);
    if (it == mItems.end()) {
        return nullptr;
    }
    return &it->second;
}

float CombatManager::CalculateDamage(const Entity& attacker,
                                     const Entity& defender,
                                     DamageType type,
                                     float damageMultiplier) const
{
    float attackPower = static_cast<float>(attacker.stats.strength * 2 + attacker.stats.dexterity);
    if (type != DamageType::Physical) {
        attackPower = static_cast<float>(attacker.stats.intelligence * 2);
    }

    const float defensePower = static_cast<float>(defender.stats.defense);
    const float reduced = std::max(1.0f, attackPower - defensePower * 0.75f);
    return reduced * std::max(0.0f, damageMultiplier);
}

bool CombatManager::CanHit(const Entity& attacker, const Entity& defender) const
{
    const float dx = attacker.worldPosition.x - defender.worldPosition.x;
    const float dy = attacker.worldPosition.y - defender.worldPosition.y;
    const float distSq = dx * dx + dy * dy;
    return distSq <= attacker.combat.attackRangeTiles * attacker.combat.attackRangeTiles;
}

std::vector<glm::vec2> PathfindingSystem::FindPath(const glm::vec2& start, const glm::vec2& goal) const
{
    // Placeholder direct path for now; replace with A* node expansion later.
    return { start, goal };
}

void StateMachine::SetState(GameState newState)
{
    if (newState == mCurrentState) {
        return;
    }

    mCurrentState = newState;
    if (mCallback) {
        mCallback(newState);
    }
}

void EventBus::Subscribe(std::string eventName, Listener listener)
{
    mListeners[std::move(eventName)].push_back(std::move(listener));
}

void EventBus::Emit(const Event& event) const
{
    auto it = mListeners.find(event.name);
    if (it == mListeners.end()) {
        return;
    }

    for (const auto& listener : it->second) {
        listener(event);
    }
}

} // namespace mon
