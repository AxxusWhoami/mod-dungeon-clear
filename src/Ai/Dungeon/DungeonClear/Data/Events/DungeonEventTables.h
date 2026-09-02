/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_DUNGEONEVENTTABLES_H
#define _PLAYERBOT_DUNGEONEVENTTABLES_H

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "Ai/Dungeon/DungeonClear/Data/DungeonEventRegistry.h"

class Player;
class Creature;
class AiObjectContext;
struct BossRosterPatch;
struct DungeonWingLayout;

// Internal registration seam for the per-dungeon event tables.
//
// Each dungeon owns one .cpp in this folder that defines its event rows
// (Register<Dungeon>Events). A Conditional event's activation predicate is a
// free function defined in the SAME file, handed to the builder by pointer
// (.Conditional(&MyPredicate)) — there is no separate condition registry and no
// global id space to keep collision-free. The central DungeonEventRegistry calls
// the Register<Dungeon>Events aggregators EXPLICITLY so every per-dungeon
// translation unit stays referenced.
//
// Why explicit calls and not self-registering static initializers: the module
// compiles into a static lib, and a TU whose only output is constructor
// side-effects (with no symbol the program references) is dropped by the linker
// — its events would silently vanish. The one-big-table this replaces avoided
// that by keeping everything in a single referenced TU; the aggregator calls
// below restore the reference chain per file. (Same reason ObjectiveHookRegistry
// and friends use hardcoded tables.)
//
// Adding a dungeon:
//   1. Create <Dungeon>Events.cpp here.
//   2. Define Register<Dungeon>Events; for any Conditional event, define its
//      predicate as a static free function in that file and pass &Predicate to
//      .Conditional() (a typo is a compile error, not a silent never-fire).
//   3. Declare the appender below.
//   4. Add the call in EventTable() (DungeonEventRegistry.cpp).
//   5. If the dungeon corrects the auto-derived boss list, define its roster
//      patch as Register<Dungeon>Roster in the SAME file (using the DcRoster
//      builders in DungeonRosterBuilders.h), declare it below, and add the call
//      in PatchTable() (BossRosterRegistry.cpp). One file owns all of a
//      dungeon's clear data: event rows + conditions + roster patch.

// Shared, cross-dungeon activation predicate — external linkage so several
// dungeon files can pass &DcRoomAggroPreClearCondition to .Conditional().
// DUE while the room-trash value still has anything to clear (every
// RoomAggroRegistry boss: SM Cathedral, Scholomance Marduk & Vectus, ...).
bool DcRoomAggroPreClearCondition(Player* bot, AiObjectContext* context);

// --- event rows (one appender per dungeon) -------------------------------
void RegisterSunkenTempleEvents(std::vector<DungeonEvent>& out);
void RegisterZulFarrakEvents(std::vector<DungeonEvent>& out);
void RegisterShadowfangKeepEvents(std::vector<DungeonEvent>& out);
void RegisterScarletMonasteryEvents(std::vector<DungeonEvent>& out);
void RegisterRazorfenDownsEvents(std::vector<DungeonEvent>& out);
void RegisterBlackrockDepthsEvents(std::vector<DungeonEvent>& out);
void RegisterDeadminesEvents(std::vector<DungeonEvent>& out);
void RegisterWailingCavernsEvents(std::vector<DungeonEvent>& out);
void RegisterStratholmeEvents(std::vector<DungeonEvent>& out);
void RegisterUldamanEvents(std::vector<DungeonEvent>& out);
void RegisterScholomanceEvents(std::vector<DungeonEvent>& out);
void RegisterDireMaulEvents(std::vector<DungeonEvent>& out);
// Hellfire Ramparts (map 543) final-approach gate — see HellfireRampartsEvents.cpp
// for the measurements. Exposed so t/TestRampartsLedgeProbe can assert against the
// real navmesh that the zone-in platform lies outside the gate; the numbers are
// only meaningful together with that probe.
namespace DcHellfireRamparts
{
    // How far to scan for the Hellfire Sentries / Vazruden.
    constexpr float FINAL_APPROACH_SCAN = 45.0f;
    // Floor Z the bot must be above: the upper level the final platform sits on.
    constexpr float FINAL_APPROACH_MIN_Z = 76.0f;
}

void RegisterHellfireRampartsEvents(std::vector<DungeonEvent>& out);
void RegisterBloodFurnaceEvents(std::vector<DungeonEvent>& out);
void RegisterSlavePensEvents(std::vector<DungeonEvent>& out);
void RegisterUnderbogEvents(std::vector<DungeonEvent>& out);
void RegisterOldHillsbradEvents(std::vector<DungeonEvent>& out);
void RegisterMechanarEvents(std::vector<DungeonEvent>& out);
void RegisterShatteredHallsEvents(std::vector<DungeonEvent>& out);
void RegisterSteamvaultEvents(std::vector<DungeonEvent>& out);
void RegisterArcatrazEvents(std::vector<DungeonEvent>& out);
void RegisterSethekkHallsEvents(std::vector<DungeonEvent>& out);
void RegisterBlackMorassEvents(std::vector<DungeonEvent>& out);
// Everything in the Black Morass wave that DRAINS Medivh's shield rather than
// fighting the party: the nine trash adds (smart_scripts SMART_EVENT_RESET ->
// CAST 'Corrupt Medivh' 31326 on SELF) AND AEONUS, whose boss_aeonus::
// IsSummonedBy does the same thing in C++ and whose 37853 drains at DOUBLE the
// rate. All of them spawn REACT_DEFENSIVE and park at a home 14yd from Medivh, so
// they never aggro the party and the engage pipeline's natural pull never reaches
// them — the wave driver (ObjectiveHookRegistry hook 12) force-pulls them and
// counts them to decide when Medivh's ring needs cleaning. Excludes the Rift
// Lords / Keepers and the wave-6/12 bosses, which all fight normally.
std::vector<uint32> const& BlackMorassDrainEntries();

// The Black Morass RIFT KEEPERS — the mob each Time Rift summons 6s after it
// opens, and the ONLY thing whose death closes the rift
// (npc_time_rift::SummonedCreatureDies -> DespawnOrUnsummon). Shared with the
// wave driver (ObjectiveHookRegistry hook 12), which selects and pulls by it.
// Disjoint from BlackMorassDrainEntries() on purpose: keepers fight normally and
// never channel Corrupt, so they are never sweep targets — and the drainers never
// close a rift, so they are never selection targets.
//
// AEONUS IS NOT HERE despite being the wave-18 boss: it walks off to Medivh the
// instant it spawns (so it is never at the rift to select on) and it is not its
// rift's _riftKeeperGUID (so killing it closes nothing). It is a drainer.
std::vector<uint32> const& BlackMorassKeeperEntries();

// Wrath of the Lich King.
void RegisterUtgardeKeepEvents(std::vector<DungeonEvent>& out);
void RegisterNexusEvents(std::vector<DungeonEvent>& out);
void RegisterAzjolNerubEvents(std::vector<DungeonEvent>& out);
void RegisterAhnkahetEvents(std::vector<DungeonEvent>& out);
// Drak'Tharon Keep (map 600) — Novos' camp, shared with the hold driver
// (ObjectiveHookRegistry hook 14, HoldNovosCamp) so the camp and the keep-out it
// is placed against have exactly ONE definition. Every number is measured; the
// reasoning is in DrakTharonKeepEvents.cpp.
namespace DcDrakTharonKeep
{
    constexpr uint32 NOVOS = 26631;

    // Column-probed against the live 600 mmtiles: one walkable surface at
    // z 28.39. 19.3yd from Novos (-379.27, -737.73), 14.9yd from the Fetid Troll
    // Corpses' arrival point, 56yd from the staircase spawn trigger.
    constexpr float CAMP_X = -379.0f;
    constexpr float CAMP_Y = -757.0f;
    constexpr float CAMP_Z = 28.4f;

    // Grid-scan radius for Novos, from the activation predicate and the driver
    // alike. The chamber is ~96 x 88yd; this must cover it and the approach
    // without reaching Trollgore's arena 206yd away.
    constexpr float NOVOS_SCAN = 120.0f;

    // Keep-out around Novos while the Arcane Field (47346) is up. THIS MUST TRACK
    // the map-600 47346 row's placement `radius` in DcHazardRegistry — the driver
    // and the placement solver have to agree on one cylinder, and
    // t/TestDcHazard's DrakTharonArcaneFieldKeepOutAgreesWithTheNovosCamp pins
    // the camp against it.
    constexpr float FIELD_KEEPOUT = 14.0f;

    // Re-centring leash (the tank comes home past this UNLESS it is in melee
    // contact) and the hard leash (it comes home regardless). The hard leash is
    // sized to catch the three places phase 1 can strand a party — the staircase
    // at 56yd, ROOM_LEFT at 40yd and ROOM_RIGHT at 50yd — while leaving the
    // corpse arrival point at 14.9yd comfortably inside.
    constexpr float CAMP_LEASH = 6.0f;
    constexpr float CAMP_HARD_LEASH = 25.0f;
}

void RegisterDrakTharonKeepEvents(std::vector<DungeonEvent>& out);

// The Violet Hold (map 608) — the numbers the declarative half
// (VioletHoldEvents.cpp) and the imperative half (VioletHoldDriver.cpp) must
// agree on. Every one of them is either read straight out of violet_hold.h or
// probed against the live 608 mmtile; the reasoning is in VioletHoldEvents.cpp.
// Shared here for the same reason DcDrakTharonKeep is: a camp and the keep-out
// it is placed against need exactly ONE definition, and the gtests pin them
// against each other.
namespace DcVioletHold
{
    constexpr uint32 MAP = 608;

    // Creature entries (violet_hold.h VHCreatures).
    constexpr uint32 NPC_SINCLARI             = 30658;
    constexpr uint32 NPC_PRISON_DOOR_SEAL     = 30896;
    constexpr uint32 NPC_TELEPORTATION_PORTAL = 31011;
    constexpr uint32 NPC_CYANIGOSA            = 31134;
    constexpr uint32 NPC_ICHORON              = 29313;
    constexpr uint32 NPC_ICHOR_GLOBULE        = 29321;

    // The aura every wave add parks on the Prison Door Seal
    // (violet_hold_trashAI::CreatureStartAttackDoor -> DoCastAOE). Spell.dbc:
    // effect 6, aura 23 SPELL_AURA_PERIODIC_TRIGGER_SPELL, amplitude 3000ms,
    // DurationIndex 21 = INFINITE. spell_destroy_door_seal_aura turns each tick
    // into one ACTION_DECREASE_DOOR_HEALTH, so ONE add at the door costs the
    // 100-point gate one point every three seconds: 300s to lose the run alone,
    // 150s for two, 100s for three. That arithmetic is what sizes SEAL_DIRTY_MIN.
    constexpr uint32 SPELL_DESTROY_DOOR_SEAL  = 58040;

    // GetData ids (violet_hold.h VHData). _gateHealth has NO GetData case, which
    // is why the driver reads the seal's aura instead of the drain level.
    constexpr uint32 DATA_ENCOUNTER_STATUS    = 30;
    constexpr uint32 DATA_WAVE_COUNT          = 33;

    // GetBossState slots (violet_hold.h VHBosses). NOT DungeonEncounter bits:
    // the released pair is rolled per instance, so killing Zuramat as the first
    // prisoner sets no bit that names Zuramat. Completion rides these.
    constexpr uint32 BOSS_STATE_1ST           = 0;
    constexpr uint32 BOSS_STATE_2ND           = 1;
    constexpr uint32 BOSS_STATE_CYANIGOSA     = 2;

    // ObjectiveHookRegistry ids. Three defend hooks, not one: a Custom step is
    // handed a default-constructed DungeonBossInfo, so a shared hook cannot tell
    // which objective invoked it.
    constexpr uint32 HOOK_START               = 15;
    constexpr uint32 HOOK_DEFEND_1ST          = 16;
    constexpr uint32 HOOK_DEFEND_2ND          = 17;
    constexpr uint32 HOOK_DEFEND_CYANIGOSA    = 18;
    constexpr uint32 HOOK_DRIVE_WAVE          = 19;

    // THE DOOR CAMP — the EMERGENCY position, not the default one.
    //
    // On the flat door landing at the top of the entrance ramp (the ramp runs
    // z 38.6 at x~1869.8 up to z 44.0 at x~1861.5), straddling the funnel that
    // the last two waypoints of ALL SIX trash paths run through — (1858.95,
    // 810.05), (1860.84, 806.65), (1861.54, 804.15) and (1857.81, 796.77) all
    // lie within 7yd of it. 10.4yd inside the convergence midpoint and 32.4yd
    // from the Prison Seal itself, so the party is between the adds and the door
    // without standing on the door. Column-probed against the live 608 mmtile:
    // exactly ONE walkable surface, z 44.23.
    //
    // The first cut of this dungeon made this the party's STANDING position and
    // let the siege walk to it. That reading of the chokepoint is arithmetically
    // tidy and is not how the Violet Hold is played, or won: a keeper portal
    // pumps 3-4 adds every 20 seconds FOREVER and the only off-switch is 52-86yd
    // away at the rim, so a party that waits at the door fights the pump's output
    // instead of the pump and falls further behind every cycle. The party now
    // stations at the live portal (STAGE / rule 5 in VhDriveWave) and comes BACK
    // here only when something has actually reached the seal.
    constexpr float CAMP_X = 1855.0f;
    constexpr float CAMP_Y = 803.5f;
    constexpr float CAMP_Z = 44.05f;

    // THE STAGING POINT — where the party waits when no portal is open.
    //
    // The middle of the arena floor, at the foot of the entrance ramp: the core's
    // own MiddleRoomLocation, which is where Cyanigosa MoveJumps to on wave 18 and
    // 4.4yd from the wave-6/12 saboteur portal, so it is a position the encounter
    // itself treats as the centre of the fight. Column-probed against the live 608
    // mmtile: exactly ONE walkable surface, z 38.89.
    //
    // It exists because the party's job between waves is to be CLOSE TO THE NEXT
    // PORTAL, and the six rim portals average 45.5yd from here against 69.0yd from
    // the door camp (worst case 59.0 against 85.5). That is ~3.5s off every hop, on
    // a clock where a keeper portal starts pumping 30s after it opens. Every trash
    // path also runs through this half of the room on its way to the door, so
    // waiting here still meets the leftovers of the previous wave head-on.
    constexpr float STAGE_X = 1892.29f;
    constexpr float STAGE_Y = 805.70f;
    constexpr float STAGE_Z = 38.44f;

    // Where every wave add ends up: the midpoint of the two convergence points
    // (1843.71, 805.81, 44.14) — paths 0, 1, 1-alt, 2, 5 — and (1845.58, 800.68,
    // 44.10) — paths 3, 4, and also violet_hold_trashAI::EnterEvadeMode's new
    // home. The driver measures "how dirty is the seal" from here when the aura
    // read is unavailable.
    constexpr float SEAL_X = 1844.6f;
    constexpr float SEAL_Y = 803.2f;
    constexpr float SEAL_Z = 44.12f;

