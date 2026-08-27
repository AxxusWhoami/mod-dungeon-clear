/*
 * mod-dungeon-clear — BetterLootRollAction.cpp
 */

#include "BetterLootRollAction.h"

#include "Group.h"
#include "ObjectMgr.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomItemMgr.h"
#include "StatsWeightCalculator.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Ai/Dungeon/DungeonClear/Settings/DcSettings.h"

namespace
{
    // RandomItemMgr::CanEquip{Weapon,Armor} reordered their parameters between
    // playerbots branches: stock master is (clazz, [level,] proto); test-staging
    // is (proto, clazz[, level]). SFINAE-dispatch to whichever overload links so
    // the module compiles against either API.
    template <typename Mgr>
    auto DcCanEquipWeapon(Mgr& mgr, uint8 clazz, ItemTemplate const* proto, int)
        -> decltype(mgr.CanEquipWeapon(proto, clazz))
    {
        return mgr.CanEquipWeapon(proto, clazz);
    }
    template <typename Mgr>
    auto DcCanEquipWeapon(Mgr& mgr, uint8 clazz, ItemTemplate const* proto, long)
        -> decltype(mgr.CanEquipWeapon(clazz, proto))
    {
        return mgr.CanEquipWeapon(clazz, proto);
    }

    template <typename Mgr>
    auto DcCanEquipArmor(Mgr& mgr, uint8 clazz, uint32 level, ItemTemplate const* proto, int)
        -> decltype(mgr.CanEquipArmor(proto, clazz, level))
    {
        return mgr.CanEquipArmor(proto, clazz, level);
    }
    template <typename Mgr>
    auto DcCanEquipArmor(Mgr& mgr, uint8 clazz, uint32 level, ItemTemplate const* proto, long)
        -> decltype(mgr.CanEquipArmor(clazz, level, proto))
    {
        return mgr.CanEquipArmor(clazz, level, proto);
    }
}

bool DungeonClearBetterLootRollAction::isUseful()
{
    // Only intercept self-bots (master == bot). A bot driven for a separate
    // human master keeps stock rolling — its vote is its own GUID, no conflict.
    if (botAI->GetMaster() == bot && DcSettings::GetBool(bot, "BetterLootRolling"))
        return false;  // bot-self: cast no vote so the human gets to roll

    return LootRollAction::isUseful();
}

namespace
{
    // Queue a loot-roll vote as a CMSG_LOOT_ROLL packet so the World thread
    // processes it on its own update loop. Calling Group::CountRollVote
    // directly from the MapUpdater worker thread corrupts the Roll's std::map
    // when two bots roll concurrently — the root cause of the loot-roll
    // SIGSEGV. QueuePacket defers the handler to World::UpdateSessions, where
    // all Group mutations are safe.
    void QueueLootRoll(Player* bot, ObjectGuid itemGuid, uint32 itemSlot,
                       RollVote vote)
    {
        WorldPacket* p = new WorldPacket(CMSG_LOOT_ROLL, 8 + 4 + 1);
        *p << itemGuid;
        *p << itemSlot;
        *p << uint8(vote);
        bot->GetSession()->QueuePacket(p);
    }
}

bool DungeonClearBetterLootRollAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Roll*> rolls = group->GetRolls();
    for (Roll*& roll : rolls)
    {
        auto voteItr = roll->playerVote.find(bot->GetGUID());
        if (voteItr == roll->playerVote.end() || voteItr->second != NOT_EMITED_YET)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(roll->itemid);
        if (!proto)
            continue;

        RollVote vote;

        if (DcSettings::GetBool(bot, "BetterLootRolling") && IsFutureWearable(proto))
        {
            int32 randomProperty = 0;
            if (roll->itemRandomPropId)
                randomProperty = roll->itemRandomPropId;
            else if (roll->itemRandomSuffix)
                randomProperty = -((int)roll->itemRandomSuffix);

            vote = CalculateFutureVote(proto, randomProperty);

            if (vote == NEED)
            {
                if (sPlayerbotAIConfig.lootNeedRollLevel == 0 || RollUniqueCheck(proto, bot))
                    vote = PASS;
                else if (sPlayerbotAIConfig.lootNeedRollLevel == 1)
                    vote = GREED;
            }
            else if (vote == GREED && !sPlayerbotAIConfig.lootGreedRollLevel)
                vote = PASS;
        }
        else
        {
            // Stock vote calculation: ask ItemUsageValue what a real player
            // would do, then apply the same post-processing. Inlined here
            // (instead of calling LootRollAction::Execute) so the vote goes
            // through QueueLootRoll instead of the thread-unsafe direct call.
            ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", proto->ItemId);
            vote = (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE) ? NEED : GREED;

            if (vote == NEED)
            {
                if (sPlayerbotAIConfig.lootNeedRollLevel == 0 || RollUniqueCheck(proto, bot))
                    vote = PASS;
                else if (sPlayerbotAIConfig.lootNeedRollLevel == 1)
                    vote = GREED;
            }
            else if (vote == GREED && !sPlayerbotAIConfig.lootGreedRollLevel)
                vote = PASS;
        }

        switch (group->GetLootMethod())
        {
            case MASTER_LOOT:
            case FREE_FOR_ALL:
                QueueLootRoll(bot, roll->itemGUID, roll->itemSlot, PASS);
                break;
            default:
                QueueLootRoll(bot, roll->itemGUID, roll->itemSlot, vote);
                break;
        }
        return true;
    }

    return false;
}

bool DungeonClearBetterLootRollAction::IsFutureWearable(ItemTemplate const* proto) const
{
    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
        return false;

    if (proto->RequiredLevel <= bot->GetLevel())
        return false;

    // CanUseItem checks faction, class/race, skill and spell BEFORE level, so
    // this exact error means the level requirement is the only blocker.
    return bot->BotCanUseItem(proto) == EQUIP_ERR_CANT_EQUIP_LEVEL_I;
}

RollVote DungeonClearBetterLootRollAction::CalculateFutureVote(ItemTemplate const* proto, int32 randomProperty)
{
    // Proficiency judged at the item's required level, not the bot's current
    // one — a 35 warrior WILL wear level-42 plate (plate unlocks at 40).
    bool proficient = proto->Class == ITEM_CLASS_WEAPON
        ? DcCanEquipWeapon(sRandomItemMgr, bot->getClass(), proto, 0)
        : DcCanEquipArmor(sRandomItemMgr, bot->getClass(), proto->RequiredLevel, proto, 0);

    if (!proficient)
        return GREED;  // never their gear, but still vendor/AH value

    StatsWeightCalculator calculator(bot);
    calculator.SetItemSetBonus(false);
    calculator.SetOverflowPenalty(false);

    if (sRandomPlayerbotMgr.IsSpecPvp(bot->GetGUID().GetCounter(), bot->getClass()))
        calculator.SetPvpSpec(true);

    return calculator.CalculateItem(proto->ItemId, randomProperty) > 0 ? NEED : GREED;
}
