/*
 * mod-dungeon-clear — DungeonClearCommand.cpp
 *
 * Slash command `.dc on|off|skip|status|bosses`. A convenience entry point that
 * works with zero config (unlike the chat keywords, which need the
 * "dungeon clear" strategy applied — see DungeonClearModule.cpp).
 *
 * Each subcommand dispatches the matching DungeonClear action ("dc on", …) to
 * the issuing player's tank bot(s) via PlayerbotAI::DoSpecificAction. The
 * actions already self-authorize (owner must be a real player in the bot's
 * group) and self-gate (e.g. `dc on` is tank-only), so we carry the issuing
 * player as the Event owner and let the existing action logic decide.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Group.h"
#include "Player.h"
#include "StringFormat.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

#include "DungeonClearDispatch.h"
#include "TestRun/DcTestDriver.h"
#include "TestRun/DcTestDungeonRegistry.h"
#include "TestRun/DcTestPlan.h"
#include "TestRun/DcTestPlanManager.h"
#include "TestRun/DcTestRunManager.h"
#include "Util/DcSpectator.h"
#include "Ai/Dungeon/DungeonClear/Settings/DcSettings.h"
#include "Ai/Dungeon/DungeonClear/Settings/DcSettingsRegistry.h"
#include "Ai/Dungeon/DungeonClear/Util/DungeonClearUtil.h"

using namespace Acore::ChatCommands;

namespace
{
    bool RunDcCommand(ChatHandler* handler, std::string const& action, std::string const& param = "")
    {
        Player* issuer = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!issuer)
        {
            handler->SendSysMessage("This command must be used in-game.");
            return true;
        }

        if (!DungeonClearDispatch::DispatchToTankBots(issuer, action, param))
            handler->SendSysMessage("No tank bot found in your group.");

        return true;
    }

    // Format a resolved raw double per the registry type, so the printout reads
    // the way the conf line is written (true/false, ints, floats).
    std::string FormatDcValue(DcSettingDef const& d, double raw)
    {
        switch (d.type)
        {
            case DcType::Bool:
                return raw != 0.0 ? "true" : "false";
            case DcType::UInt:
            case DcType::Int:
                return Acore::StringFormat("{}", static_cast<int64>(std::lround(raw)));
            case DcType::Float:
            default:
                return Acore::StringFormat("{:.2f}", raw);
        }
    }

    // Dumps every DungeonClear tunable as the module actually reads it: the live
    // conf/default value, plus the per-run effective value when the issuer's run
    // has an addon override active. This is a pure read of sConfigMgr through the
    // DcSettings accessor, so it reflects exactly what the AI sees this tick —
    // use it to confirm whether a conf edit took effect (no `.reload config`).
    bool HandleConfig(ChatHandler* handler)
    {
        Player* issuer = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!issuer)
        {
            handler->SendSysMessage("This command must be used in-game.");
            return true;
        }

        // Resolve the run owner (leader tank) so we can surface per-run overrides.
        // Empty when the issuer isn't in a DC run — then only conf/defaults show.
        Player* leader = DcLeaderSignal::FindLeaderTank(issuer);
        ObjectGuid const runOwner = leader ? leader->GetGUID() : ObjectGuid::Empty;

        handler->SendSysMessage(
            "DungeonClear config (effective values; * = addon override, H = heroic default):");
        for (DcSettingDef const& d : kDcSettings)
        {
            // confVal reads with no owner, so it never picks up the heroic layer:
            // it is the "what this would be outside the run" baseline both markers
            // are shown against. effVal is what the run is actually using.
            double const confVal = DcSettings::GetEffectiveRaw(ObjectGuid::Empty, d);
            double const effVal  = DcSettings::GetEffectiveRaw(runOwner, d);
            bool const overridden =
                !runOwner.IsEmpty() && DcSettings::HasOverride(runOwner, d.key);

            std::string line;
            if (overridden)
                line = Acore::StringFormat("  * DungeonClear.{} = {} (conf {})",
                                           d.key, FormatDcValue(d, effVal),
                                           FormatDcValue(d, confVal));
            else if (effVal != confVal)
                // Not overridden but not the conf value either: the run is heroic
                // and this row carries a heroic default. Without the marker the
                // number looks like the conf line is being ignored.
                line = Acore::StringFormat("  H DungeonClear.{} = {} (normal {})",
                                           d.key, FormatDcValue(d, effVal),
                                           FormatDcValue(d, confVal));
            else
                line = Acore::StringFormat("    DungeonClear.{} = {}",
                                           d.key, FormatDcValue(d, confVal));
            handler->SendSysMessage(line);
        }
        return true;
    }

    // Spectator free-camera toggle. Acts on the ISSUER directly (session
    // plumbing, not bot behavior) — it must NOT go through DispatchToTankBots
    // or the action pipeline: the issuer may not even be the tank, and the
    // possession belongs to their session alone. See Util/DcSpectator.h.
    bool HandleSpectate(ChatHandler* handler)
    {
        Player* issuer = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!issuer)
        {
            handler->SendSysMessage("This command must be used in-game.");
            return true;
        }

        std::string whyNot;
        if (!DcSpectator::Toggle(issuer, &whyNot))
            handler->SendSysMessage(whyNot);
        return true;
    }
}

class dungeon_clear_command_script : public CommandScript
{
public:
    dungeon_clear_command_script() : CommandScript("dungeon_clear_command_script") {}

    ChatCommandTable GetCommands() const override
    {
        // Console::Yes across `.dc test`: a console (or dashboard screen-
        // bridge) start resolves its issuing GM to the headless driver
        // character — see DcTestDriver and ResolveTestIssuer.
        static ChatCommandTable dcTestPlanTable =
        {
            { "start",  HandleTestPlanStart,  SEC_GAMEMASTER, Console::Yes },
            { "status", HandleTestPlanStatus, SEC_GAMEMASTER, Console::Yes },
            { "stop",   HandleTestPlanStop,   SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable dcTestTable =
        {
            { "start",  HandleTestStart,  SEC_GAMEMASTER, Console::Yes },
            { "status", HandleTestStatus, SEC_GAMEMASTER, Console::Yes },
            { "stop",   HandleTestStop,   SEC_GAMEMASTER, Console::Yes },
            { "list",   HandleTestList,   SEC_GAMEMASTER, Console::Yes },
            { "plan",   dcTestPlanTable },
        };
        static ChatCommandTable dcTable =
        {
            { "on",     HandleOn,     SEC_PLAYER, Console::No },
            { "off",    HandleOff,    SEC_PLAYER, Console::No },
            { "skip",   HandleSkip,   SEC_PLAYER, Console::No },
            { "pause",  HandlePause,  SEC_PLAYER, Console::No },
            { "pull",   HandlePull,   SEC_PLAYER, Console::No },
            { "status", HandleStatus, SEC_PLAYER, Console::No },
            { "bosses", HandleBosses, SEC_PLAYER, Console::No },
            { "go",     HandleGo,     SEC_PLAYER, Console::No },
            { "config", HandleConfig, SEC_PLAYER, Console::No },
            { "spectate", HandleSpectate, SEC_PLAYER, Console::No },
            { "test",   dcTestTable },
        };
        static ChatCommandTable root = { { "dc", dcTable } };
        return root;
    }

    static bool HandleOn(ChatHandler* handler)     { return RunDcCommand(handler, "dc on"); }
    static bool HandleOff(ChatHandler* handler)    { return RunDcCommand(handler, "dc off"); }
    static bool HandleSkip(ChatHandler* handler)   { return RunDcCommand(handler, "dc skip"); }
    static bool HandlePause(ChatHandler* handler)  { return RunDcCommand(handler, "dc pause"); }
    static bool HandlePull(ChatHandler* handler, Optional<std::string> param) { return RunDcCommand(handler, "dc pull", param ? *param : ""); }
    static bool HandleStatus(ChatHandler* handler, Optional<std::string> param) { return RunDcCommand(handler, "dc status", param ? *param : ""); }
    static bool HandleBosses(ChatHandler* handler, Optional<std::string> param) { return RunDcCommand(handler, "dc bosses", param ? *param : ""); }
    static bool HandleGo(ChatHandler* handler, Tail targetBoss) { return RunDcCommand(handler, "dc go", std::string(targetBoss)); }

    // --- `.dc test` — the automated test-run harness ------------------------
    // These act on DcTestRunManager directly (never DispatchToTankBots: the
    // whole point is that the GM is NOT in the bot party).

    // The issuing GM for a start: the in-game player when there is one, else
    // the headless driver (console / dashboard path). nullptr with a pending
    // message sent when the driver is still logging in — the caller retries.
    static Player* ResolveTestIssuer(ChatHandler* handler)
    {
        if (Player* issuer = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
            return issuer;

        std::string whyPending;
        if (DcTestDriver::EnsureOnline(&whyPending))
            return DcTestDriver::Get();
        handler->SendSysMessage(whyPending);
        return nullptr;
    }

    // `.dc test start <dungeon> [heroic] [level=N]` — dungeon is a registry
    // token (`.dc test list`) or a mapId.
    static bool HandleTestStart(ChatHandler* handler, Tail args)
    {
        Player* issuer = ResolveTestIssuer(handler);
        if (!issuer)
            return true;

        std::string token;
        uint32 level = 0;
        uint32 seed = 0;  // 0 = roll a random comp; seed=N replays a specific one
        bool heroic = false;
        std::istringstream in{std::string(args)};
        std::string word;
        while (in >> word)
        {
            if (word.rfind("level=", 0) == 0)
                level = static_cast<uint32>(std::strtoul(word.c_str() + 6, nullptr, 10));
            else if (word.rfind("seed=", 0) == 0)
                seed = static_cast<uint32>(std::strtoul(word.c_str() + 5, nullptr, 10));
            else if (word == "heroic")
                heroic = true;
            else if (token.empty())
                token = word;
            else
            {
                handler->SendSysMessage("Usage: .dc test start <dungeon> [heroic] [level=N] [seed=N]");
                return true;
            }
        }
        if (token.empty())
        {
            handler->SendSysMessage("Usage: .dc test start <dungeon> [heroic] [level=N] [seed=N] — see .dc test list");
            return true;
        }

        std::string msg;
        DcTestRunManager::Instance().Start(issuer, token, level, seed, heroic, &msg);
        handler->SendSysMessage(msg);
        return true;
    }

    static bool HandleTestStatus(ChatHandler* handler)
    {
        handler->SendSysMessage(DcTestRunManager::Instance().StatusText());
        if (DcTestPlanManager::Instance().HasActivePlans())
            handler->SendSysMessage(DcTestPlanManager::Instance().StatusText());
        return true;
    }

    // `.dc test stop [selector]` — bare = the single active run (errors listing
    // runs when >1 active); "all"; an exact runId; or a dungeon token (all its
    // runs). See DcTestRunSelect. "all" also stops every active plan first —
    // otherwise the plan scheduler would relaunch the runs it just aborted.
    static bool HandleTestStop(ChatHandler* handler, Tail selector)
    {
        if (std::string(selector) == "all" && DcTestPlanManager::Instance().HasActivePlans())
        {
            DcTestPlanManager::Instance().StopAll("stopped via .dc test stop all");
            handler->SendSysMessage("stopping all test plans");
        }
        std::string msg;
        DcTestRunManager::Instance().Stop(std::string(selector), &msg);
        handler->SendSysMessage(msg);
        return true;
    }

    // --- `.dc test plan` — batched campaigns (N runs, capped concurrency) ----

    // Unlike `.dc test start`, a plan does NOT need its issuer up front: the
    // scheduler re-resolves one per launch and waits out an in-flight driver
    // login. That matters because the very first console/dashboard start is
    // the click that kicks that login off — requiring an issuer here rejected
    // exactly the request that caused the driver to come online.
    static bool HandleTestPlanStart(ChatHandler* handler, Tail args)
    {
        Player* issuer = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!issuer)
        {
            std::string why;
            DcTestDriver::Readiness const ready = DcTestDriver::Ensure(&why);
            if (ready == DcTestDriver::Readiness::Unavailable)
            {
                handler->SendSysMessage("Test plan not started: " + why);
                return true;
            }
            issuer = DcTestDriver::Get();  // nullptr while the login is in flight
        }

        DcTestPlan::ParseResult const parsed = DcTestPlan::ParseStartArgs(std::string(args));
        if (!parsed.ok)
        {
            handler->SendSysMessage(parsed.err);
            return true;
        }

        std::string msg;
        DcTestPlanManager::Instance().Start(parsed.spec, issuer, &msg);
        handler->SendSysMessage(msg);
        return true;
    }

    static bool HandleTestPlanStatus(ChatHandler* handler)
    {
        handler->SendSysMessage(DcTestPlanManager::Instance().StatusText());
        return true;
    }

    // `.dc test plan stop [planId|all]` — bare = the single active plan.
    static bool HandleTestPlanStop(ChatHandler* handler, Tail selector)
    {
        std::string msg;
        DcTestPlanManager::Instance().Stop(std::string(selector), &msg);
        handler->SendSysMessage(msg);
        return true;
    }

    static bool HandleTestList(ChatHandler* handler)
    {
        handler->SendSysMessage("Supported test dungeons (.dc test start <token> [heroic]):");
        for (DcTestDungeonRegistry::Row const& row : DcTestDungeonRegistry::All())
            handler->SendSysMessage(Acore::StringFormat(
                "  {:<16} {} (map {}, level {}{})", row.token, row.name, row.mapId,
                row.recommendedLevel,
                row.heroicLevel ? Acore::StringFormat(", heroic {}", row.heroicLevel)
                                : std::string()));
        return true;
    }
};

void AddSC_dungeon_clear_command()
{
    new dungeon_clear_command_script();
}
