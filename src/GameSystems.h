#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

namespace mon {

using EntityId = std::uint32_t;

// 1) Core engine: map/layers/rendering primitives

enum class MapLayerType {
    Ground,
    Decoration,
    Collision,
    Shadow,
    Objects
};

struct TileAnimationDef {
    std::vector<int> frames;
    float fps = 6.0f;
};

struct TileLayerDef {
    MapLayerType type = MapLayerType::Ground;
    int width = 0;
    int height = 0;
    std::vector<int> tiles;
};

struct WorldMapDef {
    std::string mapId;
    std::string sourceFile;
    int tileWidth = 64;
    int tileHeight = 32;
    std::vector<TileLayerDef> layers;
};

struct CameraFollowDef {
    glm::vec2 smoothing{12.0f, 12.0f};
    glm::vec2 deadZone{32.0f, 16.0f};
};

struct MapTransitionDef {
    std::string triggerId;
    std::string targetMapId;
    glm::vec2 targetSpawnTile{0.0f};
};

struct RenderingConfig {
    bool spriteBatching = true;
    bool useTextureAtlas = true;
    bool yDepthSorting = true;
    bool shadowPass = true;
    bool lightingLayer = false;
};

// 2) Character system

struct Stats {
    int hp = 100;
    int mp = 50;
    int strength = 10;
    int dexterity = 10;
    int intelligence = 10;
    int defense = 5;
};

struct LevelProgression {
    int level = 1;
    int xp = 0;
    int xpToNextLevel = 100;
};

enum class AnimationState {
    Idle,
    Walk,
    Attack,
    Cast,
    Hurt,
    Death
};

struct CombatProfile {
    float attackRangeTiles = 1.2f;
    float attackCooldownSeconds = 0.8f;
    float critChance = 0.05f;
    float critMultiplier = 1.5f;
};

struct Entity {
    EntityId id = 0;
    std::string tag;
    glm::vec2 worldPosition{0.0f};
    Stats stats{};
    LevelProgression progression{};
    CombatProfile combat{};
    AnimationState animationState = AnimationState::Idle;
};

struct NpcProfile {
    bool hasShop = false;
    bool isQuestGiver = false;
    std::string dialogueTreeId;
};

struct MonsterProfile {
    float aggroRadiusTiles = 5.0f;
    float respawnSeconds = 25.0f;
    bool canRoam = true;
};

// 3) Combat system

enum class DamageType {
    Physical,
    Magical,
    Fire,
    Ice,
    Poison
};

struct SkillDef {
    std::string id;
    std::string displayName;
    float cooldownSeconds = 1.0f;
    int manaCost = 0;
    float aoeRadiusTiles = 0.0f;
    bool usesProjectile = false;
};

class CombatManager {
public:
    float CalculateDamage(const Entity& attacker,
                          const Entity& defender,
                          DamageType type = DamageType::Physical,
                          float damageMultiplier = 1.0f) const;
    bool CanHit(const Entity& attacker, const Entity& defender) const;
};

// 4) Item & inventory system

enum class ItemType {
    Weapon,
    Armor,
    Consumable,
    Quest
};

struct ItemDefinition {
    std::string id;
    std::string displayName;
    ItemType type = ItemType::Consumable;
    int maxStack = 1;
    int value = 0;
    std::string rarity = "Common";
};

class ItemDatabase {
public:
    bool LoadFromJson(const std::string& path);
    const ItemDefinition* Find(const std::string& id) const;
    const std::unordered_map<std::string, ItemDefinition>& All() const { return mItems; }

private:
    std::unordered_map<std::string, ItemDefinition> mItems;
};

struct InventorySlot {
    std::string itemId;
    int count = 0;
};

struct EquipmentSlots {
    std::optional<std::string> weapon;
    std::optional<std::string> helmet;
    std::optional<std::string> armor;
    std::optional<std::string> boots;
    std::optional<std::string> ring;
};

struct LootEntry {
    std::string itemId;
    float dropChance = 0.0f;
    int minCount = 1;
    int maxCount = 1;
};

struct LootTable {
    std::string monsterId;
    std::vector<LootEntry> entries;
};

// 5) Quest system

enum class QuestState {
    NotStarted,
    Active,
    Complete,
    TurnedIn
};

struct QuestObjective {
    std::string objectiveId;
    int targetCount = 0;
    int progress = 0;
};

struct Quest {
    std::string id;
    std::string title;
    QuestState state = QuestState::NotStarted;
    std::vector<QuestObjective> killObjectives;
    std::vector<QuestObjective> collectObjectives;
    int rewardXp = 0;
    int rewardGold = 0;
};

// 6) AI system

enum class AiState {
    Idle,
    Patrol,
    Roam,
    Chase,
    Attack,
    Flee
};

struct PatrolRoute {
    std::vector<glm::vec2> waypoints;
    bool loop = true;
};

class PathfindingSystem {
public:
    // Stub for future A* integration.
    std::vector<glm::vec2> FindPath(const glm::vec2& start, const glm::vec2& goal) const;
};

// 7) World systems

struct TownFeatures {
    bool hasShops = true;
    bool hasStorage = true;
    bool isSafeZone = true;
    bool hasFastTravel = false;
};

struct DungeonFeatures {
    bool enemySpawnRegions = true;
    bool hasBoss = false;
    bool usesLockedDoors = false;
    bool environmentalHazards = false;
};

// 8) Death & respawn

struct DeathRespawnConfig {
    glm::vec2 respawnTile{0.0f};
    int goldPenaltyPct = 0;
    int xpPenaltyPct = 0;
};

// 9) UI

struct DamageNumber {
    glm::vec2 worldPos{0.0f};
    int amount = 0;
    float lifetime = 0.8f;
};

class UISystem {
public:
    void PushDamageNumber(const DamageNumber& value) { mDamageNumbers.push_back(value); }
    std::vector<DamageNumber>& DamageNumbers() { return mDamageNumbers; }

private:
    std::vector<DamageNumber> mDamageNumbers;
};

// 10) Audio

struct AudioConfig {
    std::string bgmTrack;
    std::string combatSfx;
    std::string footstepSfx;
    std::string uiClickSfx;
    std::string monsterSfx;
};

// 11) Save/load

struct SaveGameData {
    Stats playerStats{};
    glm::vec2 playerTile{0.0f};
    std::vector<InventorySlot> inventory;
    EquipmentSlots equipped;
    std::unordered_map<std::string, QuestState> quests;
    int gold = 0;
};

// 12) Data architecture

enum class GameState {
    Menu,
    InWorld,
    InCombat,
    Dialogue,
    Inventory,
    Dead
};

class StateMachine {
public:
    using StateCallback = std::function<void(GameState)>;

    void SetState(GameState newState);
    GameState GetState() const { return mCurrentState; }
    void OnStateChanged(StateCallback callback) { mCallback = std::move(callback); }

private:
    GameState mCurrentState = GameState::Menu;
    StateCallback mCallback;
};

struct Event {
    std::string name;
    std::unordered_map<std::string, std::string> payload;
};

class EventBus {
public:
    using Listener = std::function<void(const Event&)>;
    void Subscribe(std::string eventName, Listener listener);
    void Emit(const Event& event) const;

private:
    std::unordered_map<std::string, std::vector<Listener>> mListeners;
};

// 13) Advanced systems (future placeholders)

struct StatusEffect {
    std::string id;
    float durationSeconds = 0.0f;
    float tickSeconds = 1.0f;
};

struct CraftingRecipe {
    std::string recipeId;
    std::vector<std::pair<std::string, int>> inputs;
    std::pair<std::string, int> output;
};

} // namespace mon