    // Arena centroid (mean of the six rim portal positions), used only by the
    // wave event's proximity gate, and the grid-scan radius that covers the whole
    // hold. The room is ~110yd across and the far rim portal is 86yd from the
    // camp; 200 sees all of it from anywhere the driver roams.
    constexpr float ARENA_X = 1893.1f;
    constexpr float ARENA_Y = 804.7f;
    constexpr float ARENA_SCAN = 200.0f;
    constexpr float EVENT_DUE_RANGE = 200.0f;

    // "Released" for a caged prisoner (and for Erekem's two guards): the
    // instance's StartBossEncounter clears UNIT_FLAG_NON_ATTACKABLE and
    // SetImmuneToNPC/All(false) at the moment the cell opens, so the flags ARE
    // the release latch. Also false through Ichoron's shattered-bubble window,
    // when he carries UNIT_FLAG_NOT_SELECTABLE for 15s — which is what makes the
    // driver retarget to his Ichor Globules instead of standing in drop-target
    // limbo. Defined in VioletHoldEvents.cpp.
    bool IsReleased(Creature const* c);
}

void RegisterVioletHoldEvents(std::vector<DungeonEvent>& out);
void RegisterMoltenCoreEvents(std::vector<DungeonEvent>& out);

// --- Halls of Stone (map 599) ---------------------------------------------
//
// The numbers HallsOfStoneEvents.cpp authors, HallsOfStoneDriver.cpp steers by
// and t/TestHallsOfStone.cpp pins. Shared here for the same reason
// DcDrakTharonKeep, DcVioletHold and DcGundrak are: the hold point and the add
// spawns it is placed against need exactly ONE definition, and several of these
// are SAFETY numbers whose whole value is that a test can re-derive them.
//
// Every coordinate below was column-probed against the live 599 mmtiles
// (7 tiles: 599.mmap + 2929/2930/2931/2932/3030/3031/3032). That is not
// ceremony on this map: the columns under the Tribunal arena carry a PHANTOM
// SURFACE at z ~ -142 and the columns under the eastern half carry one at
// z ~ 0.16 — map 599 is in the flat-grid-height family
// (ac-map601-flat-gridheight-zero), so an anchor authored from a script literal
// rather than from the probed TOP surface is a sink waiting to happen.
namespace DcHallsOfStone
{
    constexpr uint32 MAP = 599;

    // --- creature entries -------------------------------------------------
    //
    // PLAIN NORMAL-MODE ENTRIES, ON BOTH DIFFICULTIES, and that is worth one
    // paragraph because creature_template says otherwise at a glance.
    //
    // Almost every creature here carries a difficulty_entry_1 twin: Brann
    // 28070 -> 31366, Sjonnir 27978 -> 31386, the three wave adds 27983/27984/
    // 27985 -> 31876/31877/31380, the two hazard triggers 28237/28265 ->
    // 31875/31878, the Earthen Dwarf 27980 -> 31391. Reading that column alone
    // suggests every entry-keyed step and scan in this dungeon is normal-only and
    // silently dead on heroic.
    //
    // IT IS NOT, because a difficulty twin is a STAT TEMPLATE, not a separate
    // spawned entry. Creature::InitEntry resolves DifficultyEntry[diff-1] into
    // `m_creatureInfo` — the stats, the flags, the damage — and then does
    // `SetEntry(Entry); // normal entry always`. So Creature::GetEntry() returns
    // the NORMAL id on heroic, and FindNearestCreature / GetCreatureListWithEntry
    // InGrid match on it exactly as they do on normal.
    //
    // The two ways that could still have gone wrong are both checked and both
    // clear: Brann is a DB spawn (guid 126801, id 28070), and every summon in
    // brann_bronzebeard.cpp passes the bare normal constant — SummonCreature(
    // NPC_KADDRAK...), SummonCreatureGroup(0..2) over creature_summon_groups rows
    // whose entry column is 27983/27984/27985. Nothing in the script ever names a
    // 318xx id; heroic differs only in the stat template and in three
    // IsHeroic() ? ... : ... REPEAT CADENCES.
    //
    // So: no difficulty-gated event rows, no heroic entries in the scan lists.
    // Recorded here because the obvious "fix" on reading difficulty_entry_1 is to
    // add them, and adding them would be dead weight that reads as a safety net.
    constexpr uint32 NPC_BRANN                   = 28070;
    constexpr uint32 NPC_SJONNIR                 = 27978;
    constexpr uint32 NPC_KRYSTALLUS              = 27977;
    constexpr uint32 NPC_MAIDEN_OF_GRIEF         = 27975;

    // The three stone faces. NullCreatureAI, faction 114, unit_flags 33554436
    // (NOT_SELECTABLE | 0x4): they cannot be targeted, damaged or interrupted,
    // and they are pure spell emitters. They exist ONLY between InitializeEvent()
    // and EndTribunalFight(), which is what makes a bare aliveness probe on them
    // a sound "the Tribunal is running" test — and the only probe that reads true
    // during the quiet first 52 seconds, before any add has spawned. These three
    // are also the only creatures on the map with NO difficulty twin at all.
    constexpr uint32 NPC_KADDRAK                 = 30898;
    constexpr uint32 NPC_MARNAK                  = 30897;
    constexpr uint32 NPC_ABEDNEUM                = 30899;

    // The wave adds (creature_summon_groups, summonerId 28070 — three rows of
    // 27983 at (943.088, 401.378, 206.161), two of 27984 at (967.000, 376.832,
    // 206.161), one of 27985 at (964.302, 381.942, 206.161)). Every one is a
    // TempSummon with spawnId 0, so the spawn store cannot see them and both the
    // predicate and the driver must grid-scan.
    constexpr uint32 NPC_DARK_RUNE_PROTECTOR     = 27983;
    constexpr uint32 NPC_DARK_RUNE_STORMCALLER   = 27984;
    constexpr uint32 NPC_IRON_GOLEM_CUSTODIAN    = 27985;

    // The two ground-hazard carriers, both NOT_SELECTABLE triggers. See the
    // DcHazardRegistry rows — these are EMITTERS (a creature carrying a pulsing
    // aura), NOT persistent-area-aura pools, and that distinction is load-bearing.
    constexpr uint32 NPC_SEARING_GAZE_TRIGGER    = 28265;
    constexpr uint32 NPC_DARK_MATTER_TARGET      = 28237;
    constexpr uint32 NPC_DARK_MATTER_VISUAL      = 28235;

    // Sjonnir's 25% adds. Summoned, then immediately handed a random PLAYER's
    // faction — they fight on your side. See the DcNeverTargetRegistry row.
    constexpr uint32 NPC_EARTHEN_DWARF           = 27980;

    // The three constructs Brann walks into while REACT_AGGRESSIVE.
    constexpr uint32 NPC_RAGING_CONSTRUCT        = 27970;
    constexpr uint32 NPC_UNRELENTING_CONSTRUCT   = 27971;
    constexpr uint32 NPC_LIGHTNING_CONSTRUCT     = 27972;

    // THE ONLY FUNCTIONAL DOOR ON THE MAP, and the whole reason this dungeon
    // needs automation. gameobject.state = 1 (closed), Data0 = 0, lockId 0. The
    // only code path that opens it is instance_halls_of_stone's
    // SetData(BRANN_DOOR, DONE), whose only caller is Brann arriving at
    // POINT_SJONNIR_DOOR — 3.2s after a player takes gossip 10012.
    //
    // The map's other five doors (191292/191293/191294/191295/191459) all spawn
    // state = 0 (open), are referenced by no C++ anywhere, and are deliberately
    // NOT in DcEventDoorRegistry::IsScriptOnly — see the note there.
    constexpr uint32 GO_SJONNIR_DOOR             = 191296;

    // --- the instance index space (halls_of_stone.h) ----------------------
    //
    // READ THE MASK OR THE BOSS STATE, NEVER GetData, for indices 0-4:
    //
    //   * Krystallus (0) and the Maiden of Grief (1) never call SetBossState at
    //     all, so GetBossState(0) / GetBossState(1) are permanently NOT_STARTED.
    //   * GetData(BRANN_BRONZEBEARD) is permanently 0: the escort writes only the
    //     boss state, and instance_halls_of_stone::GetData just returns the
    //     SetData-written Encounter[] slot. So a garrison cannot gate on the
    //     escort through instance data.
    //   * GetData(BOSS_TRIBUNAL_OF_AGES) is unusable as a gate: it stays 0 through
    //     the whole 300s fight and only starts toggling SPECIAL/DONE during the
    //     256s of post-fight lore. Completion rides mask bit 2 / GetBossState(2).
    //
    // BRANN_DOOR (5) is the exception and the ONE clean instance-data reading on
    // this map: brann_bronzebeard.cpp writes BOTH stores (SetBossState and
    // SetData) when the door opens, so GetData(BRANN_DOOR) >= DONE is truthful
    // and monotonic. That is what the escort step's data gate uses — see
    // HallsOfStoneEvents.cpp for why that gate is load-bearing for a reason
    // beyond completion.
    constexpr uint32 BOSS_KRYSTALLUS           = 0;
    constexpr uint32 BOSS_MAIDEN_OF_GRIEF      = 1;
    constexpr uint32 BOSS_TRIBUNAL_OF_AGES     = 2;
    constexpr uint32 BOSS_SJONNIR              = 3;
    constexpr uint32 BRANN_BRONZEBEARD         = 4;
    constexpr uint32 BRANN_DOOR                = 5;

    // DungeonEncounter bits (instance_encounters, both difficulties). Bit 2 is
    // creditType 1 (ENCOUNTER_CREDIT_CAST_SPELL) on spell 59046, which is exactly
    // why BossSpawnIndex cannot derive it and why this dungeon ends at 2/3 today.
    constexpr uint32 BIT_KRYSTALLUS            = 0;
    constexpr uint32 BIT_MAIDEN_OF_GRIEF       = 1;
    constexpr uint32 BIT_TRIBUNAL_OF_AGES      = 2;
    constexpr uint32 BIT_SJONNIR               = 3;

    // ObjectiveHookRegistry ids. Hook ids are one FLAT space across every
    // dungeon; 1-10 and 12-21 are taken, 11 is retired and stays retired.
    constexpr uint32 HOOK_TRIBUNAL             = 22;
    constexpr uint32 HOOK_WAVE                 = 23;

    // --- clear-path order keys --------------------------------------------
    // ONE contiguous 1..6 scale shared by the three derived bosses (reordered in
    // place, kill-bits untouched) and the three new objectives. The bosses'
    // RELATIVE order is exactly what their DBC bits already gave them; the scale
    // exists only so the objectives have somewhere to sit between the Maiden and
    // Sjonnir.
    constexpr int32 ORDER_KRYSTALLUS           = 1;
    constexpr int32 ORDER_MAIDEN               = 2;
    constexpr int32 ORDER_ESCORT               = 3;
    constexpr int32 ORDER_TRIBUNAL             = 4;
    constexpr int32 ORDER_DOOR                 = 5;
    constexpr int32 ORDER_SJONNIR              = 6;

    // --- geometry, every value column-probed ------------------------------

    // Objective 1's anchor. 6.8yd from Brann's DB spawn (1077.41, 474.16, 207.80,
    // guid 126801), which is inside the escort step's own 5yd gossip walk-in with
    // a tick of drift to spare, so simply ARRIVING all but closes the gap. Probed:
    // exactly ONE walkable surface in the column, z 208.07.
    constexpr float MEET_X = 1077.40f, MEET_Y = 481.00f, MEET_Z = 208.07f;

    // The escort pre-clear volume: the centroid of the FOUR constructs standing
    // in Brann's first forty yards, measured from the live spawn rows —
    // 27971 at (1032.59, 475.95) and (1049.09, 468.95), both within 8yd of
    // waypoint 2/3 of path 280701, plus 27971 (1036.02, 501.23) and 27970
    // (1058.29, 499.91) a further 25yd out. Radius 28 encloses all four with
    // margin and reaches nothing else. Probed: one surface, z 208.39.
    //
    // DELIBERATELY NOT the whole route. Three more Lightning Constructs sit at
    // the FAR end of the escort — (972.52, 420.20), (983.27, 390.11), (967.97,
    // 381.01) — and sweeping those would mean walking the entire 170yd corridor
    // ahead of Brann and then walking back for him. They are the escort step's
    // own threat-engage job (ESCORT_THREAT_R below), which is what that primitive
    // is for: the party is beside him by then.
    constexpr float PRECLEAR_X = 1044.00f, PRECLEAR_Y = 486.50f, PRECLEAR_Z = 208.39f;
    constexpr float PRECLEAR_R = 28.0f;

    // Where path 280701 ends and Brann re-offers gossip (menu 9670). Probed:
    // z 207.18 (plus the arena's phantom -142.03 beneath it).
    constexpr float ESCORT_END_X = 939.65f, ESCORT_END_Y = 375.49f, ESCORT_END_Z = 207.18f;

    // BRANN AT THE CONSOLE — the thing being defended for 300 seconds.
    // MovePoint target (897.1759, 331.77386, 203.70638); probed z 203.93.
    constexpr float CONSOLE_X = 897.18f, CONSOLE_Y = 331.77f, CONSOLE_Z = 203.93f;

    // THE HOLD POINT — objective 2's anchor and the party's station for the whole
    // Tribunal. Chosen from the probe, not estimated.
    //
    // It sits 25yd from Brann along the console -> add-spawn-centroid line. That
    // line is the one every add walks: all three summon groups land within a 34yd
    // cluster ((943.088, 401.378) / (967.000, 376.832) / (964.302, 381.942)) and
    // every add is Taunt-wired to Brann (51774 -> 51775), 83yd away. At 25yd out
    // the three approach lines have fanned to only ~10yd apart, so ONE camp
    // straddles all of them; at the 40yd the first sketch of this plan proposed
    // they are ~17yd apart and the party is covering a cone it cannot hold.
    //
    // Probed hard, because a hold point is where the party stands for five
    // minutes: the column gives z 203.93, and an 8yd ring around it is walkable
    // at every one of eight bearings (203.93 on the five toward Brann, 204.10 on
    // the three toward the ramp the adds descend). No lip, no step, no hole.
    //
    // It is also 3.7yd from Brann's post-fight lore stop, which is a real bonus
    // rather than a coincidence: both sit on the same line. The 10206 gossip that
    // skips the 256s of lore therefore needs no travel at all — the party is
    // already standing on it when he walks up.
    constexpr float HOLD_X = 915.75f, HOLD_Y = 348.51f, HOLD_Z = 203.93f;

    // Brann's post-fight lore stop, where he offers gossip 10206
    // (POINT_TRIBUNAL_LORE, MovePoint target (917.253, 351.925, 203.699)).
    constexpr float LORE_X = 917.25f, LORE_Y = 351.93f, LORE_Z = 203.93f;

