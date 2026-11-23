# Dungeonbound: The Lost Realms – Product Requirements Document

Prepared by Isbah Rafay (25K-0664) and Maheen Kamran (25K-0598) · BCS-3F · FAST-NUCES Karachi · Submitted to Miss Fatima Gado · Version 1.0, derived from the project proposal dated 18 October 2026

## 1. Overview

**Product summary.** Dungeonbound: The Lost Realms is a 2D dungeon exploration, inventory management and turn-based battle game built in C++ with the SFML library. The player moves through a procedurally generated dungeon, collects items, fights enemies in turn-based combat and tries to reach the dungeon's objective or exit.

**Dual purpose.** The product is both a playable game and a demonstration of core Data Structures and Algorithms (DSA). Every central system must visibly use a student-implemented data structure or algorithm, with no STL containers standing in for the core structures.

**Context.** This is a two-person coursework project for the DSA lab. Scope is deliberately limited: a small, reliable, well-tested game is preferred over an ambitious, unfinished one.

## 2. Goals and Non-Goals

### Goals

- Deliver a complete, playable game loop: explore, collect, fight, reach the exit (win) or die (lose).
- Showcase four core DSA components: graph (adjacency list), recursive backtracking, A\* pathfinding and a binary heap.
- Keep game logic separate from SFML rendering so algorithms can be tested independently.
- Provide a README and test cases that make the project easy to run, grade and understand.
- Offer a different dungeon layout on every run.

### Non-Goals

- Reproducing a commercial game or its depth of content.
- Using an external game engine.
- Complex animation, networking, multiplayer or save-to-cloud features.
- Optional extensions (hash table, BST, undo stack) before the core is stable and tested.

## 3. Users and Stakeholders

| Stakeholder | Interest |
| --- | --- |
| Player (classmates, instructor playing the demo) | A clear, fun, bug-free game that is understandable without instructions beyond the README |
| Course instructor (Miss Fatima Gado) | Evidence of correct, custom-built DSA implementations, clean structure and testing |
| Development team (Isbah, Maheen) | A scope achievable by two people, with clean module boundaries for parallel work |

## 4. Scope

### 4.1 Minimum Complete Version (must have)

1. Procedurally generated 2D dungeon using recursion and backtracking.
2. Graph representation of rooms or walkable tiles, using a team-written adjacency list.
3. Player movement, exploration, collision/blocking checks and a clear objective or exit.
4. A\* pathfinding to a reachable destination.
5. Turn-based combat with player and enemy health, damage and a defined battle flow.
6. Custom binary heap managing combat initiative or battle event priority.
7. Inventory: collect, inspect, use and discard items.
8. SFML interface: dungeon view, status information, inventory panel and battle screen.
9. README (setup, controls, compilation, structure, implemented DSA).
10. Test cases for core data structures, algorithms and gameplay edge cases.

### 4.2 Optional Extensions (only after the core is integrated and stable)

- Custom hash table for fast item lookup by ID or name.
- Binary search tree (BST) for organizing or searching items by value.
- Stack-based undo/action history for a limited set of reversible actions.
- Extra item types, equipment, dungeon themes or visual polish.

The member who implements an extension is responsible for documenting and testing it.

## 5. Functional Requirements

Priority key: **P0** = required for the minimum complete version; **P1** = optional extension.

### 5.1 Dungeon Representation (Graph)

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-G1 | Rooms or walkable locations are vertices; valid connections are edges, stored in a team-implemented adjacency list. | P0 |
| FR-G2 | The graph supports adding vertices and edges, checking whether two locations are connected and retrieving a location's neighbors. | P0 |
| FR-G3 | The graph exposes enough information to support route finding and connectivity checks. | P0 |

### 5.2 Procedural Generation (Recursion and Backtracking)

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-P1 | A recursive generator explores neighboring locations, marks visited ones and backtracks when a path cannot be extended. | P0 |
| FR-P2 | Every generated dungeon is fully connected and the objective or exit is reachable from the player's start. | P0 |
| FR-P3 | Generation rules prevent invalid overlaps and out-of-bounds placement. | P0 |
| FR-P4 | Items and enemies are placed on valid, reachable locations. | P0 |
| FR-P5 | Generation can be seeded so a layout can be reproduced for testing and debugging. | P0 (recommended) |

### 5.3 Movement and Exploration

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-M1 | The player moves via keyboard input; walls and blocked tiles prevent movement. | P0 |
| FR-M2 | Stepping onto an item tile lets the player pick it up; stepping onto an enemy tile starts a battle. | P0 |
| FR-M3 | Reaching the objective or exit ends the run with a win state. | P0 |
| FR-M4 | The player can select a destination, and the game computes and displays a path to it (see 5.4). | P0 |

