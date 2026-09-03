/*
 * mod-dungeon-clear — DungeonClearAddonHook.cpp
 *
 * PlayerScript hook that intercepts addon messages (LANG_ADDON, prefix "DC")
 * sent by the DungeonClear companion addon.  Parses "CMD\t<sub>\t<param>"
 * payloads and dispatches to the same tank-bot actions that the `.dc` slash
 * command uses.
 *
 * The addon sends commands via SendAddonMessage("DC", ..., "PARTY") in a party
 * or "RAID" in a raid, arriving as CHAT_MSG_PARTY / CHAT_MSG_RAID / LANG_ADDON.
 * (A raid must use the RAID channel: a PARTY addon message only reaches the
 * sender's subgroup, so a tank bot in another subgroup would never see it.)
 * Our OnPlayerBeforeSendChatMessage
 * hook fires before the ChatHandler switch statement, parses the command,
 * dispatches it silently (DoSpecificAction with silent=true), then consumes
 * the message so no further chat processing occurs.
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <unordered_map>

#include "ScriptMgr.h"
#include "PlayerScript.h"
#include "Chat.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ServerFacade.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include "DcModuleEnable.h"
#include "DungeonClearDispatch.h"
#include "StringFormat.h"
#include "Util/DcSpectator.h"
#include "Ai/Dungeon/DungeonClear/Settings/DcSettings.h"
#include "Ai/Dungeon/DungeonClear/Settings/DcSettingsRegistry.h"
#include "Ai/Dungeon/DungeonClear/Util/DungeonClearUtil.h"
#include "Ai/Dungeon/DungeonClear/Value/DungeonBossesValue.h"

namespace
{
    // Send a raw "DC\t..." payload back to the player via the addon channel.
    void SendAddonPayload(Player* player, std::string const& payload)
    {
        if (!player)
            return;

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_PARTY, payload.c_str(),
                                     LANG_ADDON, CHAT_TAG_NONE,
                                     player->GetGUID(), player->GetName());

        ServerFacade::instance().SendPacket(player, &data);
    }

    // Send an error back to the player via the addon message channel.
    void SendAddonError(Player* player, std::string const& msg)
    {
        SendAddonPayload(player, "DC\tERROR\t" + msg);
    }

    // Tell the addon whether the spectator free-camera is enabled server-side so
    // it can grey out / disable its Spectate button instead of letting the player
    // click into a refusal. DungeonClear.SpectateEnable is a server-only flag, so
    // this is the only way the addon learns it. Sent in answer to the addon's
    // status poll (its panel heartbeat) — tank-independent, and refreshed every
    // time the panel reopens or combat toggles.
    void SendSpectateState(Player* player)
    {
        bool const enabled = DcSettings::GetBool(player, "SpectateEnable");
        SendAddonPayload(player,
            Acore::StringFormat("DC\tSPECTATE\t{}", enabled ? 1 : 0));
    }

    // One "DC\tSETTINGS\t<key>\t<value>\t<min>\t<max>\t<type>\t<overridden>"
    // line describing one player-facing setting's effective value + schema, so
    // the addon can populate (and optionally render) its panel from the server.
    void SendSettingLine(Player* player, ObjectGuid owner, DcSettingDef const& d)
    {
        double const value = DcSettings::GetEffectiveRaw(owner, d);
        bool const overridden = DcSettings::HasOverride(owner, d.key);

        SendAddonPayload(player, Acore::StringFormat(
            "DC\tSETTINGS\t{}\t{}\t{}\t{}\t{}\t{}",
            d.key, value, d.minVal, d.maxVal,
            static_cast<int>(d.type), overridden ? 1 : 0));
    }

    // Push every player-facing setting (the full panel) to the player, framed by
    // a SYNCSTART/SYNCEND pair so the addon knows when it has the complete set.
    void SendSettingsSync(Player* player, ObjectGuid owner)
    {
        SendAddonPayload(player, "DC\tSYNCSTART");
        for (DcSettingDef const& d : kDcSettings)
            if (d.playerFacing)
                SendSettingLine(player, owner, d);
        SendAddonPayload(player, "DC\tSYNCEND");
    }

    // V-01: The run owner is the group leader, or — when the leader is a bot —
    // the player who owns that bot. This is the single authority check every
    // mutating command passes through. Read-only commands (status, sync) skip it.
    bool IsRunOwner(Player* player)
    {
        if (!player)
            return false;

        Group* group = player->GetGroup();
        if (!group)
            return false;

        ObjectGuid leaderGuid = group->GetLeaderGUID();
        if (leaderGuid != player->GetGUID())
        {
            Player* leader = ObjectAccessor::FindConnectedPlayer(leaderGuid);
            if (!leader || !leader->IsPlayerbot())
                return false;

            PlayerbotAI* leaderAi = leader->GetPlayerbotAI();
            if (!leaderAi)
                return false;
            if (leaderAi->GetOwner() != player)
                return false;
        }

        return true;
    }

    // V-02: Validate that a creature entry is a boss in the tank bot's current
    // instance. Prevents a modified client from sending the tank to an arbitrary
    // creature ID that could exploit pathfinding or cause stalls.
    bool IsValidBossEntry(Player* tankBot, uint32 entry)
    {
        if (!tankBot || entry == 0)
            return false;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(tankBot);
        if (!botAI)
            return false;

        auto const& bosses = botAI->GetAiObjectContext()
            ->GetValue<std::vector<DungeonBossInfo>>(DcKey::DungeonBosses)->Get();

        for (auto const& boss : bosses)
            if (boss.entry == entry)
                return true;
        return false;
    }

    // V-10: Reject parameters containing tab or newline characters that would
    // break the tab-delimited addon protocol or inject unexpected fields.
    bool IsValidParam(std::string const& param)
    {
        if (param.empty())
            return true;
        return param.find('\t') == std::string::npos &&
               param.find('\n') == std::string::npos &&
               param.find('\r') == std::string::npos;
    }

    // set / reset / sync share the same run-owner resolution. Returns false (and
    // reports an error) when the player has no leader tank to own the overrides.
    void HandleSettingsCommand(Player* player, std::string const& subCmd,
                               std::string const& param)
    {
        // The run owner is this party's leader tank, if one exists. sync works
        // without one (it just reports the server defaults so the addon panel
        // can render anywhere); set/reset need an owner to attach the override
        // to and report an error when there's no tank in the group.
        Player* leader = DcLeaderSignal::FindLeaderTank(player);
        ObjectGuid const owner = leader ? leader->GetGUID() : ObjectGuid::Empty;

        if (subCmd == "sync")
        {
            SendSettingsSync(player, owner);
            return;
        }

        if (owner.IsEmpty())
        {
            SendAddonError(player, "No tank bot found in your group.");
            return;
        }

        // V-03: set and reset require run-owner authority.
        if (!IsRunOwner(player))
        {
            SendAddonError(player,
                "Only the group leader can change dungeon clear settings.");
            return;
        }

        if (subCmd == "reset")
        {
            // V-04: If a specific key is named, validate it exists and is
            // player-facing before clearing its override.
            if (!param.empty())
            {
                DcSettingDef const* def = FindDcSetting(param);
                if (!def)
                {
                    SendAddonError(player, "Unknown setting: " + param);
                    return;
                }
                if (!def->playerFacing)
                {
                    SendAddonError(player,
                        "Setting " + param + " is not player-configurable.");
                    return;
                }
            }

            DcSettings::ResetOverride(owner, param);
            SendSettingsSync(player, owner);
            return;
        }

        // subCmd == "set": param is "<key>\t<value>".
        auto const sep = param.find('\t');
        if (sep == std::string::npos)
        {
            SendAddonError(player, "set requires <key> <value>.");
            return;
        }

        std::string const key = param.substr(0, sep);
        std::string const valStr = param.substr(sep + 1);

        // V-04a: The key must exist in the registry.
        DcSettingDef const* def = FindDcSetting(key);
        if (!def)
        {
            SendAddonError(player, "Unknown setting: " + key);
            return;
        }

        // V-04b: The key must be player-facing.
        if (!def->playerFacing)
        {
            SendAddonError(player,
                "Setting " + key + " is not player-configurable.");
            return;
        }

        // V-04c: The value must be a valid number.
        char* end = nullptr;
        double const value = std::strtod(valStr.c_str(), &end);
        if (end == valStr.c_str() || !std::isfinite(value))
        {
            SendAddonError(player, "Invalid value for " + key + ".");
            return;
        }

        // V-04d: Clamp to the setting's [min, max] range and inform the player
        // if the value was adjusted.
        double const clamped = std::clamp(value, def->minVal, def->maxVal);
        if (clamped != value)
        {
            SendAddonError(player,
                Acore::StringFormat("{} clamped to {:.1} (range {}-{}).",
                    key, clamped, def->minVal, def->maxVal));
        }

        std::string err;
        if (!DcSettings::SetOverride(owner, key, clamped, &err))
        {
            SendAddonError(player, err);
            return;
        }

        // Echo the stored (clamped) value back so the addon shows the truth.
        if (DcSettingDef const* d = FindDcSetting(key))
            SendSettingLine(player, owner, *d);
    }

    // V-07: Per-player, per-subcommand rate limiting to prevent addon-message
    // spam from a modified client. Uses steady_clock for monotonic intervals.
    // Cleaned up on player logout.
    using TimePoint = std::chrono::steady_clock::time_point;

    // Cooldowns per subcommand (milliseconds). Read-only commands get short
    // windows; mutating commands get longer ones.
    std::unordered_map<std::string, uint32> const kCooldowns = {
        { "status",   1000 },
        { "bosses",    2000 },
        { "sync",      5000 },
        { "on",        1000 },
        { "off",       1000 },
        { "skip",      1000 },
        { "pause",     1000 },
        { "pull",      1000 },
        { "go",        1000 },
        { "set",       1000 },
        { "reset",     2000 },
        { "spectate",  1000 },
    };

    // player GUID -> (subCmd -> last-used timestamp)
    std::unordered_map<ObjectGuid,
        std::unordered_map<std::string, TimePoint>> g_throttleState;

    bool IsThrottled(Player* player, std::string const& subCmd)
    {
        auto it = kCooldowns.find(subCmd);
        if (it == kCooldowns.end())
            return false;

        auto now = std::chrono::steady_clock::now();
        auto& playerState = g_throttleState[player->GetGUID()];
        auto& lastUsed = playerState[subCmd];

        if (lastUsed.time_since_epoch().count() != 0)
        {
            auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastUsed).count();
            if (static_cast<uint32>(elapsed) < it->second)
                return true;
        }

        lastUsed = now;
        return false;
    }
}

class DungeonClearAddonHookScript : public PlayerScript
{
public:
    DungeonClearAddonHookScript()
        : PlayerScript("DungeonClearAddonHookScript", {
            PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
            PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT,
            PLAYERHOOK_ON_LOGOUT
        }) {}

    // True for the addon's client->server control messages, which ride in as an
    // addon chat line of the form "DC\tCMD\t<sub>[\t<param>]". Shared by the
    // parse hook (OnPlayerBeforeSendChatMessage, which acts on them) and the
    // relay-block hook (OnPlayerCanUseChat, which stops the core forwarding
    // them to the rest of the group).
    //
    // WHISPER is the solo transport. The addon's normal channel is PARTY/RAID,
    // which simply does not exist for a player with no group — so a GM watching
    // a test run from outside the bot party could not press any button at all,
    // spectate included, even though the spectator camera itself has never had
    // a group requirement. A whisper to oneself is the standard addon-message
    // channel for that case. The server dispatch self-authorizes either way
    // (bot commands still need a tank bot in the sender's group and say so),
    // so accepting the extra channel grants no new authority.
    static bool IsDcAddonCommand(uint32 type, uint32 lang, std::string const& msg)
    {
        if (lang != LANG_ADDON)
            return false;
        if (type != CHAT_MSG_PARTY && type != CHAT_MSG_PARTY_LEADER &&
            type != CHAT_MSG_RAID && type != CHAT_MSG_RAID_LEADER &&
            type != CHAT_MSG_WHISPER)
            return false;
        // "DC\t" (3) + "CMD\t" (4) = 7-byte prefix.
        return msg.compare(0, 7, "DC\tCMD\t") == 0;
    }

    // Per-run overrides are keyed by the leader tank's GUID; drop them when that
    // player logs out so stale leader GUIDs don't accumulate. A no-op for any
    // player who never owned a run.
    void OnPlayerLogout(Player* player) override
    {
        if (!DcModule::IsEnabled())
            return;  // no run can exist, so no override store to clear
        if (player)
        {
            DcSettings::ClearRun(player->GetGUID());
            // V-07: Clean up throttle state for the departing player.
            g_throttleState.erase(player->GetGUID());
        }
    }

    // Block the core from relaying our own control messages to the rest of the
    // group. We already act on them in OnPlayerBeforeSendChatMessage; returning
    // false here silently drops the packet. This REPLACES the old "consume"
    // trick of rewriting `type` to CHAT_MSG_ADDON (= 0xFFFFFFFF): that value has
    // no case in HandleMessagechatOpcode's `switch (type)`, so every command
    // fell through to the default and logged
    // "CHAT: unknown message type 4294967295, lang: 4294967295" — once per
    // command, and constantly from the addon's status-poll heartbeat.
    bool OnPlayerCanUseChat(Player* /*player*/, uint32 type, uint32 lang, std::string& msg, Group* /*group*/) override
    {
        return !IsDcAddonCommand(type, lang, msg);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& type, uint32& lang, std::string& msg) override
    {
        // Only our addon control messages ("DC\tCMD\t...", on party/raid addon
        // chat). The addon sends on RAID when the player is in a raid so the
        // command reaches a tank bot in any subgroup — on PARTY it would only
        // reach the sender's own subgroup. The matching relay-suppression lives
        // in OnPlayerCanUseChat above; here we only act on the command.
        if (!IsDcAddonCommand(type, lang, msg))
            return;

        // Master switch: answer the panel instead of silently dropping every
        // button it presses. The relay suppression in OnPlayerCanUseChat still
        // applies (the payload is ours either way, and must not reach party
        // chat as text). See DcModuleEnable.h.
        if (!DcModule::IsEnabled())
        {
            SendAddonError(player,
                           "mod-dungeon-clear is disabled on this server "
                           "(DungeonClear.Enable = 0).");
            return;
        }

        // Parse "DC\tCMD\t<subcommand>[\t<param>]" — strip the 7-byte prefix.
        std::string const cmdPayload = msg.substr(7);
        std::string subCmd;
        std::string param;

        auto const tabPos = cmdPayload.find('\t');
        if (tabPos == std::string::npos)
        {
            subCmd = cmdPayload;
        }
        else
        {
            subCmd = cmdPayload.substr(0, tabPos);
            param = cmdPayload.substr(tabPos + 1);
        }

        if (subCmd.empty())
            return;

        // V-07: Per-player rate limiting.
        if (IsThrottled(player, subCmd))
            return;

        // Per-run settings overrides (set/reset/sync) are handled in-process
        // rather than dispatched as a tank-bot action.
        if (subCmd == "set" || subCmd == "reset" || subCmd == "sync")
        {
            HandleSettingsCommand(player, subCmd, param);
            return;
        }

        // Spectator camera: acts on the sending player directly (session
        // plumbing, not a tank-bot action) — never dispatched. Bare = the
        // free-flying camera; "follow" rides the run's tank instead.
        if (subCmd == "spectate")
        {
            std::string whyNot;
            bool ok = true;
            if (param == "follow")
            {
                ok = DcSpectator::ToggleFollow(player, nullptr, &whyNot);
            }
            else if (param == "next")
            {
                ok = DcSpectator::CycleFollow(player, +1, &whyNot);
            }
            else if (param == "prev")
            {
                ok = DcSpectator::CycleFollow(player, -1, &whyNot);
            }
            else if (param.compare(0, 7, "follow\t") == 0)
            {
                // V-05: "follow\t<name>" — validate the named bot exists and is
                // in the same instance as the sender.
                std::string botName = param.substr(7);
                Player* target = ObjectAccessor::FindPlayerByName(botName, false);
                if (!target || !target->IsPlayerbot())
                {
                    SendAddonError(player, "Bot not found: " + botName);
                    return;
                }
                if (target->GetMapId() != player->GetMapId() ||
                    target->GetInstanceId() != player->GetInstanceId())
                {
                    SendAddonError(player,
                        "Bot is not in your instance: " + botName);
                    return;
                }
                ok = DcSpectator::ToggleFollow(player, target, &whyNot);
            }
            else
            {
                ok = DcSpectator::Toggle(player, &whyNot);
            }
            if (!ok)
                SendAddonError(player, whyNot);
            return;
        }

        // Piggyback the spectate-enabled flag on the addon's status poll: it's
        // the panel's heartbeat (sent on open and on combat transitions), so the
        // button stays in sync regardless of whether a tank bot is present.
        if (subCmd == "status")
            SendSpectateState(player);

        // V-10: Reject parameters containing tab or newline characters that
        // could inject unexpected fields into the action-dispatch protocol. The
        // in-process handlers (set, spectate) parse their own multi-field params
        // and are exempt.
        if (!IsValidParam(param))
        {
            SendAddonError(player, "Invalid parameter: contains forbidden characters.");
            return;
        }

        // V-01: Read-only commands (status, bosses) are allowed from any group
        // member. All mutating commands require run-owner authority.
        bool const readOnly = (subCmd == "status" || subCmd == "bosses");
        if (!readOnly && !IsRunOwner(player))
        {
            SendAddonError(player,
                "You are not the leader of this dungeon clear.");
            return;
        }

        // Map subcommand strings to action names.
        std::string action;
        if (subCmd == "on")         action = "dc on";
        else if (subCmd == "off")   action = "dc off";
        else if (subCmd == "skip")  action = "dc skip";
        else if (subCmd == "pause") action = "dc pause";
        else if (subCmd == "pull")
        {
            // V-06: Validate the pull parameter is one of the three allowed values.
            if (param != "off" && param != "on" && param != "dynamic")
            {
                SendAddonError(player,
                    "pull requires 'off', 'on', or 'dynamic'.");
                return;
            }
            action = "dc pull";
        }
        else if (subCmd == "status") action = "dc status";
        else if (subCmd == "bosses") action = "dc bosses";
        else if (subCmd == "go")
        {
            // V-02: Validate that the go parameter is a valid integer and
            // corresponds to a boss in the tank bot's current instance.
            if (param.empty())
            {
                SendAddonError(player, "go requires a creature entry id.");
                return;
            }

            char* end = nullptr;
            long const entry = std::strtol(param.c_str(), &end, 10);
            if (end == param.c_str() || *end != '\0' || entry <= 0)
            {
                SendAddonError(player, "Invalid creature entry: " + param);
                return;
            }

            Player* leader = DcLeaderSignal::FindLeaderTank(player);
            if (!leader)
            {
                SendAddonError(player, "No tank bot found in your group.");
                return;
            }

            if (!IsValidBossEntry(leader, static_cast<uint32>(entry)))
            {
                SendAddonError(player,
                    "Creature " + param + " is not a boss in this instance.");
                return;
            }

            action = "dc go";
            param = std::to_string(entry);
        }
        else
        {
            LOG_DEBUG("module", "mod-dungeon-clear: unknown addon subcommand '{}' from {}",
                      subCmd, player->GetName());
            return;
        }

        // Dispatch to the tank bot(s) silently (no PlaySound emotes). Relay
        // suppression is handled by OnPlayerCanUseChat — see the note there.
        if (!DungeonClearDispatch::DispatchToTankBots(player, action, param))
            SendAddonError(player, "No tank bot found in your group.");
    }
};

void AddSC_dungeon_clear_addon_hook()
{
    new DungeonClearAddonHookScript();
}