    // The three add spawn points, verbatim from creature_summon_groups. Exposed
    // so the driver can hold the intercept line and the gtest can re-derive the
    // hold point's placement from them rather than trusting a literal.
    constexpr float SPAWN_PROTECTOR_X   = 943.088f, SPAWN_PROTECTOR_Y   = 401.378f;
    constexpr float SPAWN_STORMCALLER_X = 967.000f, SPAWN_STORMCALLER_Y = 376.832f;
    constexpr float SPAWN_GOLEM_X       = 964.302f, SPAWN_GOLEM_Y       = 381.942f;

    // Objective 3's anchor. 2.2yd from where Reset() teleports the respawned
    // Brann (1199.685, 667.155, 196.324) — comfortably inside INTERACTION_DISTANCE
    // — and 9.1yd WEST of the closed door, i.e. on the party's side of it. Probed:
    // top surface z 195.56 (with 162.29 and 0.16 beneath — see the header note).
    constexpr float DOOR_STAGE_X = 1197.50f, DOOR_STAGE_Y = 667.10f, DOOR_STAGE_Z = 195.56f;

    // --- the wave event's proximity gate ----------------------------------
    // Centroid of the Tribunal arena. The wave event is due only within
    // EVENT_DUE_RANGE of this, which is what keeps the driver from steering a bot
    // that cannot see the fight (a corpse run) — and, just as importantly, keeps
    // it not-due at Brann's DB spawn 183.5yd away, where the escort still owns
    // the party. The Sjonnir door is 403yd off, far outside.
    constexpr float ARENA_X = 930.0f, ARENA_Y = 365.0f;
    constexpr float EVENT_DUE_RANGE = 150.0f;

    // Grid-scan radius for the heads and the adds. From the hold point the
    // furthest thing worth finding is the Stormcaller spawn at 58.5yd; 120 covers
    // the whole bowl from anywhere in it without reaching out of the arena.
    constexpr float ARENA_SCAN = 120.0f;

    // --- escort tuning ----------------------------------------------------
    // Brann walks 170yd alone as REACT_AGGRESSIVE with regeneration off and no
    // immunity, and his death restarts the whole escort from his DB spawn. So the
    // threat scan around him is deliberately WIDER than the 18yd default: the
    // Lightning Construct at (967.97, 381.01) sits 22yd off his waypoint 14, and
    // meeting it before he does is the difference between a 2-minute escort and
    // two of them.
    constexpr float ESCORT_STANDOFF  = 5.0f;
    constexpr float ESCORT_THREAT_R  = 25.0f;
    constexpr float ESCORT_THREAT_Z  = 15.0f;
    constexpr float ESCORT_SEARCH_R  = 100.0f;

    // --- step budgets -----------------------------------------------------
    // The Tribunal is a FIXED 300s survival timer plus a 17s wrap-up; nothing the
    // party does shortens it. 10 minutes bounds a run that has gone wrong without
    // ever being the thing that ends a healthy one — the hook returns Done off
    // mask bit 2 the moment the credit spell lands.
    //
    // NOTE there is deliberately NO escort timeout below. The EscortCreature step
    // is watchdog-owned (DungeonEventExecutor never escalates it on elapsed time;
    // DriveEscortCreature's own dead-air watchdog owns liveness), so authoring one
    // would be a number that does nothing.
    constexpr uint32 PRECLEAR_TIMEOUT_MS   = 180000;
    constexpr uint32 TRIBUNAL_TIMEOUT_MS   = 600000;
    constexpr uint32 LORE_SKIP_TIMEOUT_MS  = 120000;
    constexpr uint32 DOOR_GOSSIP_TIMEOUT_MS = 120000;
    constexpr uint32 DOOR_STATE_TIMEOUT_MS = 60000;
    constexpr uint32 WAVE_TIMEOUT_MS       = 600000;
}

void RegisterHallsOfStoneEvents(std::vector<DungeonEvent>& out);

// --- Halls of Lightning (map 602) ------------------------------------------
//
// The numbers HallsOfLightningEvents.cpp authors, HallsOfLightningDriver.cpp
// steers by and t/TestHallsOfLightning.cpp pins. Shared here for the same reason
// DcBlackwingLair, DcVioletHold and DcHallsOfStone are: the transit's corridor
// box, its route slice and the point it ends at need exactly ONE definition, and
// the probe suite re-derives them against the real mesh.
//
// NO ROSTER PATCH, and that is worth writing down rather than leaving as an
// absence. All four of map 602's instance_encounters rows are creditType 0
// (ENCOUNTER_CREDIT_KILL_CREATURE) against real `creature` spawns, and
// DungeonEncounter.dbc's order (Bjarngrim 0, Volkhan 1000, Ionar 2000, Loken
// 3000) IS the walking order — so BossSpawnIndex::Build derives the whole list
// with correct coordinates on both difficulties. This is the first WotLK map in
// the module where that is true; the previous four (Halls of Stone, Drak'Tharon,
// Gundrak, The Nexus) each needed a patch to be runnable at all. Do not add one
// here: the transit below is a CONDITIONAL event, which needs no roster row, no
// order keys and no OBJ() anchor, and a patch could only make things worse.
//
// DIFFICULTY TWINS ARE STAT TEMPLATES, NOT ENTRIES. Almost every creature on
// this map has one (Slag 28585 -> 30970, Volkhan 28587 -> 31536, ...), and
// Creature::InitEntry resolves DifficultyEntry[diff-1] into m_creatureInfo and
// then does SetEntry(Entry) — "normal entry always" — so GetEntry() returns the
// NORMAL id on heroic. Every entry-keyed constant below is the normal id and is
// correct on both difficulties. Do not add the 30xxx/31xxx ids anywhere.
//
// EVERY COORDINATE HERE IS COLUMN-PROBED against the live 602 mmtiles
// (t/TestHallsOfLightningRouteProbe), and on this map that is not ceremony. The
// mmtiles carry flat sheets far below the dungeon — a continuous surface at
// z ~ -1.9 and another at z ~ -13.9 under essentially the whole footprint — so
// map 602 is in the flat-grid-height family ([[ac-map601-flat-gridheight-zero]])
// and an anchor authored from a script literal rather than from the probed TOP
// surface is a sink waiting to happen. The open centre of Bjarngrim's ring
// (1330, 30) has NO dungeon floor at all: probing it returns 143.02, -1.88,
// -13.88, so a hold point or camp placed there drops a bot 45-55yd.
namespace DcHallsOfLightning
{
    constexpr uint32 MAP_ID = 602;

    // The four bosses, in DungeonEncounter.dbc order — which is also the order
    // the party walks them.
    constexpr uint32 NPC_BJARNGRIM = 28586;
    constexpr uint32 NPC_VOLKHAN   = 28587;
    constexpr uint32 NPC_IONAR     = 28546;
    constexpr uint32 NPC_LOKEN     = 28923;

    // Volkhan's slot in instance_halls_of_lightning's OWN HoLBossIds enum, which
    // is what GetBossState is keyed on. It is THREE, not one: the header's order
    // is Bjarngrim 0, Ionar 1, Loken 2, Volkhan 3, and that is NOT the DBC bit
    // order. Read from halls_of_lightning.h; never reuse an encounterIndex here.
    constexpr uint32 VOLKHAN_ENCOUNTER_INDEX = 3;

    // --- the Slag Furnace (the Bjarngrim -> Volkhan transit) ---------------
    //
    // The 395yd of pit between Bjarngrim's hall and Volkhan's gallery, and the
    // one leg on this map the ordinary clear cannot cross. Three facts, and
    // between them they are the whole design:
    //
    //   1. THE PIT IS THE ONLY WAY THROUGH. An A* over the map-602 mmtiles with
    //      every poly below z 40 removed returns NO PATH from Bjarngrim's floor
    //      to Volkhan. The party must descend 17yd from the hall's south arm
    //      (z 40.7 -> z 23.9), walk ~110yd south along the pit floor, climb the
    //      far wall to the mid ledge (z 38.6) and climb again to the gallery
    //      (z 52.7) — 395yd of route for 80yd of straight line. There is no
    //      upper gallery route.
    //   2. FOURTEEN SLAG (28585) LINE THE WALKWAY ON A TWENTY-SECOND RESPAWN
    //      (`creature.spawntimesecs = 20`, MovementType 1, wander_distance 8 —
    //      the shortest respawn on any map this module runs). Twelve of the
    //      fourteen are within 20yd of the route line in 2D; with the wander all
    //      fourteen reach it, and level 79 against level 80 is inside base aggro
    //      range for the whole walk.
    //   3. SO THE CLEAR HAS NO DRIVER IN THERE. DcCombatFlag::MayDrive is false
    //      while anything in the party is engaged and Advance is registered only
    //      in the NON-combat engine, so party engagement never drops for the
    //      no-engage grace window. The leg is not slow; it is stopped. This is
    //      Blackwing Lair's Suppression Rooms one tier down.
    //
    // AND KILLING THEM IS NEGATIVE PROGRESS, which is why the answer is a
    // transit and not a clear. Each Slag is 15 828 HP with DamageModifier 1 —
    // trivial, harmless in melee — but smart_scripts 28585 id 1/3 is
    // `On Just Died -> Blast Wave` (23113 normal / 22424 heroic): 10yd, and
    // SPELL_AURA_MOD_DECREASE_SPEED -50% for 6s. Fourteen deaths inside a 110yd
    // crossing is a party that spends most of the crossing at half speed.
    //
    // HONEST NOTE ON WHAT THIS IS *NOT*. Unlike Blackwing Lair's whelps this
    // does NOT meet DcNeverTargetRegistry's class-3 arithmetic bar — 0.7
    // spawns/s against fourteen low-HP mobs is a population a high-DPS party
    // genuinely can clear, where BWL's 5.3/s across 375yd is not. The problem
    // here is the SNARE and the MISSING DRIVER, and the fix addresses those. No
    // never-target rows are authored for 28585; if the transit alone turns out
    // not to cross the pit, the row wants a new justification written into that
    // table's doc comment, not an existing class stretched to fit.

    constexpr uint32 NPC_SLAG = 28585;

    // The two things in the pit that ARE worth standing and killing: Unbound
    // Firestorm (28584), 3600s respawn, two of them at (1300.0, -214.8, 23.3)
    // and (1362.2, -215.2, 23.3) — at the foot of the climb out — plus the
    // Blistering Steamragers (28583) that share their 3600s timer on the mid
    // ledge above. A kill here is progress that stays bought; a Slag kill is
    // not. This is BWL's Taskmaster/Hatcher role, and the driver's Elite hold
    // is keyed on exactly these two entries.
    constexpr uint32 NPC_UNBOUND_FIRESTORM       = 28584;
    constexpr uint32 NPC_BLISTERING_STEAMRAGER   = 28583;

    // WHERE THE TRANSIT'S SLICE OF THE ROUTE STARTS AND ENDS.
    //
    // The registry row for Volkhan is the WHOLE Bjarngrim -> Volkhan leg
    // (29 anchors; see RegisterHallsOfLightningRoute), because the ordinary
    // clear needs a polyline to walk and a cursor to project onto for the halves
    // the transit does not own. The driver is handed the middle of it — anchors
    // TRANSIT_STAGE_ANCHOR_INDEX .. TRANSIT_END_ANCHOR_INDEX — so that its
    // cursor 0 IS the staging point and its LAST anchor IS the point the
    // crossing ends at, which is what the kernel's gather gate (cursorIndex 0)
    // and completion test (the last anchor) both mean.
    //
    // A MIDDLE slice, where Blackwing Lair takes a tail one. The difference is
    // real geometry, not taste: BWL's crossing ends AT the next boss, so its
    // route simply stops there; here the party still has a hairpin and a second
    // ramp to climb after the pit, and those are ordinary ground the clear
    // walks — and must NOT be steered by a driver whose route ends behind them.
    constexpr std::size_t TRANSIT_STAGE_ANCHOR_INDEX = 5;
    constexpr std::size_t TRANSIT_END_ANCHOR_INDEX   = 18;

    // THE STAGING POINT — the top of the descent, and the last quiet ground
    // before the gauntlet. Routed and column-probed: (1323.21, -46.56, 40.66) is
    // the corridor point at which the hall's south arm stops being flat and
    // starts dropping. Nearest Slag 78yd; nothing in the pit reaches it.
    //
    // The ordinary clear delivers the party here by itself (it is out of combat
    // for the whole hall once Bjarngrim is down), so the transit's first act is
    // a gather, not a haul.
    constexpr float TRANSIT_STAGE_X = 1323.21f;
    constexpr float TRANSIT_STAGE_Y = -46.56f;
    constexpr float TRANSIT_STAGE_Z = 40.66f;

    // THE MID LEDGE — where the crossing ends and the ordinary pipeline (pull,
    // muster, standoff, engage) takes back over. The top of the climb out of the
    // pit, z 38.55, with the four z38 trash spawns 18-30yd west and east of it.
    //
    // Those spawns are the REASON the transit stops here rather than running on
    // to Volkhan: they are 3600s respawns on open ground, which is a fight worth
    // setting up for, and NO_STOP is released at the same anchor.
    constexpr float TRANSIT_END_X = 1340.53f;
    constexpr float TRANSIT_END_Y = -233.26f;
    constexpr float TRANSIT_END_Z = 38.55f;

    // How close the leader has to get before the crossing is over. Also the
    // predicate's own OFF switch: the event is due while the leader is inside the
    // pit box and NOT yet here, so arriving simply stops it being due. There is
    // no completion latch to reset — a party shoved back into the pit re-arms it,
    // which is the correct answer, and a wipe leaves no stale flag behind.
    constexpr float TRANSIT_END_RADIUS = 10.0f;

    // --- THE CORRIDOR, and why it is TWO boxes ------------------------------
    //
    // The transit's activation predicate is gated on the leader standing inside
    // this volume, which is what keeps a rung registered on every bot's combat
    // engine inert for the other three encounters of this dungeon.
    //
    // ONE box cannot draw it, and the reason is a switchback. The climb OUT of
    // the pit runs north-to-south at x 1340.5 from z 23.9 up to z 38.6; the climb
    // on to Volkhan's gallery runs back south-to-north at x 1348-1350 from z 38.6
    // up to z 52.7. Two ramps, 7.5yd apart in x, overlapping in y and in z for
    // their whole lower halves. A single box wide enough to hold the pit also
    // holds the gallery ramp, and a leader standing on THAT would have the
    // transit re-arm behind it: ResolveCursor would project it back on to the pit
    // ramp and the driver would walk the party back down the hole.
    //
    // So: box 1 is the descent and the pit floor, cut off at y -205 — north of
    // the switchback entirely. Box 2 is the climb out, narrow in x (1330..1346)
    // so it holds the pit ramp and excludes the gallery ramp (whose nearest
    // point is x 1348.03). Both share a z ceiling of 44, which is what excludes
    // the gallery itself (floor z 52.7) and Volkhan (z 57.0) from either.
    //
    // Certified against the authored anchors in t/TestHallsOfLightning: anchors
    // 5..18 are inside, 0..4 and 19..28 are outside, and so are Bjarngrim, the
    // entrance, Volkhan, Ionar, Loken and the Hall of the Watchers.
    constexpr float PIT_BOX_MIN_X = 1298.0f;
    constexpr float PIT_BOX_MAX_X = 1358.0f;
    constexpr float PIT_BOX_MIN_Y = -205.0f;
    constexpr float PIT_BOX_MAX_Y = -44.0f;
    constexpr float PIT_BOX_MIN_Z = 18.0f;
    constexpr float PIT_BOX_MAX_Z = 44.0f;

