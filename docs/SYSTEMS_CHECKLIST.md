# MONClient Systems Checklist (Scaffold Coverage)

This checklist mirrors the requested systems and tracks whether scaffold coverage exists in `GameSystems`.

## 1. Core Engine Systems
- [x] Tilemap layer models (ground/decoration/collision/shadow/object)
- [x] Tile animation definition
- [x] Map loading metadata and transitions
- [x] Camera follow definition
- [x] Rendering config flags (batching/atlas/depth/shadow/light)

## 2. Character System
- [x] Player/NPC/Monster data models
- [x] Animation states (idle/walk/attack/cast/hurt/death)
- [x] Base stat block and leveling data

## 3. Combat System
- [x] Core hit-range check
- [x] Damage formula with damage types
- [x] Skill definitions (cooldown/mana/AoE/projectile flag)

## 4. Item & Inventory System
- [x] Item base definition + item types + rarity
- [x] Stack sizing and value fields
- [x] Grid-slot-ready inventory slot model
- [x] Equipment slots and loot table structures
- [x] Central item database JSON loading

## 5. Quest System
- [x] Quest states and objective trackers
- [x] Reward fields (XP/gold)

## 6. AI System
- [x] AI state model (idle/patrol/roam/chase/attack/flee)
- [x] Patrol route model
- [x] Pathfinding system placeholder (`FindPath`, future A*)

## 7. World Systems
- [x] Town feature flags
- [x] Dungeon feature flags

## 8. Death & Respawn
- [x] Respawn + penalty config model

## 9. UI Systems
- [x] Damage number UI queue model

## 10. Audio System
- [x] Audio cue/track references

## 11. Save / Load System
- [x] Save-game data model (stats, position, inventory, quests, equipment)

## 12. Data Architecture
- [x] Entity base model
- [x] Event bus
- [x] Game state machine

## 13. Advanced Systems (future hooks)
- [x] Status effect model
- [x] Crafting recipe model

## Remaining implementation work
This patch provides broad schema + manager scaffolding only. Runtime integration into scene update loops, OpenGL passes, input flow, and persistence I/O still needs to be implemented.