### 5.4 Route Finding (A\*)

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-A1 | A\* finds a path from the player's current location to a selected destination through walkable tiles, using path costs and a Manhattan-distance heuristic on grids. | P0 |
| FR-A2 | When no route exists, the algorithm returns a clear "no path" result and the game responds gracefully. | P0 |
| FR-A3 | The open set uses the team's custom binary heap where practical. If time is short, a simplified implementation is acceptable provided the playable loop is preserved. | P0 |
| FR-A4 | The computed route is shown visually and may be followed step by step. | P0 |

### 5.5 Turn-Based Combat (Binary Heap)

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-B1 | Each combatant has health, attack/damage values and an initiative value used as heap priority. | P0 |
| FR-B2 | A student-implemented binary heap retrieves the next acting combatant or event. | P0 |
| FR-B3 | Available actions include attack, defend, use item and escape (where appropriate). | P0 |
| FR-B4 | Battle ends when the enemy is defeated, the player is defeated or the player escapes. | P0 |
| FR-B5 | Invalid actions (for example, using an item with an empty inventory) are rejected with clear feedback and do not consume the turn unexpectedly. | P0 |
| FR-B6 | Player death produces a lose state with an option to restart. | P0 |

### 5.6 Inventory

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-I1 | The player can collect, view, inspect, use and discard items. | P0 |
| FR-I2 | Item effects (for example, healing) are applied correctly and the item is consumed when appropriate. | P0 |
| FR-I3 | Inventory handles edge cases: empty inventory, invalid selection, capacity limit if one is defined. | P0 |
| FR-I4 | Hash table for fast item lookup by identifier or name. | P1 |
| FR-I5 | BST to display or search items ordered by value. | P1 |
| FR-I6 | Stack-based undo/history for a limited set of reversible actions. | P1 |

### 5.7 Graphics, UI and Controls

| ID | Requirement | Priority |
| --- | --- | --- |
| FR-U1 | SFML renders the dungeon, player, items, enemies and a health/status HUD. | P0 |
| FR-U2 | Separate inventory panel and battle interface are available. | P0 |
| FR-U3 | Keyboard controls handle movement and menu navigation. | P0 |
| FR-U4 | Mouse controls for buttons, inventory selection and path destination. | P1 |
| FR-U5 | The interface prioritizes clarity over animation; placeholder shapes are acceptable in place of sprites. | P0 |

## 6. Non-Functional Requirements

- **Performance.** Dungeon generation and A\* must complete fast enough that the player perceives no delay at the chosen map size; the game should hold a smooth frame rate on a typical student laptop.
- **Reliability.** No crashes or infinite loops on any generated dungeon; the game must handle invalid input and edge cases gracefully.
- **Memory safety.** Custom data structures must manage memory correctly: no leaks, no dangling pointers, proper destructors and copy handling.
- **Maintainability.** Logic and rendering are separate; modules have clear interfaces; code is commented where algorithms are non-obvious.
- **Portability.** Windows 10/11 is the primary target; code should avoid platform-specific calls so it can build elsewhere with SFML configured.
- **Constraint.** Core structures must not be replaced by `std::list`, `std::map`, `std::stack`, `std::queue` or `std::priority_queue`. Other standard utilities (strings, I/O, random numbers) are allowed where they do not replace the central DSA work, subject to course rules.

## 7. DSA Mapping

| Game Feature | Data Structure / Algorithm | Why it fits |
| --- | --- | --- |
| Dungeon layout | Recursion and backtracking | Naturally explores, commits and undoes choices while carving a connected layout |
| Rooms and connections | Graph (adjacency list) | Sparse connectivity between locations; fast neighbor lookup |
| Click-to-move or hint route | A\* with Manhattan heuristic | Efficient shortest-path search on weighted grids |
| A\* open set and turn order | Binary heap (min or max priority) | Fast retrieval of the next lowest-cost node or highest-initiative combatant |
| Item lookup (optional) | Hash table | Average constant-time lookup by ID or name |
| Items by value (optional) | Binary search tree | Ordered display and range or search queries |
| Undo history (optional) | Stack | Last-in, first-out reversal of recent actions |

## 8. Architecture and Module Plan

The codebase is split so algorithms compile and run without SFML.

- **Core DSA layer (no SFML dependency):** `Graph`, `BinaryHeap`, `DynamicArray` or `LinkedList` helper structures as needed, plus optional `HashTable`, `BST` and `Stack`.
- **Game logic layer:** `DungeonGenerator`, `Pathfinder` (A\*), `Player`, `Enemy`, `Item`, `Inventory`, `BattleSystem`, `GameState`.
- **Presentation layer (SFML):** `Renderer`, `InputHandler`, `HUD`, `InventoryPanel`, `BattleScreen`, `Game` (main loop and state machine).
- **Tests:** a lightweight test harness or standalone test executable covering the core and logic layers.

**Game states:** Main menu → Exploration → Battle → Inventory → Game over / Victory. A simple state machine controls transitions.

Suggested repository layout: `/src/core`, `/src/game`, `/src/ui`, `/tests`, `/assets`, `README.md`.

## 9. Controls (proposed)