    constexpr float CLIMB_BOX_MIN_X = 1330.0f;
    constexpr float CLIMB_BOX_MAX_X = 1346.0f;
    constexpr float CLIMB_BOX_MIN_Y = -240.0f;
    constexpr float CLIMB_BOX_MAX_Y = -205.0f;
    constexpr float CLIMB_BOX_MIN_Z = 18.0f;
    constexpr float CLIMB_BOX_MAX_Z = 44.0f;

    // Is `bot` standing inside the Slag Furnace corridor right now? Two bbox
    // tests, and the transit's cheapest real gate after the map compare.
    // Defined in HallsOfLightningEvents.cpp.
    bool InTransitCorridor(Player* bot);

    // --- the driver's knobs -------------------------------------------------
    //
    // CONSTANTS, NOT SETTINGS. Blackwing Lair's equivalents are DcSettings rows
    // because that transit was retrofitted onto a raid mid-testing and the raid
    // scale genuinely varies; nothing here does. A five-man's pit crossing has
    // one right answer for each of these and a conf row nobody will ever turn is
    // cost pushed onto every future reader of the config.

    // The gather radius, and the pack leash — how tight a ball the party enters
    // the gauntlet as, and how far off the leader's route cursor a member may be
    // before the leg stops for it.
    //
    // Both are 5-man numbers and both are tighter than Blackwing Lair's 20/25.
    // The argument for the transit is that a wide ball sweeps more of a pit whose
    // spawns are the problem, and four followers behind one tank have no
    // legitimate reason to be strung over 25yd on a walkway 41yd wide at its
    // narrowest. TRANSIT_PACK_LEASH must stay comfortably above the gather
    // radius' own floor — see DcSuppressionTransit::GatherRadiusFloor, which the
    // driver applies for exactly the [[dc-party-gate-must-read-the-rungs-ring]]
    // reason.
    constexpr float TRANSIT_GATHER_RADIUS = 15.0f;
    constexpr float TRANSIT_PACK_LEASH    = 20.0f;

    // The fraction of living followers that must be inside the leash. 0.75 is
    // three of four — one bot that cannot path in must never hold the other
    // three at the top of a ramp, and stranded recovery (relevance 42) sits above
    // this whole ladder and owns that member.
    constexpr float TRANSIT_GATHER_QUORUM = 0.75f;

    // How close a Firestorm / Steamrager has to be before the driver stands and
    // fights it instead of walking past. 20yd is a little over the aggro band of
    // the level-79/80 elites on this leg, so the hold arms as the party pulls
    // them rather than after they are already in the middle of the column.
    //
    // SLAGS ARE DELIBERATELY NOT A HOLD. The tank face-pulls what walks into it
    // and keeps moving; the party fights on the move. That is the whole point of
    // NO_STOP plus OwnsThePull, and it is the difference between a transit and a
    // clear.
    constexpr float TRANSIT_ELITE_HOLD_RADIUS = 20.0f;

    // Grid-scan radius for the driver's per-tick elite sweep. Wide enough to see
    // a hold-worthy spawn from the middle of a leg (the widest hold radius is
    // 20yd and the longest authored leg is 16), tight enough that it never
    // reaches from the pit floor up on to the gallery.
    constexpr float TRANSIT_SCAN = 40.0f;

    // How far the leader may be from the staging point when the transit ARMS and
    // still be asked to gather there. Past this the gather latches open and says
    // so: you cannot gather at a point you have already walked past, and a driver
    // that insisted would march a leader standing in the pit 200yd back up the
    // ramp to form up. The case is a re-arm after a partial wipe, which is
    // exactly when walking backwards is worst.
    constexpr float TRANSIT_STAGE_SKIP_DIST = 40.0f;

    // How far the leader may be from its own stored cursor before the projection
    // is believed over it (DcSuppressionTransit::ResolveCursor). Inside a working
    // crossing nothing is ever this far from the anchor it is walking to; a
    // leader that is has died and come back, and the stored index is a fact about
    // a previous attempt.
    constexpr float TRANSIT_CURSOR_RESYNC_DIST = 50.0f;

    // The three hold watchdogs. None of them is a target: each bounds ONE wait,
    // and on expiry the driver walks on and logs which hold gave up. The elite
    // budget is a quarter of Blackwing Lair's because the fights are — two
    // Firestorms, not six Taskmasters — and the gather budget is halved for the
    // same reason (four followers, not twenty-four, and no whelps at the staging
    // point to keep them in combat).
    constexpr uint32 TRANSIT_GATHER_TIMEOUT_MS = 30000;
    constexpr uint32 TRANSIT_PACK_HOLD_TIMEOUT_MS  = 20000;
    constexpr uint32 TRANSIT_ELITE_HOLD_TIMEOUT_MS = 30000;

    // Throttle on the driver's per-tick telemetry line. Without the line a failed
    // run says nothing about WHICH mechanism is still biting; without the
    // throttle it says it several times a second, per bot.
    constexpr uint32 TRANSIT_TELEMETRY_MS = 3000;

    // Bound on the whole crossing, as the Custom step's timeout. The walk is
    // 395yd of route; at a snared 3.5yd/s that is under two minutes, and the two
    // Firestorm fights are seconds. Five minutes is a ceiling on a genuinely
    // broken run and never the binding constraint on a working one — the event is
    // Repeatable and every yielded tick re-bases the step clock, so this can only
    // fire when the driver has held continuously for the whole budget, i.e. when
    // a hold's own watchdog has failed to release.
    constexpr uint32 TRANSIT_TIMEOUT_MS = 300000;

    // The event row and the hook that drives it. Event ids are per-map; HOOK ids
    // are ONE FLAT SPACE across every dungeon (see ObjectiveHookRegistry::AddHook,
    // which LOG_ERRORs a collision rather than silently dropping one) — 1-14 are
    // the older dungeons', 15-19 the Violet Hold's, 20-21 Blackwing Lair's and
    // 22-23 Halls of Stone's, so this is 24.
    constexpr uint32 EVENT_SLAG_FURNACE_TRANSIT = 1;
    constexpr uint32 HOOK_SLAG_FURNACE_TRANSIT  = 24;
}

void RegisterHallsOfLightningEvents(std::vector<DungeonEvent>& out);

// --- Gundrak (map 604) ----------------------------------------------------
// The numbers GundrakEvents.cpp authors and t/TestGundrak.cpp pins. Shared here
// for the same reason DcDrakTharonKeep and DcVioletHold are: several of them are
// SAFETY numbers whose whole value is that a test can re-derive them from the
// live spawn data (the mojo search that must exclude four trash spawns, the pool
// volume that must name exactly three of six Ruins Dwellers), and a literal
// buried in the .cpp cannot be pinned. Every coordinate was column-probed against
// the live 604 mmtiles — see GundrakEvents.cpp for why each is where it is, and
// why on THIS map a GO's own Z is never usable.
namespace DcGundrak
{
    constexpr uint32 MAP = 604;

    // Encounters. The entry numbering does NOT follow the encounter order:
    // 29305 is Moorabi and 29306 is Gal'darah, not the other way round.
    constexpr uint32 SLADRAN   = 29304;
    constexpr uint32 MOORABI   = 29305;
    constexpr uint32 GALDARAH  = 29306;
    constexpr uint32 COLOSSUS  = 29307;
    constexpr uint32 ECK       = 29932;

    // The Drakkari ELEMENTAL — the Colossus's instance_encounters credit entry
    // (it is what dies; SummonedCreatureDies then KillSelf()s the Colossus). It
    // has no `creature` row on any map, which is exactly why BossSpawnIndex drops
    // the Colossus. Never an anchor entry; recorded so the roster test can say
    // which entry it must NOT have used.
    constexpr uint32 DRAKKARI_ELEMENTAL = 29573;

    constexpr uint32 LIVING_MOJO   = 29830;
    constexpr uint32 RUINS_DWELLER = 29920;

    // DungeonEncounter.dbc bits, read off the live DBC. The Colossus is bit 1 on
    // BOTH difficulties, so one MakeBossWithBit serves both; Eck is heroic-only
    // bit 3 (and heroic Gal'darah shifts to bit 4, which the DERIVED row already
    // carries correctly).
    constexpr uint32 BIT_COLOSSUS = 1;
    constexpr uint32 BIT_ECK      = 3;

    // The three altars and the four bridge statues (gundrak.h). Each altar's
    // click drives ITS OWN statue to GO_STATE_READY via instance_gundrak::SetData
    // — 192518 -> 192564, 192520 -> 192567, 192519 -> 192565. That mapping is easy
    // to transpose (the script's own _bridgeGUIDs indices run Slad'ran / Drakkari
    // / Moorabi, a DIFFERENT order from the DATA_* enum), so it is pinned by test.
    constexpr uint32 ALTAR_SLADRAN  = 192518;
    constexpr uint32 ALTAR_MOORABI  = 192519;
    constexpr uint32 ALTAR_COLOSSUS = 192520;

    constexpr uint32 STATUE_SNAKE   = 192564;  // Slad'ran's
    constexpr uint32 STATUE_MAMMOTH = 192565;  // Moorabi's
    constexpr uint32 STATUE_RHINO   = 192566;  // Gal'darah's — the BRIDGE witness
    constexpr uint32 STATUE_TROLL   = 192567;  // the Colossus's

    // Where all four statues stand, on the bridge-chamber floor 61-89yd from the
    // altars that drive them. STATUE_SEARCH has to reach this from every altar
    // anchor or the verification step hangs forever.
    constexpr float STATUE_X = 1775.16f;
    constexpr float STATUE_Y = 743.455f;
    constexpr float STATUE_Z = 119.073f;

    // --- the three altar clicks ------------------------------------------
    //
    // Two of the three altars stand in HOLES in the navmesh (the GO's own column
    // has no walkable surface at its Z), so each of those gets a roomy objective
    // anchor plus a short MoveTo onto a measured rim pad. The third, Moorabi's,
    // has 7.25yd of continuous mesh under it and needs neither.
    constexpr float SLADRAN_ANCHOR_X = 1775.29f;
    constexpr float SLADRAN_ANCHOR_Y = 670.00f;
    constexpr float SLADRAN_ANCHOR_Z = 129.26f;
    constexpr float SLADRAN_CLICK_X  = 1775.29f;
    constexpr float SLADRAN_CLICK_Y  = 676.18f;
    constexpr float SLADRAN_CLICK_Z  = 129.34f;

    constexpr float COLOSSUS_ANCHOR_X = 1683.00f;
    constexpr float COLOSSUS_ANCHOR_Y = 743.60f;
    constexpr float COLOSSUS_ANCHOR_Z = 142.94f;
    constexpr float COLOSSUS_CLICK_X  = 1690.01f;
    constexpr float COLOSSUS_CLICK_Y  = 743.60f;
    constexpr float COLOSSUS_CLICK_Z  = 142.94f;

    constexpr float MOORABI_ANCHOR_X = 1772.22f;
    constexpr float MOORABI_ANCHOR_Y = 804.96f;
    constexpr float MOORABI_ANCHOR_Z = 129.34f;

    // The GOs' own positions, for the tests that re-derive the click geometry
    // (worst-case reach = d + radius must stay inside DC_EVENT_GO_USE_RANGE).
    constexpr float ALTAR_SLADRAN_GO_X  = 1775.29f;
    constexpr float ALTAR_SLADRAN_GO_Y  = 679.68f;
    constexpr float ALTAR_COLOSSUS_GO_X = 1693.51f;
    constexpr float ALTAR_COLOSSUS_GO_Y = 743.595f;

    // Measured pad radii at the two click points — the largest disc around each
    // where every sample is still walkable mesh. The MoveTo radius must stay
    // under these or the bot can turn off the rim into the hole.
    constexpr float SLADRAN_CLICK_PAD  = 1.40f;
    constexpr float COLOSSUS_CLICK_PAD = 1.45f;

    constexpr float ALTAR_SEARCH  = 12.0f;
    constexpr float STATUE_SEARCH = 120.0f;
    constexpr float ALTAR_ARRIVE  = 6.0f;
    constexpr float CLICK_RADIUS  = 1.25f;
    constexpr uint32 ALTAR_TIMEOUT = 60000;

    // --- the Drakkari Colossus's Living Mojo ring -------------------------
    //
    // The five mojos the boss summons around himself (boss_drakkari_colossus.cpp
    // mojoPosition[]) and the merge point they charge on ACTION_MERGE.
    constexpr float MOJO_RING_X[5] = { 1663.10f, 1669.97f, 1680.70f, 1680.70f, 1670.40f };
    constexpr float MOJO_RING_Y[5] = {  743.60f,  753.70f,  750.70f,  737.10f,  733.50f };

    constexpr float COLOSSUS_X = 1672.96f;
    constexpr float COLOSSUS_Y = 743.49f;
    constexpr float COLOSSUS_Z = 142.94f;   // mesh under the (143.34) spawn

    // The four PRE-PLACED trash mojos of the west corridor (guids 127076-127079).
    // They are not TempSummons, so they aggro, fight, and inform NOBODY — hitting
    // one does not start the encounter. MOJO_SEARCH exists to keep them out.
    constexpr float MOJO_TRASH_X[4] = { 1634.21f, 1634.25f, 1624.94f, 1580.78f };
    constexpr float MOJO_TRASH_Y[4] = {  760.22f,  750.15f,  762.23f,  726.10f };

    constexpr float MOJO_SEARCH    = 20.0f;
    constexpr float COLOSSUS_SCAN  = 40.0f;
    constexpr uint32 MOJO_TIMEOUT  = 120000;

    // --- Eck's pool (heroic) ---------------------------------------------
    //
    // Centroid of the FORMATION trio 127201/127202/127203 — the only three of the
    // six Ruins Dwellers whose deaths summon Eck. Z is the WATER SHEET above their
    // 107.28 spawns, not the spawns themselves.
    constexpr float POOL_X      = 1646.40f;
    constexpr float POOL_Y      = 938.85f;
    constexpr float POOL_Z      = 108.22f;
    constexpr float POOL_RADIUS = 15.0f;
    constexpr float POOL_ZBAND  = 6.0f;
    constexpr float POOL_ARRIVE = 18.0f;
    constexpr uint32 POOL_TIMEOUT = 300000;

