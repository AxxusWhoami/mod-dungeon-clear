# mod-dungeon-clear

Autonomous dungeon-clearing mode for **mod-playerbots** tank bots, packaged as a drop-in AzerothCore module. A tank bot drives the party from boss to boss, clearing trash, navigating the layout, pausing for loot, resting between fights, and handling doors and scripted events along the way. You deal damage and let the tank run the dungeon.

Routes are generated at runtime from the live navigation mesh. There are no waypoint files or hardcoded paths, so the same system can work across supported instances. The module runs against a **stock, unmodified mod-playerbots** checkout and does not require edits to playerbots source files.

> ## Companion addon
>
> [**mod-dungeon-clear-addon**](https://github.com/jrad7/mod-dungeon-clear-addon) is the recommended control surface: it provides a movable panel with On, Off, Skip, and Pause buttons, live status, a per-boss list, and live settings. The `dc` chat keywords and `.dc` commands remain available.

## Features

- **Runtime navigation** from boss to boss over the live navmesh, including long corridors, doors, multi-wing maps, and selected submerged routes.
- **Trash pulling** with Dynamic, Leeroy, and Advanced pull modes.
- **Dungeon events** for levers, altars, gongs, freed prisoners, escorts, item-on-object interactions, and other encounter gates.
- **Loot handling** with a configurable quality floor, quest-item priority, and corpse skip logic.
- **Rest management** based on party health and mana, including optional Smart Rest hysteresis.
- **Party cohesion**: followers stay with the tank, healers reposition for line of sight, and the party regroups after combat separates it.
- **Hazard avoidance** for registered persistent damage areas and active vacate situations.
- **Death recovery**: with PostCombatRez enabled, the run can hold while a living Priest, Paladin, Shaman, or Druid resurrects a fallen member. A full wipe, missing resurrector, or timeout still ends the run.
- **Stranded-member recovery**: a bot stuck out of range after a long period without progress can be returned to the tank.
- **GM-only automated runs** through `.dc test`, including repeatable seeds, concurrent plans, spectator watching, diagnostics, and JSONL results.

## Requirements

- AzerothCore with **mod-playerbots** installed and enabled.
- A playerbots build compatible with the core revision used by the server.
- A configured worldserver and a dungeon map/navmesh installation.

This module is a playerbots extension, not a standalone AI or standalone server module. It subclasses playerbots strategy, action, trigger, and value classes and links into the playerbots engine.

## Installation

1. Clone or copy this repository into `modules/mod-dungeon-clear/`.
2. Keep `modules/mod-playerbots/` enabled in the same AzerothCore checkout.
3. Re-run CMake and rebuild the worldserver with static modules enabled:

   ```text
   -DMODULES=static
   ```

4. Optionally copy `conf/mod_dungeon_clear.conf.dist` to the server's active configuration directory as `mod_dungeon_clear.conf`.
5. Restart the worldserver after installing the module or changing compiled code.

The CMake registration automatically collects sources below `src/` and installs the distributed configuration file. When adding or removing source files, re-run CMake so the source list is refreshed.

## Quick start

The tank must be a bot. Join a dungeon with a bot tank, then use one of these controls from a real player in that bot's group:

```text
.dc on
.dc status
.dc off
```

`.dc on` requires the player to be inside a dungeon. Non-tank party bots follow the tank only while dungeon clear is active and return to the player's normal control afterward.

You can use the equivalent in-party chat keywords, such as `dc on` or `dungeon clear on`.

## Commands

| Command | Chat keyword | Description |
|---|---|---|
| `.dc on` | `dc on` / `dungeon clear on` | Start the clear. |
| `.dc off` | `dc off` / `dungeon clear off` | Stop and return bots to the player. |
| `.dc pause` | `dc pause` / `dungeon clear pause` | Soft-stop in place; use again to resume. |
| `.dc skip` | `dc skip` | Skip the current objective when the tank is stalled. |
| `.dc pull` | `dc pull` / `dungeon clear pull` | Cycle the pull mode. |
| `.dc status` | `dc status` | Print the current run status. |
| `.dc bosses` | `dc bosses` | List bosses and their kill state. |
| `.dc go <boss>` | — | Route directly to a named boss. |
| `.dc spectate` | — | Toggle the free-fly spectator camera. |

Commands must come from a real player in the bot's group. The same commands are available from the worldserver console where supported; console-launched test runs use the test driver described below.

## Pull modes

| Mode | Behavior | Speed | Risk |
|---|---|---:|---:|
| **Dynamic** | Chooses Leeroy or a camp pull per pack based on the estimated danger. | Medium | Medium |
| **Leeroy** | Walks directly into each pack and fights in place. | Fastest | Highest |
| **Advanced** | Pulls each pack back to a held camp before fighting. | Slowest | Lowest |

Dynamic is recommended for most content. Its ceiling is controlled by `DungeonClear.PullDynamicMaxLeeroyMobs`; use Leeroy for content you out-gear and Advanced for difficult content where every pull matters.

See the [Pull modes wiki page](https://github.com/jrad7/mod-dungeon-clear/wiki/Pull-Modes) for detailed tuning guidance.

## Dungeon events and supported content

Many instances require a scripted action before the next boss becomes reachable: opening a gate, activating an altar, ringing a gong, freeing a prisoner, escorting an NPC, or crossing an off-mesh gap. The module executes registered events as part of the normal route and pauses or skips a step when the required game state cannot be completed.

Event definitions are maintained per dungeon under `src/Ai/Dungeon/DungeonClear/Data/Events/`. Current coverage includes Deadmines, Shadowfang Keep, Wailing Caverns, Uldaman, Sunken Temple, Razorfen Downs, Scarlet Monastery, Zul'Farrak, Blackrock Depths, Scholomance, Stratholme, Dire Maul, Arcatraz, Sethekk Halls, Old Hillsbrad, and Black Morass. Faction-specific events run only for the relevant faction, and coverage continues to expand.

The full per-dungeon list is documented on the [Scripted Dungeon Events wiki page](https://github.com/jrad7/mod-dungeon-clear/wiki/Scripted-Dungeon-Events).

## Configuration

All `DungeonClear.*` options are documented in `conf/mod_dungeon_clear.conf.dist`. The file contains defaults, valid ranges, reload behavior, heroic overrides, diagnostic options, and test-run settings.

Common settings include:

- `DungeonClear.LootMinQuality` — minimum item quality worth looting.
- `DungeonClear.RestHealthPct` and `DungeonClear.RestManaPct` — legacy rest targets.
- `DungeonClear.SmartRest` and its role-based thresholds — fewer, longer full-rest cycles.
- `DungeonClear.PostCombatRez` — recover from individual deaths instead of stopping immediately.
- `DungeonClear.StrandedRecovery` — return stuck bot members to the tank after prolonged no-progress.
- `DungeonClear.PullDynamicMaxLeeroyMobs` — Dynamic mode's maximum estimated Leeroy pull size.
- `DungeonClear.RecordDecisions` — capture decision records for offline replay and regression analysis.
- `DungeonClear.TestRun.*` — automated run timeouts, driver identity, and output behavior.

The companion addon can override general settings for the duration of one group's run. The server validates and clamps those values; debugging and advanced pathing settings remain server-only. Heroic runs can use a separate `<setting>.Heroic` value.

A few mod-playerbots settings also affect movement and interaction, especially `LootDistance`, `ReactDistance`, `SightDistance`, and `FollowDistance`.

See the [Configuration wiki page](https://github.com/jrad7/mod-dungeon-clear/wiki/Configuration) for the broader reference.

## How the module is organized

The module is split into engine-facing AI code and small, testable decision units:

- `src/Ai/Dungeon/DungeonClear/Action/` — navigation, engagement, pulling, and follower actions.
- `src/Ai/Dungeon/DungeonClear/Trigger/` and `Value/` — conditions and data exposed to the playerbots strategy engine.
- `src/Ai/Dungeon/DungeonClear/Strategy/` — strategy priorities and relevance ordering.
- `src/Ai/Dungeon/DungeonClear/Data/` — dungeon rosters, events, hazards, doors, routes, and other declarative registries.
- `src/Ai/Dungeon/DungeonClear/Util/` — navigation, geometry, party state, loot, rest, recovery, and pure decision helpers.
- `src/Ai/Dungeon/DungeonClear/TestRun/` — GM-only automated run provisioning, monitoring, diagnostics, and result records.
- `src/DcStrategyGate.*` — installs the dungeon-clear strategy only for bots on dungeon or raid maps.
- `src/DungeonClearCommand.cpp` — `.dc` command registration and dispatch.
- `src/DungeonClearAddonHook.cpp` — addon status and settings communication.
- `t/` — the native test suite, replay fixtures, and optional navigation harness.
- `tools/` — deterministic checks, test-run inspection, map-data utilities, and audits.

The module appends its context factories to the shared playerbots registries during the first world tick, registers the `.dc` command, and installs the dungeon-clear strategy through login/map-change hooks. It does not modify playerbots source files.

See [How it integrates](https://github.com/jrad7/mod-dungeon-clear/wiki/How-It-Integrates) for the integration details.

## Automated test runs (`.dc test`)

The test harness is **GM-only** and is separate from normal dungeon play. It can create a randomized five-bot party with one tank, one healer, and three distinct DPS classes, provision it, send it to a supported dungeon, and record the result. Each run records its seed so a failure can be reproduced with the same party composition.

| Command | Description |
|---|---|
| `.dc test list` | List supported dungeons and default levels. |
| `.dc test start <dungeon> [level=N] [seed=N]` | Start one automated run. The dungeon can be a list token or map ID. |
| `.dc test status` | Show live progress for each run. |
| `.dc test stop [runId\|dungeon\|all]` | Abort selected runs. |
| `.dc test watch [runId\|dungeon]` | Follow a live run with the spectator camera; use `off` to return. |
| `.dc test watch next` | Hop to the next live run. |
| `.dc test plan start <dungeon> total=N [concurrent=N]` | Run a dungeon repeatedly with a concurrency limit. |
| `.dc test plan status` | Show live plan progress. |
| `.dc test plan stop [planId\|all]` | Stop a plan and its runs. |

The commands also work from the worldserver console and AC Command Deck. Since those contexts have no player to anchor a run, the module creates a dedicated driver account and character on first use. `DungeonClear.TestRun.DriverAccount` and `DungeonClear.TestRun.DriverCharacter` control their names; setting the account to an empty value disables automatic creation.

A run succeeds when every boss is down. It fails on a party wipe, prolonged stall, missing progress, or the configured time cap. Results are written to `dc_testruns.jsonl` next to the worldserver, with run log lines bracketed by `TESTRUN START` and `TESTRUN END` markers. Wing-split dungeons run one wing at a time, such as `dm-east` and `sm-cath`.

### Local test suite

From the module root, run:

```text
sudo bash t/run_tests.sh
```

The suite covers pure decision kernels, dungeon registries, event builders, pull behavior, recovery rules, replay fixtures, and other engine-independent logic. Navigation tests that require private map-data slices are skipped when those fixtures are unavailable.

For a recorded test run, `tools/dc_test_run.py <run-id>` gathers the run record, diagnostic snapshot, live state, and correlated log lines. Use its `--list`, `--grep`, `--logs`, `--level`, `--dump`, or `--json` options to narrow the report. A worldserver restart can remove older log files; the JSONL result and diagnostic snapshot remain the durable record.

## Important limitation

Dungeon clear drives the **tank bot's AI**. Do not personally control the tank while dungeon clear is active, because the player and the AI would compete for the same character. The exception is self-bot mode: turn your own character into a self-bot with `.playerbots bot self` and let the AI drive it. If you want direct keyboard control, play a follower instead.

## License

AGPL-3.0-or-later, inherited from mod-playerbots. See [LICENSE](LICENSE).