| Input | Action |
| --- | --- |
| Arrow keys or WASD | Move the player |
| I | Open or close inventory |
| Up/Down and Enter | Navigate and confirm menu or battle selections |
| Esc | Back, close panel or pause |
| Mouse click (optional) | Select a destination for A\* or press UI buttons |

Final bindings will be confirmed during implementation and listed in the README.

## 10. Testing and Acceptance Criteria

### 10.1 Test Plan

| Area | Cases to test |
| --- | --- |
| Graph | Add vertices and edges, neighbor retrieval, connectivity, duplicate and invalid edges |
| Generation | Every generated map is connected; start and exit are reachable; no overlaps or out-of-bounds; repeatable with a fixed seed; run across many seeds |
| A\* | Path exists; no path exists; start equals goal; blocked destination; path is optimal on known grids |
| Binary heap | Insert and extract ordering, ties, empty-heap extract, large input sequences |
| Inventory | Collect, use, discard, empty inventory, invalid index, capacity limit |
| Combat | Damage calculation, defend effect, escape, player death, enemy death, insufficient health, turn order by initiative |
| Movement | Wall collision, map boundary, item pickup, enemy trigger, reaching the exit |

### 10.2 Definition of Done (minimum complete version)

- A player can start the game, explore a generated dungeon, collect items, win and lose battles, and reach the exit, all without crashes.
- Generated dungeons are connected in every tested seed.
- A\* returns a correct route or a clear "no path" result.
- Combat turn order is driven by the custom binary heap.
- No required structure uses a banned STL container.
- All core tests pass; the README is complete and the project builds from a clean checkout by following it.

## 11. Deliverables

- Complete C++ source code in a shared Git repository, with meaningful commit history from both members.
- Running SFML application.
- README: setup, controls, compilation, project structure and implemented DSA.
- Test cases or test harness for core structures, algorithms and edge cases.
- Project report, as required by the course.

## 12. Delivery Plan

Exact dates should be set against the lab's deadlines. Work is organised in phases, with each phase ending in something runnable.

| Phase | Focus | Exit criteria |
| --- | --- | --- |
| 1. Setup | SFML configured on both machines, repository, folder structure, build instructions | Blank SFML window builds and runs for both members |
| 2. Core DSA | Graph, binary heap, supporting containers, with unit tests | Structures pass tests independent of SFML |
| 3. Generation and pathfinding | Recursive backtracking generator, A\* | Tested across many seeds; routes and no-path cases verified |
| 4. Gameplay | Player, movement, items, inventory, enemies, combat with heap turn order | Full text or debug-mode game loop works |
| 5. UI integration | Dungeon view, HUD, inventory panel, battle screen, menus | Playable end to end in SFML |
| 6. Stabilise | Bug fixing, edge-case tests, README, report | Definition of Done met |
| 7. Optional | Hash table, BST, undo stack, polish | Only started after Phase 6; each extension documented and tested |

### Suggested Work Split

The proposal does not assign roles, so this is a suggestion for the team to adjust.

- **Member A:** graph, dungeon generation, A\*, movement and rendering of the dungeon.
- **Member B:** binary heap, combat system, inventory, battle and inventory UI.
- **Shared:** project setup, integration, testing, README and report.

Work on separate branches and merge through pull requests, integrating at the end of each phase.

## 13. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| SFML setup problems (compiler or library mismatch) | Blocks all graphical work | Do setup first; document exact versions and project settings; both members verify a hello-world build |
| Generator produces disconnected or unwinnable maps | Broken gameplay | Validate connectivity with the graph after generation; regenerate on failure; test with many seeds |
| A\* complexity | Schedule slip | Build a simple version first; the proposal allows simplification as long as the core loop works |
| Scope creep from optional features | Core left unfinished | Optional features are gated until the Definition of Done is met |
| Merge conflicts and integration bugs | Lost time | Clear module interfaces, frequent small commits, branch-per-feature, scheduled integration |
| Memory errors in custom structures | Crashes, grading issues | Rule-of-three/five discipline, testing with sanitizers or Visual Studio diagnostics |
| Art and asset time sink | Delays logic work | Use simple shapes and colored tiles first; sprites only if time permits |

## 14. Assumptions and Open Questions

**Assumptions**

- Grid-based (tile) dungeon rather than free-form rooms; this keeps A\* with a Manhattan heuristic straightforward.
- Single-player, single-session play with no persistent save file.
- Static enemy placement with battle triggered on contact.
- Windows with Visual Studio or VS Code is the build environment.

**Open questions to settle early**

1. Does the course allow a plain dynamic array (a custom or `std::vector`-style wrapper) for helper storage, or must everything be built from custom structures? The proposal says standard utilities are allowed "subject to course rules."
2. Is a single dungeon level enough for the minimum version, or should there be multiple floors?
3. What are the fixed map dimensions and the enemy/item counts per run?
4. Will the instructor expect a visible demonstration of each DSA (for example, a debug overlay showing the A\* path or heap order)?
5. What is the exact format and deadline for the project report and demo?