    // The six Ruins Dweller spawns. The first three are the formation (leader
    // 127203, groupAI 3); the last three are ungrouped and gate NOTHING — killing
    // them is three pointless elite fights and no Eck.
    constexpr float DWELLER_GATING_X[3]   = { 1651.26f, 1643.20f, 1644.73f };
    constexpr float DWELLER_GATING_Y[3]   = {  936.455f, 943.617f, 936.472f };
    constexpr float DWELLER_GATING_Z[3]   = {  107.277f, 107.276f, 107.288f };
    constexpr float DWELLER_UNGROUPED_X[3] = { 1708.48f, 1701.66f, 1717.30f };
    constexpr float DWELLER_UNGROUPED_Y[3] = {  926.962f, 951.026f, 935.615f };
    constexpr float DWELLER_UNGROUPED_Z[3] = {  116.094f, 116.536f, 117.105f };

    // Eck's HOME (boss_eck.cpp EckHomePosition), on the mesh: the water sheet at
    // 108.00 rather than the 107.205 the script names. NEVER his summon point
    // (1624.70, 891.43, 95.08) — no poly within 5yd of it.
    constexpr float ECK_X = 1642.712f;
    constexpr float ECK_Y = 934.646f;
    constexpr float ECK_Z = 108.00f;

    // --- the bridge crossing ---------------------------------------------
    //
    // The checkpoint is on comp#0, the landing on comp#1, and there is no walkable
    // route between them at any Z. Both sit on the causeway CENTRELINES, back from
    // the tips: the tips themselves measure 0.00 and 0.25yd of pad, which five
    // bots cannot arrive on.
    constexpr float CROSS_CHECK_X = 1746.00f;
    constexpr float CROSS_CHECK_Y = 744.00f;
    constexpr float CROSS_CHECK_Z = 119.10f;
    constexpr float CROSS_LAND_X  = 1802.00f;
    constexpr float CROSS_LAND_Y  = 743.50f;
    constexpr float CROSS_LAND_Z  = 119.58f;
    constexpr float CROSS_RADIUS  = 8.0f;
    constexpr float CROSS_ARRIVE  = 5.0f;
    constexpr uint32 BRIDGE_TIMEOUT = 60000;

    // The two mesh gaps the crossing spans, for the tests: the west causeway ends
    // at x 1753.50 and the east one begins at x 1796.50, with a 7-poly DEAD
    // island (no links in any direction) between them.
    constexpr float WEST_TIP_X   = 1753.50f;
    constexpr float EAST_TIP_X   = 1796.50f;

    // --- the rhino stampede, which stands on the Gal'darah approach -------
    //
    // Both rhinos spawn ON THE CAUSEWAY between the teleport landing and the
    // boss, so the party cannot reach Gal'darah without walking through them.
    //
    // 29931 is the loaded one: vehicle_template_accessory seats THREE Drakkari
    // Raiders (29982) on it, and its SmartAI runs waypoint path 1272070, fires
    // Do Action 150/151/152 at point 3, and each raider answers with "Exit
    // vehicle + Set Home Position". Three ordinary trash mobs are then loose on
    // the approach, and — instanced creatures never leash — anything they tag
    // stays tagged from wherever they end up. That is the standoff combat that
    // pinned the tank outside engage range in tr-20260830-195435-2 / -6.
    //
    // 29838 is the charging one (Charge 55530 at 5-40yd, Deafening Roar); it
    // killed tr-20260830-195435-7 outright.
    constexpr uint32 RHINO_LOADED   = 29931;
    constexpr uint32 RHINO_CHARGING = 29838;
    constexpr uint32 RAIDER         = 29982;
    constexpr uint32 RAIDER_SEATS   = 3;

    constexpr float RHINO_LOADED_X   = 1865.06f;
    constexpr float RHINO_LOADED_Y   = 742.78f;
    constexpr float RHINO_CHARGING_X = 1887.56f;
    constexpr float RHINO_CHARGING_Y = 742.88f;

    // --- Gal'darah's sealed arena ----------------------------------------
    constexpr float MUSTER_X = 1858.00f;
    constexpr float MUSTER_Y = 743.60f;
    constexpr float MUSTER_Z = 136.23f;

    // Mojo Puddle, the only PERSISTENT_AREA_AURA on the map.
    constexpr uint32 SPELL_MOJO_PUDDLE = 55627;

    // Clear-order keys — one contiguous scale so the five hand-authored
    // objectives have integer slots between the bosses.
    constexpr int32 ORDER_SLADRAN        = 1;
    constexpr int32 ORDER_ALTAR_SLADRAN  = 2;
    constexpr int32 ORDER_COLOSSUS       = 3;
    constexpr int32 ORDER_ALTAR_COLOSSUS = 4;
    constexpr int32 ORDER_MOORABI        = 5;
    constexpr int32 ORDER_ALTAR_MOORABI  = 6;
    constexpr int32 ORDER_ECK_POOL       = 7;
    constexpr int32 ORDER_ECK            = 8;
    constexpr int32 ORDER_BRIDGE         = 9;
    constexpr int32 ORDER_GALDARAH       = 10;

    // Event ids on this map.
    constexpr uint32 EVENT_ALTAR_SLADRAN  = 1;
    constexpr uint32 EVENT_COLOSSUS_MOJO  = 2;
    constexpr uint32 EVENT_ALTAR_COLOSSUS = 3;
    constexpr uint32 EVENT_ALTAR_MOORABI  = 4;
    constexpr uint32 EVENT_ECK_POOL       = 5;
    constexpr uint32 EVENT_BRIDGE         = 6;
}

void RegisterGundrakEvents(std::vector<DungeonEvent>& out);

// --- Blackwing Lair (map 469) ---------------------------------------------
// The numbers Razorgore's two halves must agree on. The declarative half (the
// event row and its activation predicate) is BlackwingLairEvents.cpp; the
// controller is Overrides/BlackwingLairDriver.cpp; the arithmetic is
// Util/DcRazorgoreDecision.h.
namespace DcBlackwingLair
{
    constexpr uint32 MAP_ID = 469;

    constexpr uint32 NPC_RAZORGORE = 12435;

    // The Orb of Domination and the thirty Black Dragon Eggs. Both are GOOBERs
    // (type 10) and both are WORLD SPAWNS, present from map load — so the driver
    // can grid-scan for them before anything has been engaged, unlike a
    // TempSummon-based encounter.
    constexpr uint32 GO_ORB_OF_DOMINATION = 177808;
    constexpr uint32 GO_BLACK_DRAGON_EGG  = 177807;
    constexpr uint32 EGG_COUNT            = 30;

    // instance_blackwing_lair's DATA_EGG_EVENT. The ONE readable progress seam
    // this encounter offers: it returns NOT_STARTED / IN_PROGRESS / SPECIAL /
    // DONE. There is deliberately no egg COUNT accessor in the instance script,
    // so "how many are left" is answered by scanning the eggs themselves.
    constexpr uint32 DATA_EGG_EVENT = 2;

    // Mind control (19832, 90s) and the charmer's lockout (23958, 60s). The
    // driver never runs a timer against either — it reads the charm and the aura
    // straight off the units, which is authoritative through a wipe, a despawn
    // and a phase flip alike.
    constexpr uint32 SPELL_MIND_CONTROL   = 19832;
    constexpr uint32 SPELL_MIND_EXHAUSTION = 23958;
    constexpr uint32 SPELL_DESTROY_EGG    = 19873;

    // The orb, and where the runner stands to take it: on the upper ledge at
    // z 413, above and clear of all eight add-spawn positions (all z 407).
    constexpr float ORB_X = -7614.83f;
    constexpr float ORB_Y = -1026.62f;
    constexpr float ORB_Z = 413.38f;

    // "Standing at the orb" — the runner clicks from here, and holds here for
    // its whole window (possession roots the charmer anyway; this only keeps it
    // from being walked off before the click).
    constexpr float ORB_STATION_RADIUS = 4.0f;

    // THE THREE MOBS ON THE ORB PLATFORM. Grethok the Controller (a level 62
    // elite caster) and two Blackwing Guardsmen stand 6-10yd from the orb, on the
    // ledge, from map load — they are not part of the encounter and nothing in
    // the instance script removes them.
    //
    // They are the reason this block exists. The runner's walk is 78yd and ends
    // ON TOP of them; the first live run sent one DPS up alone into three elites
    // and it died on the ramp without ever reaching the orb.
    //
    // GRETHOK IS THE PULL. He is not an encounter and carries no kill credit, but
    // he is what a human raid pulls to start this fight, and DC now treats him
    // that way: the roster patch makes him boss #0 of the map, so the tank brings
    // the raid to him with the ordinary pipeline (advance, muster, standoff,
    // engage) instead of forty bots racing each other up the ramp. Nothing on the
    // platform — no election, no staging, no click — happens before that pull.
    constexpr uint32 NPC_GRETHOK_THE_CONTROLLER = 12557;
    constexpr uint32 NPC_BLACKWING_GUARDSMAN    = 14456;

    // How far from the orb to look for them. The furthest of the three spawns
    // 10.0yd out; 25 covers that with room for the pull dragging one a few yards
    // without the scan reaching down to the floor pack (the nearest floor spawn
    // is 33yd out and a tier below).
    constexpr float ORB_GUARD_RADIUS = 25.0f;

    // GRETHOK'S SPAWN — and the roster anchor DC pulls him from.
    //
    // He is not a DungeonEncounter and the DBC has never heard of him, but for a
    // clear he is the boss of this room: he is what the tank pulls, and pulling
    // him starts the encounter (his formation holds Razorgore). So the roster
    // patch adds him as boss #0 of map 469 and the ORDINARY pull pipeline —
    // advance, raid muster, boss standoff, engage — brings the whole raid up to
    // him as one body. See RegisterBlackwingLairRoster.
    //
    // Read straight off `creature` (guid 84389): the two Blackwing Guardsmen
    // (84390 / 84391) stand 5-7yd either side of him at the same height.
    constexpr float GRETHOK_X = -7618.29f;
    constexpr float GRETHOK_Y = -1021.42f;
    constexpr float GRETHOK_Z = 413.56f;

    // The leader has to be in the chamber before the raid is sent up the ramp.
    // The event's own due range (200yd) is the whole approach; this is the room.
    constexpr float GUARD_CLEAR_RANGE = 100.0f;

    // WHERE THE REST OF THE RAID FIGHTS — at the foot of the orb platform, one
    // step toward the middle of the room, on the floor at z 408.87.
    //
    // The runner is rooted on the ledge for ninety seconds at a time and cannot
    // defend itself; if the raid fights wherever the pull left it, the adds that
    // pick the runner arrive unopposed and the mind control ends with its death.
    // So the raid camps between the room and the ledge.
    //
    // NOT on the ledge itself, which is the shape the first live run had and the
    // reason this exists: the platform is small (the navmesh column 5yd out from
    // the orb already has no 413 surface over it), so a raid standing on it has
    // nowhere to spread and nothing between it and the floor the adds cross.
    //
    // Column-probed against the live 469 mmtile: exactly one walkable surface
    // under it, z 408.87. It is 11.4yd (2D) / 12.3yd (3D) from the orb — inside
    // every healer's range of the runner, a couple of steps for a melee bot that
    // has to peel something off the ledge — and 46-81yd from all eight of the
    // instance's add-spawn positions, so every wave has to cross the room to
    // reach it rather than arriving on top of it.
    constexpr float CAMP_X = -7608.30f;
    constexpr float CAMP_Y = -1036.00f;
    constexpr float CAMP_Z = 408.87f;

    // How far off the camp anyone may drift before they are walked back. ONE
    // number for the whole raid — the tank used to get a longer tier of its own
    // (12 for members, 20 for the leader) and both were too tight live: bots
    // were being walked off adds they had legitimately stepped onto, which is
    // the one thing this rung must never do.
    //
    // 30 still keeps the raid in the runner's half of the chamber — the camp is
    // 12.3yd from the orb and the nearest add spawn is 46yd from the camp, so a
    // bot at the end of its leash is still between the room and the ledge — and
    // it is still a leash: "do not chase across the room" is the whole point of
    // a camp in a fight whose adds all come to you.
    //
    // A leash this wide COVERS THE ORB LEDGE (Grethok's own spawn is 18yd from
    // the camp centre), so a raid that fought the guard pull up there reads as in
    // position and is left alone. That is intended: the first add wave pulls it
    // down anyway, and nothing HOLDS it up there.
    //
    // THE COST, deliberately accepted: a bot at the FAR edge is 42yd from the
    // runner, past a healer's 40yd range, and the runner is rooted and cannot
    // come to it. The raid is one body in practice — it fights what walks in,
    // near the camp centre — so this is the far corner of the leash, not where
    // the fight happens. If runner deaths come back with the healers alive and
    // out of range, this number is the first suspect.
    constexpr float CAMP_LEASH = 30.0f;

    // How far INSIDE the leash a drifted bot is walked back to.
    //
    // The camp is a LEASH, not a point: the walk-back aims at the near EDGE of
    // it, never at the centre. Aiming at the centre is what the first live run
    // looked like — a bot crosses the whole camp inward, the fight pushes it
    // back out, and it crosses again, forever, every bot out of phase with the
    // others. Landing just inside the boundary makes the correction a step
    // instead of a lap.
    //
    // The margin IS the hysteresis, so it is neither zero nor a hair: land
    // exactly on the boundary and the next yard of drift re-arms the rung. It
    // must stay well under CAMP_LEASH — a margin that reached the leash would
    // put the hold point back at the centre and bring the lap back with it.
    constexpr float CAMP_HOLD_MARGIN = 4.0f;

    // Covers the whole chamber from anywhere in it: the eggs span ~80yd of x by
    // ~93yd of y across two tiers, and the orb sits 78yd from the boss's spawn.
    constexpr float ROOM_SCAN = 150.0f;

    // Proximity gate for the event's activation predicate — the leader must
    // actually be at the encounter, not corpse-running the entrance ramp.
    constexpr float EVENT_DUE_RANGE = 200.0f;

    // The event row and the hook that drives it. Hook ids are ONE FLAT SPACE
    // across every dungeon (see ObjectiveHookRegistry::AddHook); 15-19 are the
    // Violet Hold's.
    constexpr uint32 EVENT_RAZORGORE_ORB = 1;
    constexpr uint32 HOOK_RAZORGORE_ORB  = 20;

    // The orb platform, as the two facts both halves of the encounter ask about:
    // is any of Grethok / the two Blackwing Guardsmen still standing, and has the
    // tank PULLED them yet.
    //
    // `engaged` is what releases the orb runner. The click used to wait on the
    // platform being empty; it now waits on the pull, because the pull is the
    // only thing that has to happen first — Grethok's formation drags Razorgore
    // into the fight on first contact (groupAI 7), so from the tag onward every
    // second without the mind control is a second the raid spends damaging a boss
    // whose phase-1 death wipes it.
    //
    // Scanned from the ORB rather than from `bot`, so the answer does not change
    // with where the asker happens to be. Shared rather than duplicated because
    // TWO rungs act on it and a disagreement between them is a runner that clicks
    // an orb the leader thinks it is holding: the leader's driver
    // (Overrides/BlackwingLairDriver.cpp) uses it to decide the step, and the
    // runner's own rung (Action/DcRazorgoreActions.cpp) uses it to decide whether
    // to click on arrival or hold station.
    struct OrbGuardState
    {
        bool alive{false};    // at least one of the three is still up
        bool engaged{false};  // ...and somebody has it in combat
    };
    OrbGuardState OrbGuards(Player* bot);

    // IS THE EGG RUN HOLDING THE RAID WHERE IT STANDS?
    //
    // True on map 469 for every member (leader included) from the tick the pull
    // on Grethok lands until a tick or two after the last egg breaks — the same
    // window the camp rung arms on, asked by the rungs that would otherwise WALK
    // the raid somewhere else.
    //
    // It exists because of what the ordinary pipeline does the moment Grethok's
    // anchor clears: the next boss becomes Razorgore, and Razorgore — possessed,
    // being driven egg to egg by our own runner — is a MOVING anchor. The advance
    // then does exactly what it does for any wandering boss: it re-paths at his
    // live position every few seconds and holds at the engage range, and the
    // followers, who follow the tank, come with it. Measured on the first live
    // run of the egg phase (23:14:41 and 23:15:47, 45yd and 42yd splines issued
    // at the possessed boss, interleaved with "within engage range of Razorgore
    // the Untamed (25yd/27yd/11yd/23yd/17yd) -> holding for at-boss"): the raid
    // toured the chamber behind the boss at a fixed standoff instead of holding
    // the camp at the foot of the ledge, which is the one thing phase 1 asks of
    // it — the runner is rooted on that ledge for ninety seconds at a time.
    //
    // So during phase 1 nothing in the approach family may drive: not the route,
    // not the boss standoff, not the muster, not the engage. The raid's position
    // for the whole egg run belongs to the camp rung
    // (DungeonClearRazorgoreCampTrigger), which holds inside the leash and walks
    // a drifted bot back to its near edge.
    //
    // Bounded by construction, so it can never wedge a run: the stamp behind it
    // goes stale within ~3s of the driver stopping, by every exit the encounter
    // has — the last egg, a wipe, the event's own 10-minute timeout (it is
    // Optional and skips), `dc pause`, a dead leader.
    bool EggRunHoldsTheRaid(Player* bot);

    // IS THIS BOT HOLDING THE POSSESSION RIGHT NOW?
    //
    // Read off the bot's OWN unit fields — UNIT_FIELD_CHARM, resolved and checked
    // against Razorgore's entry — and off nothing else. That independence is the
    // whole point of it.
    //
    // The runner's rung and the raid's camp both hang off DcRunState::
    // razorDrivingMs, a stamp the LEADER refreshes on every tick its driver runs.
    // That is right for positioning (a stamp that goes stale releases the raid,
    // which is the safe direction) and WRONG for the channel: 19832 is a channel
    // on the runner's own body, and the instant the runner's rung goes inert the
    // bot's rotation comes back — a swing, a wand shot, a step out of a cone, a
    // health potion at ACTION_EMERGENCY — and ends it. Razorgore is then freed
    // mid-run, the runner eats a 60s lockout, and the raid is left holding a boss
    // it must not kill. Every reason the leader's stamp can go stale (the leader
    // dies, walks out of EVENT_DUE_RANGE, the event stops being due for a tick,
    // `dc pause`) is a reason the possession is STILL UP and still needs guarding.
    //
    // So the possession guards itself: the charm is a fact about this bot, no
    // cross-bot signal is involved, and both the rung that owns the tick and the
    // multiplier that mutes everything else read it here.
    //
    // Free everywhere else — the map compare rejects before the field is read.
    bool HoldsThePossession(Player* bot);

    // --- Vaelastrasz the Corrupt (boss 2) ---------------------------------
    //
    // The only boss on this map — and one of the very few anywhere — that a raid
    // does not PULL. He lies at 30% health, faction 35 (friendly to everyone),
    // REACT_PASSIVE and stand-state DEAD, offering a gossip; the raid starts the
    // encounter by TALKING to him, sits through ~63s of scripted RP, and is then
    // attacked by him. Nothing DC does to a hostile boss applies: he cannot be
    // tagged, he cannot be pulled, and walking the tank into melee does nothing.
    constexpr uint32 NPC_VAELASTRASZ = 13020;

    // His spawn (creature guid 84512, map 469). Also the roster anchor the
    // auto-derived boss list already carries — repeated here only as the
    // proximity gate for the rouse predicate, so a leader corpse-running the
    // entrance never reads as "at Vaelastrasz".
    constexpr float VAEL_X = -7483.79f;
    constexpr float VAEL_Y = -1015.99f;
    constexpr float VAEL_Z = 408.652f;

    // THE GOSSIP CHAIN, and why the option index is 0 at every level.
    //
    // creature_template.gossip_menu_id is 21333; its lone option opens 21334,
    // whose lone option is the one boss_vaelastrasz::sGossipSelect answers
    // (`sender == 21334 && action == 0`, where the core hands sGossipSelect the
    // MENU id as `sender` and the selected list index as `action` — see
    // WorldSession::HandleGossipSelectOptionOpcode). 21334's option in turn opens
    // 21332, which is pure flavour and closes.
    //
    // DungeonEventExecutor::SelectGossip walks exactly that shape by itself: it
    // selects the authored option on the first menu and then keeps selecting
    // option 0 of whatever submenu opens until the menu closes. So ONE Gossip
    // step with option 0 fires BeginSpeech, and there is nothing here to author
    // per level.
    constexpr int32 VAEL_GOSSIP_OPTION = 0;

    // How far out the leader may be and still have the rouse read due. Generous
    // enough to cover the boss standoff the approach parks the tank at (the
    // gossip step walks the last yards in itself), tight enough that the event is
    // never due from the Razorgore chamber ~140yd back.
    constexpr float VAEL_DUE_RANGE = 80.0f;

    // Grid-scan radius for Vaelastrasz himself. He never moves before the pull,
    // so this only has to cover his own room from the standoff.
    constexpr float VAEL_SCAN = 100.0f;

    // The event row. Ids are per-map, so this is 2 alongside Razorgore's 1.
    constexpr uint32 EVENT_VAELASTRASZ_ROUSE = 2;

    // WHERE VAELASTRASZ IS IN HIS OWN OPENING, as the two facts both halves of
    // the rouse ask about. ONE scan for both, for the same reason OrbGuards does
    // it: the engage rung and the event predicate must never disagree about
    // whether he has turned, and a second sweep of the room every tick buys
    // nothing.
    //
    //   * `offersRouse` — he still bears UNIT_NPC_FLAG_GOSSIP, i.e. nobody has
    //     talked to him yet. BeginSpeech strips the flag as its first act, so
    //     this is a one-way latch and the ONLY safe gate on "may I gossip him":
    //     a second select after the RP has started reaches no script and simply
    //     drops.
    //   * `dormant` — he is not hostile to us. The faction flip
    //     (FACTION_FRIENDLY -> FACTION_DRAGONFLIGHT_BLACK) is the last act of the
    //     intro, on the same tick he AttackStarts the bot that talked to him, so
    //     this is exactly "the fight has not begun" and it covers BOTH the wait
    //     before the gossip and the ~63s of RP after it. Deliberately a pure
    //     faction reaction rather than IsValidAttackTarget: he is also
    //     NOT_SELECTABLE for most of the intro and briefly selectable again at
    //     the end of it, and a gate that flickers would hand the engage rung a
    //     tick in the middle of the speech.
    //
    // Free everywhere else — the map compare rejects before anything is scanned.
    struct VaelastraszState
    {
        bool present{false};     // alive, in his room
        bool offersRouse{false}; // ...and still waiting to be talked to
        bool dormant{false};     // ...and has not turned on the raid yet
    };
    VaelastraszState Vaelastrasz(Player* bot);

    // --- the Suppression Rooms (the Vaelastrasz -> Broodlord transit) ------
    //
    // The 375yd of gauntlet between Vaelastrasz's chamber and Broodlord
    // Lashlayer, and the one leg on this map that the ordinary clear cannot
    // cross AT ALL. Everything in this block exists because of one arithmetic
    // fact and one code fact:
    //
    //   * 160 Corrupted Whelps live in the two rooms on a THIRTY-SECOND
    //     respawn — a spawn rate of 5.3/s, ~3.3/s for the hundred within 20yd
    //     of the route line. No DPS closes that; the room's population is a
    //     fixed point the party cannot move.
    //   * DcCombatFlag::MayDrive is false for as long as anything in the party
    //     is engaged, and Advance is registered ONLY in the non-combat engine.
    //     So with the whelps up the clear has no driver at all: it is not slow,
    //     it is stopped.
    //
    // Plus a third that decides how long "stopped" lasts: 38 Suppression Devices
    // (GO 179784) each pulse spell 22247 in a 20yd bubble for -80% movement
    // speed, and 95% of the route lies inside at least one of them. 54 seconds
    // of walking becomes 268.
    //
    // THE ANSWER IS NOT TO CLEAR IT. This leg is a TRANSIT: one body, brakes
    // off, crossed under fire. Four parts, in the order they ship:
    //
    //   A  the authored route below (data; the cursor the other three share)
    //   B  DcNeverTargetRegistry rows on 14022-14025, so the clear's pickers
    //      stop walking the tank BACK into the room for the nearest whelp
    //   D  DungeonClearTransitPack{Trigger,Action} — a moving camp that keeps
    //      the raid inside one leash around the leader's route cursor
    //   C  the transit driver (event EVENT_SUPPRESSION_TRANSIT / hook
    //      HOOK_SUPPRESSION_TRANSIT, Overrides/BlackwingLairDriver.cpp), the
    //      only thing on this map that moves the leader while it is engaged
    //
    // What the transit does NOT skip is the ELITES. Thirteen of the twenty
    // Death Talon Hatchers / Blackwing Taskmasters are within 25yd of the route,
    // they respawn on a TEN-MINUTE timer, and six Taskmasters stand on the only
    // ramp between the two rooms. Killing those is real progress and the driver
    // holds for them.

    // Broodlord Lashlayer — boss 3, and the route's owner in
    // DungeonClearRouteRegistry.
    constexpr uint32 NPC_BROODLORD_LASHLAYER = 12017;

    // ...and his index in instance_blackwing_lair's own BWLEncounter enum
    // (DATA_BROODLORD_LASHLAYER = 2), which is what GetBossState is keyed on.
    // Read rather than derived: the instance's index space is the script's, not
    // the roster's, and the two agree here only by coincidence.
    constexpr uint32 BROODLORD_ENCOUNTER_INDEX = 2;

    // THE DRAKE HALL, and why its four bosses need naming here at all.
    //
    // Firemaw stands at (-7520.2, -1025.8, 449.1) — **24.7yd STRAIGHT ABOVE
    // approach anchor 7** at (-7520.5, -1023.3, 424.5), which is 2.5yd from him
    // in plan view. The hall the raid walks from Vaelastrasz to the staging point
    // runs directly under his room, and a level-63 boss's aggro radius is about
    // 25yd, measured in 3D. In tp-20260828-121941-1 that tripped in four of five
    // runs: one bot inside the radius (21.8yd) aggros him, `BossAI::_JustEngagedWith`
    // calls `DoZoneInCombat()` — every player in the map within 250yd, no LOS and
    // no reachability test — and all twenty-five bots enter combat with a boss two
    // floors up while he never leaves his spawn. The raid then spends the rest of
    // the run split across two floors, and some of it walks up through the ceiling
    // to reach him (a PathGenerator with no navmesh route still hands a player a
    // straight line — see the ac-pathgenerator note).
    //
    // Nothing here can stop him being flagged; the exclusion rows in
    // DcTargetExclusionRegistry stop the raid ACTING on it.
    constexpr uint32 NPC_FIREMAW    = 11983;
    constexpr uint32 NPC_EBONROC    = 14601;
    constexpr uint32 NPC_FLAMEGOR   = 11981;
    constexpr uint32 NPC_CHROMAGGUS = 14020;

    // The four Corrupted Whelps (red / green / blue / bronze). Level 60 normals,
    // 4 578 HP, no AI and no script, MovementType 1 — they aggro on proximity,
    // individually, and they are back thirty seconds after they die. The
    // DcNeverTargetRegistry rows are keyed on these; nothing else does.
    constexpr uint32 NPC_CORRUPTED_RED_WHELP    = 14022;
    constexpr uint32 NPC_CORRUPTED_GREEN_WHELP  = 14023;
    constexpr uint32 NPC_CORRUPTED_BLUE_WHELP   = 14024;
    constexpr uint32 NPC_CORRUPTED_BRONZE_WHELP = 14025;

    // The two elites the transit stands and fights: Blackwing Taskmaster (9
    // spawns, six of them stacked on the ramp at (-7711,-1070,445)) and Death
    // Talon Hatcher (11 spawns, scattered through both rooms). 600s respawn
    // each, so unlike the whelps a kill here is progress that stays bought.
    constexpr uint32 NPC_BLACKWING_TASKMASTER = 12458;
    constexpr uint32 NPC_DEATH_TALON_HATCHER  = 12468;

    // The Suppression Device: GameObject 179784, `type 6` (TRAP), 38 spawns.
    // go_suppression_device casts 22247 every 5s while it is GO_STATE_READY —
    // 20yd radius, -80% move speed, -80% cast speed, 6s duration, i.e. permanent
    // while you stand in it.
    //
    // DISARMING IS NOT OURS. mod-playerbots already owns it
    // (BwlSuppressionDeviceTrigger / BwlTurnOffSuppressionDeviceAction, Ai/Raid/
    // BWL/), turning off any READY device within 15yd at ACTION_RAID (60) — and
    // this box runs `AiPlayerbot.BotCheats = "food,taxi,raid"`, so every bot
    // qualifies, not just rogues. A device turned off that way NEVER re-arms
    // (nothing in the bot path calls DoAction(ACTION_DISARMED), so
    // EVENT_SUPPRESSION_RESET is never scheduled, and a bare SetGoState sets no
    // cooldown for autoCloseTime to fire off). One pass is enough.
    //
    // All the transit owes that rung is a TICK: 17 of the 19 route-adjacent
    // devices are within 10yd of the route line, so walking the route reaches
    // them with no detour — the driver only has to stop walking for one tick
    // when an armed one is close enough for the disarm to fire.
    constexpr uint32 GO_SUPPRESSION_DEVICE = 179784;

    // THE STAGING POINT — the last genuinely clean ground before the gauntlet,
    // at the head of the climb into the lower room.
    //
    // Measured against map 469's spawn table: nearest whelp 40.8yd, nearest
    // device 50.1yd, ZERO whelps within 30yd. The ordinary clear delivers the
    // raid here by itself (it is out of combat up to this point), so the
    // transit's first step is a short intra-room hop, not a haul.
    constexpr float TRANSIT_STAGE_X = -7630.9f;
    constexpr float TRANSIT_STAGE_Y = -915.5f;
    constexpr float TRANSIT_STAGE_Z = 437.3f;

    // WHERE THE STAGING POINT SITS IN THE AUTHORED ROW, and why that is not zero.
    //
    // The registry row for Broodlord is two halves (see RegisterBlackwingLairRoute):
    // anchors 0-19 are the un-crossed approach from Vaelastrasz's chamber, which
    // exists so the ordinary clear has a polyline to walk and a cursor to project
    // onto, and 20-39 are the crossing itself. The transit driver must NOT see the
    // approach — its kernel keys the gather gate on `cursorIndex == 0` meaning "at
    // staging", and its arm pins the cursor to 0 — so BwlTransitRoute slices the
    // row here and hands the driver a route whose anchor 0 is the staging point,
    // exactly as it was before the approach was authored.
    //
    // Certified by t/TestBlackwingLairSuppressionRouteProbe: the row's anchor at
    // this index must BE the staging point, or the driver runs its cursor down the
    // wrong half of the route.
    constexpr std::size_t TRANSIT_STAGE_ANCHOR_INDEX = 20;

    // THE BROODLORD STANDOFF — where the transit ends and the ordinary raid
    // pipeline (muster, standoff, engage) takes back over. Nearest whelp 27.5yd,
    // nearest device 21.7yd; clean ground, on the upper room's floor.
    constexpr float TRANSIT_END_X = -7573.8f;
    constexpr float TRANSIT_END_Y = -1033.5f;
    constexpr float TRANSIT_END_Z = 449.3f;

    // How close the leader has to get before the transit is over. Also the
    // predicate's own OFF switch: the event is due while the leader is inside the
    // corridor and NOT yet here, so arriving simply stops it being due. There is
    // no completion latch to reset — a leader shoved back into the gauntlet
    // re-arms it, which is the correct answer.
    constexpr float TRANSIT_END_RADIUS = 10.0f;

    // THE CORRIDOR — the axis-aligned box that is the two suppression rooms plus
    // the approach and NOTHING else on the map. The transit's activation
    // predicate is gated on the leader being inside it, which is what keeps a
    // rung registered on every bot's combat engine inert for the other seven
    // encounters of this raid.
    //
    // Derived from the route: x spans the climb (-7631) to the standoff (-7574),
    // y spans the upper room's far wall (-1130) to the head of the climb (-905),
    // z spans the lower room's floor (437) to the upper room's (449) with a
    // couple of yards of slack either side. Vaelastrasz's chamber (-7484,-1016,
    // z 409) is outside it on x, y AND z; the Razorgore chamber (-7615,-1027,
    // z 409-413) is outside it on z.
    constexpr float TRANSIT_BOX_MIN_X = -7720.0f;
    constexpr float TRANSIT_BOX_MAX_X = -7570.0f;
    constexpr float TRANSIT_BOX_MIN_Y = -1130.0f;
    constexpr float TRANSIT_BOX_MAX_Y = -905.0f;
    constexpr float TRANSIT_BOX_MIN_Z = 430.0f;
    constexpr float TRANSIT_BOX_MAX_Z = 455.0f;

    // Is `bot` standing inside the suppression corridor right now? One bbox test,
    // and the transit's cheapest real gate after the map compare.
    bool InTransitCorridor(Player* bot);

    // How far INSIDE the pack leash a drifted member is walked back to.
    //
    // The margin IS the hysteresis, so it is neither zero nor a hair: land exactly
    // on the boundary and the next yard of drift re-arms the rung. It must stay
    // well under the leash — a margin that reached it would put the hold point
    // back at the cursor itself and bring the cross-the-whole-pack lap back with
    // it. Four yards is the Razorgore camp's number, which is authored against the
    // same shape (CAMP_HOLD_MARGIN) and survived a live raid.
    //
    // Not a setting: it is a property of the hold, not of the leg, and it only
    // means anything relative to TransitPackLeash — which IS a setting, and whose
    // clamp floor (10) keeps this comfortably inside it.
    constexpr float TRANSIT_PACK_HOLD_MARGIN = 4.0f;

    // The pack rung's ARRIVAL leash — how close to its hold point a walking member
    // has to get before the rung hands the tick back.
    //
    // Named rather than inlined at the call site because the gather gate has to
    // read it. A member parks anywhere in (leash - margin, leash - margin + this]
    // of the cursor: the trigger keeps firing while it is outside the hold radius,
    // but the action refuses to move it once it is within this of the point it
    // aims at. That band IS where the raid stands, so a gate that asks for a
    // radius under its top edge is asking for a formation the rung will never
    // produce — see TransitGatherRadius's floor in the driver, and
    // [[dc-moving-camp-rung-hysteresis]] for the first half of the same lesson.
    constexpr float TRANSIT_PACK_ARRIVE_LEASH = 2.0f;

    // How far the mesh may move the pack rung's chord hold point before the chord
    // is judged to be describing somewhere the party cannot stand, and the rung
    // rides the authored polyline instead (DcTransit::HoldPoint).
    //
    // TIGHT ON PURPOSE, and tighter than it looks. NavmeshSnap searches a FIXED
    // 10yd vertical extent ([[dc-boss-anchor-snap-vertical-extent]]), so a chord
    // that has merely sunk under a ramp still snaps — to the ramp, a yard or two
    // up — and that is a GOOD outcome: measured on the real mesh, the north-arm
    // follower's chord snaps 1.40yd to (-7629.8, -931.7, 441.1), which routes
    // 11.7yd and arrives. It is the chords the snap CANNOT rescue that need the
    // polyline. Two yards is the line between those two populations.
    constexpr float TRANSIT_HOLD_SNAP_TOLERANCE = 2.0f;

    // Horizontal search box for that snap. Deliberately smaller than the pack
    // leash: the question is "is the chord standing on the corridor", and a wide
    // box answers "is there floor SOMEWHERE near here", which on a C-shaped ramp
    // finds the OTHER arm across the void and calls the chord good.
    constexpr float TRANSIT_HOLD_SNAP_RADIUS = 3.0f;

    // NavmeshSnap's own vertical extent, restated so the certification probe can
    // search the same box the runtime does. Not a knob — it is fixed inside
    // NavmeshSnap::Snap; named here only so the probe cannot drift from it.
    constexpr float TRANSIT_HOLD_SNAP_V_EXTENT = 10.0f;

    // Grid-scan radius for the driver's per-tick elite / device sweeps. Wide
    // enough to see every hold-worthy thing from the middle of a leg (the widest
    // hold radius is 20yd and a leg is up to 33yd), tight enough that it never
    // reaches across the room divider into the other suppression room.
    constexpr float TRANSIT_SCAN = 40.0f;

    // The event row and the hook that drives it. Event ids are per-map (1
    // Razorgore, 2 Vaelastrasz); HOOK ids are ONE FLAT SPACE across every dungeon
    // (see ObjectiveHookRegistry::AddHook) — 15-19 are the Violet Hold's and 20
    // is Razorgore's, so this is 21.
    constexpr uint32 EVENT_SUPPRESSION_TRANSIT = 3;
    constexpr uint32 HOOK_SUPPRESSION_TRANSIT  = 21;

    // THE THREE HOLD WATCHDOGS, and why they are three numbers rather than one.
    //
    // The holds are not the same KIND of wait. A device disarm is a tick or two of
    // standing still while somebody else's ACTION_RAID rung fires; a straggler
    // catching up across a 24yd leg is tens of seconds; an elite fight on this leg
    // is minutes (the live budget allows ~3 of the crossing's 4). One shared
    // watchdog would either release the disarm hold before that rung had a tick,
    // or leave the leg parked behind a wedged straggler for the whole elite
    // budget.
    //
    // None of them is a target. Each bounds ONE wait, and on expiry the driver
    // walks on and logs which hold gave up — the member it leaves behind is
    // stranded recovery's problem (relevance 42), which sits above this whole
    // ladder.
    constexpr uint32 TRANSIT_PACK_HOLD_TIMEOUT_MS   = 30000;
    constexpr uint32 TRANSIT_ELITE_HOLD_TIMEOUT_MS  = 120000;
    constexpr uint32 TRANSIT_DISARM_HOLD_TIMEOUT_MS = 1500;


    // Telemetry threshold only. How far a live combat reference has to be from
    // every member holding it before the driver's `towing N` field counts it as
    // aggro the raid is CARRYING rather than a fight the raid is IN.
    //
    // NOTHING HOLDS ON THIS. A gate that did was tried and removed: it waited at
    // the staging point, and tr-20260830-152617-2 and -5 both show the raid
    // reaching staging with the count at zero and 25 of 25 formed up on merit.
    // The count only climbs at cursor 9, the foot of the Taskmaster ramp, ~200yd
    // into the crossing — so a staging gate can never see it.
    //
    // Deliberately below DC_ENGAGEMENT_RADIUS (100yd), which is the opposite
    // question: DcCombatFlag's scan discards everything past 100yd as "a
    // reference that has outlived its geometry", and that discarded set is
    // exactly what this counts. 60yd because the crossing's own fights are inside
    // TRANSIT_SCAN (40yd) and the far packs sit 100-200yd off it.
    constexpr float TRANSIT_AGGRO_SHED_DIST = 60.0f;

    // How far from the staging point the leader may be when the transit ARMS and
    // still be asked to gather there.
    //
    // Past this the gather gate latches open immediately, and is logged: you
    // cannot gather at a point you have already walked past, and a driver that
    // insisted would march a leader standing in the upper room 300yd back through
    // the gauntlet to form up. The case is a re-arm after a partial wipe, which is
    // exactly when walking backwards is worst.
    constexpr float TRANSIT_STAGE_SKIP_DIST = 60.0f;

    // How far the leader may be from its own stored cursor before the projection
    // is believed over it (DcSuppressionTransit::ResolveCursor). Inside a working
    // crossing nothing is ever this far from the anchor it is walking to; a leader
    // that is has died and come back, and the stored index is a fact about a
    // previous attempt.
    constexpr float TRANSIT_CURSOR_RESYNC_DIST = 60.0f;

    // Throttle on the driver's per-tick telemetry line. Without the line a failed
    // run says nothing about WHICH of the four mechanisms is still biting; without
    // the throttle it says it several times a second, per bot.
    constexpr uint32 TRANSIT_TELEMETRY_MS = 3000;

    // Bound on the whole crossing, as the step's timeout.
    //
    // §6.2's live budget is FOUR MINUTES from the staging point (54s of
    // unsuppressed walking plus ~3min of elite fights), so ten minutes is a
    // ceiling on a genuinely broken run and never the binding constraint on a
    // working one. The event is Repeatable and every yielded tick re-bases the
    // step clock, so this only fires when the driver has held (returned Running)
    // continuously for the whole budget — i.e. when a hold's own watchdog has
    // failed to release, which is the one shape nothing else here can see.
    constexpr uint32 TRANSIT_TIMEOUT_MS = 600000;

    // --- Chromaggus (boss 7) — the cage, and the lever that opens it -------
    //
    // The second boss on this map a raid does not pull. Chromaggus stands behind
    // a shut portcullis (GO 179116) and boss_chromaggus's constructor holds him
    // with `SetImmuneToAll(true)` — a core hack-fix that stops him being pulled
    // through the floor from the corridor below. NOTHING clears that immunity but
    // the lever: go_chromaggus_lever's GossipHello opens the portcullis, walks him
    // out on waypoint path 140200, and hands his AI the clicker's GUID via
    // SetGUID(GUID_LEVER_USER), which is what calls SetImmuneToAll(false) and,
    // when the path ends, SetInCombatWith(the clicker).
    //
    // So the lever is the pull, and the bot that pulls it is the bot Chromaggus
    // engages. That is the tank, because a conditional event is performed by the
    // leader.
    constexpr uint32 NPC_CHROMAGGUS_CAGE_DOOR = 179116;   // GO, guid 75161
    constexpr uint32 GO_CHROMAGGUS_LEVER      = 179148;   // GO, guid 56161

    // The lever's spawn (gameobject guid 56161, map 469), on the south wall of
    // the chamber. Also the event's proximity gate.
    constexpr float CHROMA_LEVER_X = -7510.98f;
    constexpr float CHROMA_LEVER_Y = -1094.69f;
    constexpr float CHROMA_LEVER_Z = 476.555f;

    // WHERE CHROMAGGUS FIGHTS, and the roster anchor DC replaces his spawn with.
    //
    // boss_chromaggus::homePos: the script SetHomePosition()s him here the moment
    // the lever is pulled and runs waypoint path 140200 (four points, all
    // (-7488.41, -1074.58, 476.544)) to walk him out of the cage into the chamber.
    // That is the ground the encounter is actually fought on; his DB spawn
    // (-7515.34, -1029.62, 476.73) is a holding pen behind a shut portcullis.
    //
    // WHAT THIS DOES AND DOES NOT BUY. Once his grid is loaded the advance routes
    // at the LIVE creature, not at the anchor (DcAdvanceAction takes bossX/Y/Z off
    // GetLiveBoss), so this does NOT move where the tank ends up standing off —
    // that is one BossEngageRange short of the cage, which is fine: nothing shares
    // his floor within 60yd. What it does buy is the FAR approach, which routes to
    // the anchor while the grid is still streaming in, and every distance readout
    // in the panel and the diag roster. Aiming the long walk at a point sealed
    // inside a cage, directly above a corridor of z-449 trash, is the wrong-floor
    // path-cursor shape this map has already produced once (see the Firemaw note
    // above); aiming it at the open chamber the party walks through is not.
    constexpr float CHROMA_HOME_X = -7491.1587f;
    constexpr float CHROMA_HOME_Y = -1069.718f;
    constexpr float CHROMA_HOME_Z = 476.59094f;

    // How far from the LEVER the leader may be and still read the cage as due.
    // Wide enough to cover the whole chamber — the standoff the approach parks
    // the tank at is ~47yd out, by the cage door, the muster spread adds to that,
    // and the UseGO step walks the last yards in itself — and short of the drake
    // hall behind it (Flamegor is 130yd away).
    constexpr float CHROMA_DUE_RANGE = 90.0f;

    // ...and the FLOOR the leader has to be standing on to be "at the lever".
    //
    // The range gate is 2D, and 2D is a lie on this part of map 469: the whole
    // Broodlord floor sits 27yd DIRECTLY BELOW the chamber, and the Suppression
    // Rooms transit corridor passes within 36yd (2D) of the lever at z 449. The
    // muster gate already makes it impossible for the cage event to be due down
    // there — the next boss during the crossing is Broodlord, not Chromaggus —
    // but this map has produced the wrong-floor bug twice already (the Firemaw
    // approach, the drake-hall/Broodlord overlap), and a 2D radius that spans two
    // floors is exactly how. Half-band, applied around the chamber's floor.
    constexpr float CHROMA_FLOOR_Z    = 476.6f;
    constexpr float CHROMA_FLOOR_BAND = 15.0f;

    // Search radius handed to the UseGO step. The step's own default is 20yd,
    // which would never see the lever from the standoff; this has to reach from
    // anywhere the due range admits.
    constexpr float CHROMA_LEVER_SEARCH = 100.0f;

    // Grid-scan radius for Chromaggus himself, from the bot. Only has to cover
    // the cage from the chamber — his spawn is 47yd from the home anchor and
    // 66yd from the lever, and the bot may be anywhere between them.
    constexpr float CHROMA_SCAN = 100.0f;

    // instance_blackwing_lair's DATA_CHROMAGGUS. Same index space as
    // BROODLORD_ENCOUNTER_INDEX above (the script's DATA_ enum, which on this map
    // happens to match the DBC bits).
    constexpr uint32 CHROMAGGUS_ENCOUNTER_INDEX = 6;

    // The event row. Ids are per-map: 1 Razorgore, 2 Vaelastrasz, 3 the transit.
    constexpr uint32 EVENT_CHROMAGGUS_CAGE = 4;

    // WHERE CHROMAGGUS IS IN HIS OWN OPENING — the two facts the event predicate
    // and the boss-engage hold must never disagree about, read in ONE scan for
    // the same reason OrbGuards and Vaelastrasz do it.
    //
    //   * `caged` — he is alive and still carries UNIT_FLAG_IMMUNE_TO_PC. Only
    //     the lever clears it (SetGUID -> SetImmuneToAll(false)), and nothing
    //     re-applies it short of a full respawn, so this is exactly "the cage has
    //     never been opened in this instance" and it survives a wipe correctly:
    //     after a failed attempt he evades home ATTACKABLE, and the raid re-pulls
    //     him the ordinary way with no lever involved.
    //   * `leverReady` — the lever is spawned, GO_STATE_READY and still
    //     selectable. go_chromaggus_lever's GossipHello stamps
    //     GO_FLAG_NOT_SELECTABLE | GO_FLAG_IN_USE and GO_STATE_ACTIVE on itself,
    //     unconditionally and permanently (nothing in the script ever resets it),
    //     so this is the one-way latch the click cannot double-fire through.
    //
    // Free everywhere else — the map compare rejects before anything is scanned.
    struct ChromaggusState
    {
        bool present{false};    // alive, in or out of the cage
        bool caged{false};      // ...and still immune, i.e. the lever is unpulled
        bool leverReady{false}; // ...and the lever is still there to pull
    };
    ChromaggusState Chromaggus(Player* bot);

    // --- Nefarian (boss 8) — starting the encounter ------------------------
    //
    // The third boss here a raid does not pull, and the only one that does not
    // EXIST until the raid asks for him. Lord Victor Nefarius (10162) sits
    // friendly and passive on his balcony offering a gossip; answering it runs
    // boss_victor_nefarius::sGossipSelect -> BeginEvent, which flips him hostile,
    // engages the raid and starts the drakonid waves. Nefarian himself (11583) is
    // summoned only after MAX_DRAKONID_KILLED (42) adds die, flies in on waypoint
    // path 11583 and lands at the far end of the room.
    //
    // DC's entire job here is the gossip. Everything after it — the wave fight,
    // the transformation, the class calls — belongs to mod-playerbots' raid
    // strategy, exactly as Razorgore's adds and Vaelastrasz's burn do.
    constexpr uint32 NPC_VICTOR_NEFARIUS = 10162;
    constexpr uint32 NPC_NEFARIAN        = 11583;

    // Victor's spawn (creature guid 85785, map 469) — the gossip target and the
    // event's proximity gate.
    constexpr float NEFARIUS_X = -7587.76f;
    constexpr float NEFARIUS_Y = -1261.43f;
    constexpr float NEFARIUS_Z = 482.21f;

    // WHERE NEFARIAN LANDS, and the roster anchor for him.
    //
    // He has NO creature spawn row at all, so BossSpawnIndex cannot derive him
    // and the auto-roster ends at Chromaggus — the run would report itself
    // finished one boss short. This is the last point of waypoint path 11583
    // (the intro flight from (-7348.85, -1495.13, 552.52) down into the lair),
    // i.e. the ground he is standing on the moment he becomes a boss anybody can
    // fight. 86yd from Victor, which the event's due range has to span.
    constexpr float NEFARIAN_X = -7502.0f;
    constexpr float NEFARIAN_Y = -1256.5f;
    constexpr float NEFARIAN_Z = 476.758f;

    // His real DungeonEncounter bit (DBC row 617, encounterIndex 7) — read off
    // DungeonEncounter.dbc, not guessed from the instance script's DATA_ enum.
    // MakeBossWithBit takes it directly because there is no derived row to
    // inherit a bit from.
    constexpr uint32 NEFARIAN_ENCOUNTER_INDEX = 7;

    // THE GOSSIP CHAIN. creature_template.gossip_menu_id is 21330; its lone
    // option opens 21331, whose lone option opens 21332, whose lone option
    // ("Please do.") is the one boss_victor_nefarius::sGossipSelect answers
    // (`sender == 21332 && action == 0`). Every level offers exactly one option
    // at OptionID 0 and none of the three carries a `conditions` row, so
    // DungeonEventExecutor::SelectGossip — which selects the authored option and
    // then keeps selecting option 0 of whatever submenu opens until the menu
    // closes — walks the whole chain from a single authored 0.
    constexpr int32 NEFARIUS_GOSSIP_OPTION = 0;

    // How far from VICTOR the leader may be and still read the start as due, and
    // how far out the Gossip step may acquire him.
    //
    // The due range is a WINDOW, not just a floor, and both ends are load-bearing.
    // It must clear the 86yd from Nefarian's landing (the raid anchor) to Victor
    // with slack for wherever the at-boss hold actually parks the tank — or the
    // event could never arm from the place the advance delivers the raid to. And
    // it must NOT reach back to the lair's entrance portcullis, which is 149yd
    // from Victor: an event due at the door would preempt the advance there and
    // walk the tank the length of the room while the raid was still filing in
    // behind it. 120 sits between the two with room either side.
    //
    // The scan is deliberately much wider — it only has to RESOLVE him once the
    // range gate has already said yes.
    constexpr float NEFARIUS_DUE_RANGE = 120.0f;
    constexpr float NEFARIUS_SCAN      = 200.0f;

    // The event row. Ids are per-map: 1 Razorgore, 2 Vaelastrasz, 3 the transit,
    // 4 Chromaggus' cage.
    constexpr uint32 EVENT_NEFARIAN_START = 5;

    // WHETHER THE ENCOUNTER HAS BEEN STARTED, read off Victor himself.
    //
    // `offersStart` is UNIT_NPC_FLAG_GOSSIP, and it is the same one-way latch the
    // Vaelastrasz rouse uses: sGossipSelect removes the flag before anything
    // else, so the predicate goes false on the click and a second select would
    // reach no script anyway. It also comes BACK on its own — a failed attempt
    // schedules EVENT_RESPAWN_NEFARIUS 15min out and Reset() re-adds the flag —
    // which is why the event is Repeatable: the raid gets to start him again.
    struct NefariusState
    {
        bool present{false};     // alive, on his balcony
        bool offersStart{false}; // ...and still waiting to be talked to
    };
    NefariusState Nefarius(Player* bot);
}

void RegisterBlackwingLairEvents(std::vector<DungeonEvent>& out);

// Every TempSummon the siege can field — the trash, the elites, the three portal
// keepers, Ichoron's globules, Xevozz's spheres and Cyanigosa. Probed by
// ALIVENESS by the wave event's activation predicate, which is sound only
// because none of them exists before the encounter creates it. The six caged
// prisoners and Erekem's guards are world spawns and are NOT here — see
// VioletHoldPrisonerEntries().
// --- Halls of Stone shared entry lists (HallsOfStoneEvents.cpp) -----------

// The three Tribunal wave adds. Every one is a TempSummon with spawnId 0, so the
// spawn store cannot see them and both the activation predicate and the wave
// driver grid-scan this list instead.
//
// It must stay COMPLETE: an entry missing here reads as "the arena is quiet",
// hands the tick back to the plain garrison, and lets that add walk to Brann —
// whose death is the encounter's only fail condition and restarts the entire
// escort. It must also stay EXCLUSIVE of the three heads, which are probed
// separately (they exist for the WHOLE fight, including the quiet first 52
// seconds, and are what arms the driver before any add has spawned).
//
// NORMAL ENTRIES ONLY, on both difficulties — see the difficulty-twin note in
// namespace DcHallsOfStone for why adding 31876/31877/31380 would be dead weight.
std::vector<uint32> const& HallsOfStoneWaveEntries();

// Kaddrak / Marnak / Abedneum, probed for mere ALIVENESS — sound because they
// exist only between InitializeEvent() and EndTribunalFight()/ResetEvent().
std::vector<uint32> const& HallsOfStoneHeadEntries();

std::vector<uint32> const& VioletHoldWaveEntries();

// Portal Guardian 30660 / Portal Keeper 30695 / 30893 — the ONLY thing whose
// death closes a keeper portal and stops its 20-second, never-ending add pump
// (npc_vh_teleportation_portal kills itself once nothing is left to channel
// 58012 on). Shared with the wave driver, which selects and travels by it.
// Deliberately disjoint from the trash: keepers never walk to the door and never
// drain the seal, and no trash mob ever closes a portal.
std::vector<uint32> const& VioletHoldKeeperEntries();

// The six caged prisoners plus Erekem's two guards. World spawns, present from
// map load behind sealed cells, so they are probed with DcVioletHold::IsReleased
// (the instance clears NON_ATTACKABLE / IMMUNE_TO_PC on release) and never for
// mere aliveness — an aliveness probe on these would read true on an inert
// dungeon and hand the wave driver the tick before the party had even entered.
std::vector<uint32> const& VioletHoldPrisonerEntries();

// --- roster patches (one appender per dungeon that corrects the boss list) -
// Each relocates that dungeon's BossRosterPatch out of BossRosterRegistry.cpp
// so a dungeon's whole clear definition lives in one file. Aggregated by
// PatchTable() (BossRosterRegistry.cpp). Only dungeons that patch the derived
// roster appear here (e.g. Shadowfang Keep / Blood Furnace have events but no
// patch, so no roster appender).
void RegisterScarletMonasteryRoster(std::vector<BossRosterPatch>& t);
void RegisterScholomanceRoster(std::vector<BossRosterPatch>& t);
void RegisterSunkenTempleRoster(std::vector<BossRosterPatch>& t);
void RegisterRazorfenDownsRoster(std::vector<BossRosterPatch>& t);
void RegisterZulFarrakRoster(std::vector<BossRosterPatch>& t);
void RegisterBlackrockDepthsRoster(std::vector<BossRosterPatch>& t);
void RegisterBlackwingLairRoster(std::vector<BossRosterPatch>& t);
void RegisterDeadminesRoster(std::vector<BossRosterPatch>& t);
void RegisterWailingCavernsRoster(std::vector<BossRosterPatch>& t);
void RegisterStratholmeRoster(std::vector<BossRosterPatch>& t);
void RegisterDireMaulRoster(std::vector<BossRosterPatch>& t);
void RegisterUldamanRoster(std::vector<BossRosterPatch>& t);
void RegisterHellfireRampartsRoster(std::vector<BossRosterPatch>& t);
void RegisterSlavePensRoster(std::vector<BossRosterPatch>& t);
void RegisterUnderbogRoster(std::vector<BossRosterPatch>& t);
void RegisterOldHillsbradRoster(std::vector<BossRosterPatch>& t);
void RegisterMechanarRoster(std::vector<BossRosterPatch>& t);
void RegisterShatteredHallsRoster(std::vector<BossRosterPatch>& t);
void RegisterSteamvaultRoster(std::vector<BossRosterPatch>& t);
void RegisterArcatrazRoster(std::vector<BossRosterPatch>& t);
void RegisterSethekkHallsRoster(std::vector<BossRosterPatch>& t);
void RegisterBlackMorassRoster(std::vector<BossRosterPatch>& t);
void RegisterMaraudonRoster(std::vector<BossRosterPatch>& t);
void RegisterUtgardeKeepRoster(std::vector<BossRosterPatch>& t);
void RegisterNexusRoster(std::vector<BossRosterPatch>& t);
void RegisterAzjolNerubRoster(std::vector<BossRosterPatch>& t);
void RegisterAhnkahetRoster(std::vector<BossRosterPatch>& t);
void RegisterDrakTharonKeepRoster(std::vector<BossRosterPatch>& t);
void RegisterVioletHoldRoster(std::vector<BossRosterPatch>& t);
void RegisterHallsOfStoneRoster(std::vector<BossRosterPatch>& t);
void RegisterGundrakRoster(std::vector<BossRosterPatch>& t);
void RegisterMoltenCoreRoster(std::vector<BossRosterPatch>& t);

// --- wing layouts (one appender per split map) ---------------------------
// Records which boss credit-entries belong to which wing of a multi-wing map;
// aggregated by DungeonWingRegistry. Only split maps appear here. Maraudon has
// no events (wings + one roster removal) and lives in MaraudonEvents.cpp.
void RegisterDireMaulWings(std::unordered_map<uint32, DungeonWingLayout>& store);
void RegisterScarletMonasteryWings(std::unordered_map<uint32, DungeonWingLayout>& store);
void RegisterMaraudonWings(std::unordered_map<uint32, DungeonWingLayout>& store);

// --- anchor routes (one appender per dungeon that hand-authors a route) ---
// Waypoint anchors StridedPathfinder walks INSTEAD of asking the navmesh
// pathfinder for a corridor, for stretches where the mesh defeats it. These
// take no `out` parameter — they call DungeonClearRouteRegistry::Register
// directly — and are invoked from DungeonClearRouteRegistry's own one-time
// seed, for the same linkage reason as the tables above.
void RegisterAzjolNerubRoute();
// Blackwing Lair (469) — Vaelastrasz -> Broodlord Lashlayer, the suppression
// rooms. Unlike the Azjol-Nerub row this is NOT there because the mesh defeats
// the pathfinder: the corridor routes fine. It is there because the SUPPRESSION
// TRANSIT needs a fixed, monotone polyline to run a cursor along — the pack
// leash, the hold decisions and the telemetry all measure against the same
// authored legs, and a route re-derived every tick from the leader's live
// position is not something a cursor can advance through. See
// DcBlackwingLair's transit block.
void RegisterBlackwingLairRoute();
// Halls of Lightning (602) — Bjarngrim -> Volkhan, the Slag Furnace. Same reason
// as the Blackwing Lair row and not the Azjol-Nerub one: the corridor routes
// fine, but the SLAG FURNACE TRANSIT needs a fixed, monotone polyline to run a
// cursor along, and the middle of that row is sliced out and handed to the driver
// (DcHallsOfLightning::TRANSIT_STAGE_ANCHOR_INDEX .. TRANSIT_END_ANCHOR_INDEX).
void RegisterHallsOfLightningRoute();

#endif
