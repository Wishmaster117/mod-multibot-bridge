#include "Bag.h"
#include "Chat.h"
#include "CharacterCache.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "BudgetValues.h"
#include "ChatHelper.h"
#include "GameObject.h"
#include "Group.h"
#include "GuildMgr.h"
#include "Item.h"
#include "ItemPackets.h"
#include "ItemUsageValue.h"
#include "LootObjectStack.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestPackets.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "PlayerbotRepository.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ReputationMgr.h"
#include "AiObjectContext.h"
#include "Event.h"
#include "EventProcessor.h"
#include "Trigger.h"
#include "Formations.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "Unit.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
char const* const kAddonPrefix = "MBOT";
char const* const kAddonEnvelope = "MBOT\t";
char const* const kBridgeName = "mod-multibot-bridge";
char const* const kProtocolVersion = "1";
char const kFieldSeparator = '~';

std::size_t constexpr kMaxBridgeWireLength = 255;
std::size_t constexpr kMaxBridgePayloadLength = kMaxBridgeWireLength - 5;
std::size_t constexpr kMaxOpcodeLength = 24;
std::size_t constexpr kMaxRequestTypeLength = 32;
std::size_t constexpr kMaxBotNameLength = 64;
std::size_t constexpr kMaxTokenLength = 64;
std::size_t constexpr kMaxEncodedFieldLength = 192;
std::size_t constexpr kMaxCommandLength = 160;
std::size_t constexpr kMaxStateBots = 128;
std::size_t constexpr kMaxStateStrategiesPerScope = 256;
std::size_t constexpr kMaxStrategyOperations = 32;
std::size_t constexpr kMaxStrategyNameLength = 96;
std::size_t constexpr kMaxStrategyMatchedBots = 128;
std::size_t constexpr kStrategyMutationRateLimit = 24;
std::chrono::milliseconds constexpr kStrategyMutationRateWindow(2000);
std::size_t constexpr kItemActionRateLimit = 24;
std::chrono::milliseconds constexpr kItemActionRateWindow(2000);
std::size_t constexpr kInventoryExactRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryExactRateWindow(2000);
std::size_t constexpr kBankSnapshotRateLimit = 4;
std::chrono::milliseconds constexpr kBankSnapshotRateWindow(2000);
std::size_t constexpr kBankSnapshotMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemMoveRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemMoveRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemMoveReplayTtl(10);
std::size_t constexpr kInventoryItemMoveMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemMoveMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemTradeRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemTradeRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemTradeReplayTtl(10);
std::size_t constexpr kInventoryItemTradeMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemTradeMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemDepositExactRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemDepositExactRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemDepositExactReplayTtl(10);
std::size_t constexpr kInventoryItemDepositExactMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemDepositExactMaxRequesterStates = 512;
std::size_t constexpr kLootRuleItemRateLimit = 8;
std::chrono::milliseconds constexpr kLootRuleItemRateWindow(2000);
std::chrono::seconds constexpr kLootRuleItemReplayTtl(10);
std::size_t constexpr kLootRuleItemMaxRecentTokens = 32;
std::size_t constexpr kLootRuleItemMaxRequesterStates = 512;
std::size_t constexpr kLootRuleItemMaxMatchedBots = 128;
std::size_t constexpr kLootRuleItemPersistenceBudget = 128;
std::chrono::seconds constexpr kLootRuleItemPersistenceWindow(10);
std::size_t constexpr kInventoryItemEquipRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemEquipRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemEquipReplayTtl(10);
std::size_t constexpr kInventoryItemEquipMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemEquipMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemUnequipRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemUnequipRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemUnequipReplayTtl(10);
std::size_t constexpr kInventoryItemUnequipMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemUnequipMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemDestroyRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemDestroyRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemDestroyReplayTtl(10);
std::size_t constexpr kInventoryItemDestroyMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemDestroyMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemUseRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemUseRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemUseReplayTtl(10);
std::size_t constexpr kInventoryItemUseMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemUseMaxRequesterStates = 512;
std::size_t constexpr kInventoryItemSellRateLimit = 8;
std::chrono::milliseconds constexpr kInventoryItemSellRateWindow(2000);
std::chrono::seconds constexpr kInventoryItemSellReplayTtl(10);
std::size_t constexpr kInventoryItemSellMaxRecentTokens = 32;
std::size_t constexpr kInventoryItemSellMaxRequesterStates = 512;
std::size_t constexpr kVendorBuybackRateLimit = 8;
std::chrono::milliseconds constexpr kVendorBuybackRateWindow(2000);
std::chrono::seconds constexpr kVendorBuybackReplayTtl(10);
std::size_t constexpr kVendorBuybackMaxRecentTokens = 32;
std::size_t constexpr kVendorBuybackMaxRequesterStates = 512;
std::size_t constexpr kTalentApplyRateLimit = 4;
std::chrono::milliseconds constexpr kTalentApplyRateWindow(2000);
std::chrono::seconds constexpr kTalentApplyReplayTtl(10);
std::size_t constexpr kTalentApplyMaxRecentTokens = 32;
std::size_t constexpr kTalentApplyMaxRequesterStates = 512;
std::size_t constexpr kMaxTalentApplyBuildLength = 128;
std::size_t constexpr kTalentSpecApplyRateLimit = 4;
std::chrono::milliseconds constexpr kTalentSpecApplyRateWindow(2000);
std::chrono::seconds constexpr kTalentSpecApplyReplayTtl(10);
std::size_t constexpr kTalentSpecApplyMaxRecentTokens = 32;
std::size_t constexpr kTalentSpecApplyMaxRequesterStates = 512;
std::size_t constexpr kQuestAbandonRateLimit = 4;
std::chrono::milliseconds constexpr kQuestAbandonRateWindow(2000);
std::chrono::seconds constexpr kQuestAbandonReplayTtl(10);
std::size_t constexpr kQuestAbandonMaxRecentTokens = 32;
std::size_t constexpr kQuestAbandonMaxRequesterStates = 512;
std::size_t constexpr kQuestProgressRateLimit = 48;
std::chrono::milliseconds constexpr kQuestProgressRateWindow(2000);
std::size_t constexpr kQuestProgressMaxRequesterStates = 512;
std::size_t constexpr kGroupRollRateLimit = 4;
std::chrono::milliseconds constexpr kGroupRollRateWindow(2000);
std::size_t constexpr kSelfBotRateLimit = 8;
std::chrono::milliseconds constexpr kSelfBotRateWindow(2000);
std::size_t constexpr kSelfBotMaxRequesterStates = 512;
std::size_t constexpr kAltRosterRateLimit = 4;
std::chrono::milliseconds constexpr kAltRosterRateWindow(2000);
std::size_t constexpr kAltRosterMaxRequesterStates = 512;
std::size_t constexpr kMaxAltRosterEntries = 128;
std::size_t constexpr kBotLifecycleQueryRateLimit = 256;
std::size_t constexpr kBotLifecycleMutationRateLimit = 64;
std::chrono::milliseconds constexpr kBotLifecycleRateWindow(2000);
std::chrono::seconds constexpr kBotLifecycleReplayTtl(10);
std::chrono::seconds constexpr kBotLifecycleConnectTimeout(10);
std::chrono::seconds constexpr kBotLifecyclePendingRetention(60);
std::size_t constexpr kBotLifecycleMaxRecentTokens = 320;
std::size_t constexpr kBotLifecycleMaxRequesterStates = 512;
std::size_t constexpr kBotLifecycleMaxPendingConnects = 64;
std::chrono::seconds constexpr kSelfBotHeavyActionRateWindow(10);
std::size_t constexpr kSelfBotHeavyActionMaxRequesterStates = 512;
std::size_t constexpr kWarlockStoneSwitchMaxPending = 512;
std::size_t constexpr kWarlockStoneSwitchMaxApplyAttempts = 20;
std::chrono::milliseconds constexpr kWarlockStoneSwitchApplyRetryDelay(100);
std::chrono::milliseconds constexpr kWarlockStoneSwitchCreateTimeout(7000);
std::size_t constexpr kEnchantTradeRateLimit = 4;
std::chrono::milliseconds constexpr kEnchantTradeRateWindow(2000);
std::size_t constexpr kMaxEnchantTradeEntries = 256;
std::size_t constexpr kCraftRecipeTargetRateLimit = 4;
std::chrono::milliseconds constexpr kCraftRecipeTargetRateWindow(2000);
std::chrono::seconds constexpr kCraftRecipeTargetReplayTtl(10);
std::size_t constexpr kCraftRecipeTargetMaxRecentTokens = 32;
std::size_t constexpr kCraftRecipeTargetMaxRequesterStates = 512;
std::size_t constexpr kMaxGroupRollItemLinkLength = 160;
char const* const kStateFramingCapability = "STATE_FRAMING_V1";
char const* const kStrategyMutationCapability = "STRATEGY_MUTATION_V1";
char const* const kOutfitCapability = "OUTFIT_V1";
char const* const kInventoryCapability = "INVENTORY_V1";
char const* const kInventoryExactCapability = "INVENTORY_EXACT_V1";
char const* const kInventoryItemMoveCapability = "ITEM_MOVE_V1";
char const* const kInventoryItemTradeCapability = "ITEM_TRADE_V1";
char const* const kInventoryItemDepositExactCapability = "ITEM_DEPOSIT_EXACT_V1";
char const* const kInventoryItemEquipCapability = "ITEM_EQUIP_V1";
char const* const kInventoryItemUnequipCapability = "ITEM_UNEQUIP_V1";
char const* const kInventoryItemDestroyCapability = "ITEM_DESTROY_V1";
char const* const kInventoryItemUseCapability = "ITEM_USE_V1";
char const* const kInventoryItemSellCapability = "ITEM_SELL_SINGLE_V1";
char const* const kVendorBuybackCapability = "VENDOR_BUYBACK_V1";
char const* const kInventoryBulkSellCapability = "INVENTORY_BULK_SELL_V1";
char const* const kInventoryOpenCapability = "INVENTORY_OPEN_V1";
char const* const kLootRuleItemCapability = "LOOT_RULE_ITEM_V1";
char const* const kQuestAbandonCapability = "QUEST_ABANDON_V1";
char const* const kQuestProgressCapability = "QUEST_PROGRESS_V1";
char const* const kQuestProgressPushCapability = "QUEST_PROGRESS_PUSH_V1";
char const* const kTalentApplyCapability = "TALENT_APPLY_V1";
char const* const kTalentSpecApplyCapability = "TALENT_SPEC_APPLY_V1";
char const* const kCraftRecipeTargetCapability = "CRAFT_RECIPE_TARGET_V1";
char const* const kGroupRollCapability = "GROUP_ROLL_V1";
char const* const kEnchantTradeCapability = "ENCHANT_TRADE_V1";
char const* const kSelfBotCapability = "SELF_BOT_V1";
char const* const kSelfStrategyCapability = "SELF_STRATEGY_V1";
char const* const kSelfActionCapability = "SELF_ACTION_V1";
char const* const kAltRosterCapability = "ALT_ROSTER_V1";
char const* const kBotLifecycleCapability = "BOT_LIFECYCLE_V1";
char const* const kBotTargetResolveCapability = "BOT_TARGET_RESOLVE_V1";
uint32 constexpr kMaxItemActionCount = 1000;
uint32 constexpr kMaxInventoryItemMoveCount = 1000;
uint32 constexpr kMaxInventoryItemTradeCount = 1000;
uint32 constexpr kMaxInventoryItemDepositExactCount = 1000;
uint32 constexpr kMaxInventoryItemEquipCount = 1000;
uint32 constexpr kMaxInventoryItemDestroyCount = 1000;
uint32 constexpr kMaxInventoryItemUseCount = 1000;
uint32 constexpr kMaxInventoryItemSellCount = 1000;
uint32 constexpr kMaxVendorBuybackCount = 1000;
uint32 constexpr kQuestProgressWatchIntervalMs = 500;

enum class BridgePayloadStatus
{
    NotBridge,
    Valid,
    Invalid
};

bool BridgeConsoleLogsEnabled()
{
    return sConfigMgr->GetOption<bool>("MultiBotBridge.EnableConsoleLogs", true);
}

Player* FindBotByName(Player* player, std::string const& botName);
PlayerbotAI* GetBotAI(Player* bot);
bool ConsumeSelfBotRequestRateLimit(Player* requester);
bool ConsumeSelfBotHeavyActionRateLimit(Player* requester);
std::vector<Player*> GetBridgeVisibleBots(Player* player);
void SendAddonPacket(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& payload = "");
bool SendStateAddonPacket(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& payload);
bool SendProtocolError(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& requestType, std::string const& token, std::string const& reason);
void SendOutfitPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void SendInventoryExactSnapshot(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void RunInventoryItemMoveCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, uint8 srcBag, uint8 srcSlot, uint32 srcItemId, uint32 srcCount, uint8 dstBag, uint8 dstSlot, uint32 dstItemId, uint32 dstCount);
void RunInventoryItemTradeCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, uint8 srcBag, uint8 srcSlot, uint32 srcItemId, uint32 srcCount);
void RunInventoryItemDepositExactCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& actionValue, uint8 srcBag, uint8 srcSlot, uint32 srcItemId, uint32 srcCount);
void RunLootRuleItemCommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& actionValue, uint32 itemId);
void SendTrainerPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void RunOutfitCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& encodedSuffix, std::string const& persistToken);
void RunTrainerLearnCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& trainerEntryValue, std::string const& spellIdValue);
void RunProfessionRecipeCraftCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& skillIdValue, std::string const& spellIdValue, std::string const& itemIdValue);
void RunProfessionRecipeTargetCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, uint32 skillId, uint32 spellId, uint32 targetBag, uint32 targetSlot, uint32 targetItemId);
void SendEnchantTradePackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void RunEnchantTradeCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& spellIdValue);
void RunInventoryItemActionCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& actionValue, std::string const& itemIdValue, std::string const& countValue);
void RunTalentApplyCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& build);
void RunTalentSpecApplyCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, uint32 slot, uint32 specIndex);
void RunQuestAbandonCommand(Player* requester, ChatMsg replyType, std::string const& requestToken, uint32 questId);
void RunGroupRollCommand(Player* requester, ChatMsg replyType, std::string const& requestToken, std::string const& modeValue, std::string const& encodedItemLink);
void RunFormationCommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& encodedFormation);
void SendFormationPackets(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken);
void SendBotReputationPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void SendBotEmblemPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void SendQuestProgressPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, bool watch);
std::array<uint32, 3> BuildTalentTabPoints(Player* bot);
uint32 GetPct(uint32 current, uint32 max);

std::string Trim(std::string const& value)
{
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string ToUpper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::toupper(c); });
    return value;
}

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

std::size_t GetAddonWireLength(std::string const& opcode, std::string const& payload)
{
    std::size_t length = std::char_traits<char>::length(kAddonEnvelope) + opcode.size();
    if (!payload.empty())
        length += 1 + payload.size();
    return length;
}

bool IsAddonPacketWithinBudget(std::string const& opcode, std::string const& payload)
{
    return GetAddonWireLength(opcode, payload) <= kMaxBridgeWireLength;
}

bool SendCapabilitiesPackets(Player* player, ChatMsg chatType)
{
    char const* const capabilities[] =
    {
        kStateFramingCapability,
        kStrategyMutationCapability,
        kOutfitCapability,
        kInventoryCapability,
        kInventoryExactCapability,
        kInventoryItemMoveCapability,
        kInventoryItemTradeCapability,
        kInventoryItemDepositExactCapability,
        kInventoryItemEquipCapability,
        kInventoryItemUnequipCapability,
        kInventoryItemDestroyCapability,
        kInventoryItemUseCapability,
        kInventoryItemSellCapability,
        kVendorBuybackCapability,
        kInventoryBulkSellCapability,
        kInventoryOpenCapability,
        kLootRuleItemCapability,
        kQuestAbandonCapability,
        kQuestProgressCapability,
        kQuestProgressPushCapability,
        kTalentApplyCapability,
        kTalentSpecApplyCapability,
        kCraftRecipeTargetCapability,
        kGroupRollCapability,
        kEnchantTradeCapability,
        kSelfBotCapability,
        kSelfStrategyCapability,
        kSelfActionCapability,
        kAltRosterCapability,
        kBotLifecycleCapability,
        kBotTargetResolveCapability
    };

    std::vector<std::string> chunks;
    std::string chunk;

    for (char const* const capability : capabilities)
    {
        std::string const candidate = chunk.empty() ? std::string(capability) : chunk + "," + capability;
        if (IsAddonPacketWithinBudget("CAPS", candidate))
        {
            chunk = candidate;
            continue;
        }

        if (chunk.empty() || !IsAddonPacketWithinBudget("CAPS", capability))
            return false;

        chunks.push_back(chunk);
        chunk = capability;
    }

    if (!chunk.empty())
        chunks.push_back(chunk);

    SendAddonPacket(player, chatType, "CAPS_BEGIN");
    for (std::string const& capabilityChunk : chunks)
        SendAddonPacket(player, chatType, "CAPS", capabilityChunk);
    SendAddonPacket(player, chatType, "CAPS_END");
    return true;
}

std::pair<std::string, std::string> SplitOnce(std::string const& value, char separator)
{
    size_t const pos = value.find(separator);
    if (pos == std::string::npos)
        return {value, ""};

    return {value.substr(0, pos), value.substr(pos + 1)};
}

std::vector<std::string> SplitFields(std::string const& value)
{
    std::vector<std::string> fields;
    std::size_t start = 0;

    while (true)
    {
        std::size_t const pos = value.find(kFieldSeparator, start);
        if (pos == std::string::npos)
        {
            fields.push_back(value.substr(start));
            break;
        }

        fields.push_back(value.substr(start, pos - start));
        start = pos + 1;
    }

    return fields;
}

bool HasControlCharacter(std::string const& value)
{
    for (unsigned char const c : value)
        if (c < 0x20 || c == 0x7F)
            return true;

    return false;
}

bool IsValidProtocolName(std::string const& value, std::size_t maxLength)
{
    if (value.empty() || value.size() > maxLength)
        return false;

    for (unsigned char const c : value)
        if (!std::isalnum(c) && c != '_')
            return false;

    return true;
}

bool IsValidRawField(std::string const& value, std::size_t maxLength, bool allowEmpty)
{
    if (value.size() > maxLength || HasControlCharacter(value))
        return false;

    return allowEmpty || !value.empty();
}

bool IsValidCanonicalRawField(std::string const& value, std::size_t maxLength, bool allowEmpty)
{
    return value == Trim(value) && IsValidRawField(value, maxLength, allowEmpty);
}

bool IsValidRequestToken(std::string const& value)
{
    if (!IsValidCanonicalRawField(value, kMaxTokenLength, false))
        return false;

    for (unsigned char const c : value)
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.' && c != ':')
            return false;

    return true;
}

int HexDigitValue(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    c = static_cast<unsigned char>(std::toupper(c));
    if (c >= 'A' && c <= 'F')
        return 10 + c - 'A';

    return -1;
}

bool TryUrlDecodeField(std::string const& value, std::string& out, std::size_t maxDecodedLength, bool allowEmpty)
{
    if (value.size() > kMaxEncodedFieldLength)
        return false;

    out.clear();
    out.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        unsigned char decoded = static_cast<unsigned char>(value[i]);

        if (value[i] == '%')
        {
            if (i + 2 >= value.size())
                return false;

            int const high = HexDigitValue(static_cast<unsigned char>(value[i + 1]));
            int const low = HexDigitValue(static_cast<unsigned char>(value[i + 2]));
            if (high < 0 || low < 0)
                return false;

            decoded = static_cast<unsigned char>((high << 4) | low);
            i += 2;
        }

        if (decoded < 0x20 || decoded == 0x7F)
            return false;

        out.push_back(static_cast<char>(decoded));
        if (out.size() > maxDecodedLength)
            return false;
    }

    return allowEmpty || !out.empty();
}

bool IsValidEncodedField(std::string const& value, std::size_t maxDecodedLength, bool allowEmpty)
{
    std::string decoded;
    return TryUrlDecodeField(value, decoded, maxDecodedLength, allowEmpty);
}

bool TryParseUint32Field(std::string const& value, uint32 minValue, uint32 maxValue, uint32& parsed)
{
    std::string const canonical = Trim(value);
    if (canonical.empty() || canonical != value || canonical.size() > 10)
        return false;

    uint64 result = 0;
    for (unsigned char const c : canonical)
    {
        if (!std::isdigit(c))
            return false;

        result = result * 10 + static_cast<uint64>(c - '0');
        if (result > maxValue)
            return false;
    }

    if (result < minValue)
        return false;

    parsed = static_cast<uint32>(result);
    return true;
}

std::string GetSafeErrorToken(std::vector<std::string> const& fields, std::size_t index)
{
    if (index >= fields.size() || !IsValidRequestToken(fields[index]))
        return "";

    return fields[index];
}

std::string SanitizeLogValue(std::string const& value, std::size_t maxLength)
{
    std::string out;
    out.reserve(std::min(value.size(), maxLength));

    for (unsigned char const c : value)
    {
        if (out.size() >= maxLength)
            break;

        if (c < 0x20 || c == 0x7F)
            out.push_back('?');
        else
            out.push_back(static_cast<char>(c));
    }

    if (value.size() > maxLength)
        out += "...";

    return out;
}

BridgePayloadStatus TryExtractBridgePayload(uint32 lang, std::string const& msg, std::string& payload, std::string& reason)
{
    payload.clear();
    reason.clear();

    if (lang != LANG_ADDON)
        return BridgePayloadStatus::NotBridge;

    std::size_t const envelopeLength = std::char_traits<char>::length(kAddonEnvelope);
    if (msg.size() < envelopeLength || msg.compare(0, envelopeLength, kAddonEnvelope) != 0)
        return BridgePayloadStatus::NotBridge;

    if (msg.size() > kMaxBridgeWireLength)
    {
        reason = "WIRE_TOO_LONG";
        return BridgePayloadStatus::Invalid;
    }

    payload = msg.substr(envelopeLength);
    if (payload.empty())
    {
        reason = "EMPTY_PACKET";
        return BridgePayloadStatus::Invalid;
    }

    if (payload.size() > kMaxBridgePayloadLength)
    {
        reason = "PAYLOAD_TOO_LONG";
        return BridgePayloadStatus::Invalid;
    }

    if (HasControlCharacter(payload))
    {
        reason = "CONTROL_CHARACTER";
        return BridgePayloadStatus::Invalid;
    }

    return BridgePayloadStatus::Valid;
}

std::string UrlEncodeField(std::string const& value)
{
    std::ostringstream out;
    char const* const hex = "0123456789ABCDEF";

    for (unsigned char c : value)
    {
        if (c == '%' || c == '~' || c == '\r' || c == '\n')
        {
            out << '%';
            out << hex[(c >> 4) & 0x0F];
            out << hex[c & 0x0F];
        }
        else
            out << static_cast<char>(c);
    }

    return out.str();
}

std::string UrlDecodeField(std::string const& value)
{
    std::string decoded;
    if (!TryUrlDecodeField(value, decoded, kMaxEncodedFieldLength, true))
        return "";

    return decoded;
}

struct InventorySummaryData
{
    uint32 gold = 0;
    uint32 silver = 0;
    uint32 copper = 0;
    uint32 bagUsed = 0;
    uint32 bagTotal = 16;
};

struct StatsData
{
    std::string name;
    uint32 level = 0;
    uint32 gold = 0;
    uint32 silver = 0;
    uint32 copper = 0;
    uint32 bagUsed = 0;
    uint32 bagTotal = 0;
    uint32 durabilityPct = 0;
    uint32 xpPct = 0;
    uint32 manaPct = 0;
};

struct SpellbookEntryData
{
    uint32 spellId = 0;
    uint32 schoolMask = 0;
    std::string spellName;
};

struct BotSkillEntryData
{
    uint32 skillId = 0;
    std::string category;
    std::string key;
    std::string name;
    uint32 value = 0;
    uint32 maxValue = 0;
};

struct BotReputationEntryData
{
    uint32 factionId = 0;
    std::string name;
    uint32 rank = 0;
    int32 value = 0;
    int32 maxValue = 0;
};

struct SkillDefinition
{
    uint32 skillId = 0;
    char const* key = "";
    char const* name = "";
    char const* category = "";
};

struct BotDetailData
{
    std::string name;
    std::string race;
    std::string gender;
    std::string className;
    uint32 level = 0;
    std::array<uint32, 3> talentTabs = {0, 0, 0};
    uint32 itemLevelScore = 0;
};

struct ProfessionSkillDefinition
{
    uint32 skillId = 0;
    char const* key = "";
};

std::array<ProfessionSkillDefinition, 14> const kProfessionSkillDefinitions = {{
    { SKILL_ALCHEMY, "alchemy" },
    { SKILL_BLACKSMITHING, "blacksmithing" },
    { SKILL_ENCHANTING, "enchanting" },
    { SKILL_ENGINEERING, "engineering" },
    { SKILL_HERBALISM, "herbalism" },
    { SKILL_INSCRIPTION, "inscription" },
    { SKILL_JEWELCRAFTING, "jewelcrafting" },
    { SKILL_LEATHERWORKING, "leatherworking" },
    { SKILL_MINING, "mining" },
    { SKILL_SKINNING, "skinning" },
    { SKILL_TAILORING, "tailoring" },
    { SKILL_COOKING, "cooking" },
    { SKILL_FIRST_AID, "firstaid" },
    { SKILL_FISHING, "fishing" },
}};

std::array<SkillDefinition, 11> const kPrimaryProfessionSkillDefinitions = {{
    { SKILL_ALCHEMY, "alchemy", "Alchemy", "profession" },
    { SKILL_BLACKSMITHING, "blacksmithing", "Blacksmithing", "profession" },
    { SKILL_ENCHANTING, "enchanting", "Enchanting", "profession" },
    { SKILL_ENGINEERING, "engineering", "Engineering", "profession" },
    { SKILL_HERBALISM, "herbalism", "Herbalism", "profession" },
    { SKILL_INSCRIPTION, "inscription", "Inscription", "profession" },
    { SKILL_JEWELCRAFTING, "jewelcrafting", "Jewelcrafting", "profession" },
    { SKILL_LEATHERWORKING, "leatherworking", "Leatherworking", "profession" },
    { SKILL_MINING, "mining", "Mining", "profession" },
    { SKILL_SKINNING, "skinning", "Skinning", "profession" },
    { SKILL_TAILORING, "tailoring", "Tailoring", "profession" },
}};

std::array<SkillDefinition, 3> const kSecondarySkillDefinitions = {{
    { SKILL_COOKING, "cooking", "Cooking", "secondary" },
    { SKILL_FIRST_AID, "firstaid", "First Aid", "secondary" },
    { SKILL_FISHING, "fishing", "Fishing", "secondary" },
}};

std::array<SkillDefinition, 28> const kClassSkillDefinitions = {{
    { 6, "frost", "Frost", "class" },
    { 8, "fire", "Fire", "class" },
    { 26, "arms", "Arms", "class" },
    { 38, "combat", "Combat", "class" },
    { 39, "subtlety", "Subtlety", "class" },
    { 50, "beastmastery", "Beast Mastery", "class" },
    { 51, "survival", "Survival", "class" },
    { 56, "holy", "Holy", "class" },
    { 78, "shadow", "Shadow Magic", "class" },
    { 134, "feralcombat", "Feral Combat", "class" },
    { 163, "marksmanship", "Marksmanship", "class" },
    { 184, "retribution", "Retribution", "class" },
    { 237, "arcane", "Arcane", "class" },
    { 253, "assassination", "Assassination", "class" },
    { 256, "fury", "Fury", "class" },
    { 257, "protection", "Protection", "class" },
    { 267, "paladinprotection", "Protection", "class" },
    { 354, "demonology", "Demonology", "class" },
    { 355, "affliction", "Affliction", "class" },
    { 373, "enhancement", "Enhancement", "class" },
    { 374, "restoration", "Restoration", "class" },
    { 375, "elemental", "Elemental Combat", "class" },
    { 573, "druidrestoration", "Restoration", "class" },
    { 574, "balance", "Balance", "class" },
    { 593, "destruction", "Destruction", "class" },
    { 613, "discipline", "Discipline", "class" },
    { 770, "blood", "Blood", "class" },
    { 771, "deathknightfrost", "Frost", "class" },
}};

std::array<SkillDefinition, 23> const kWeaponSkillDefinitions = {{
    { 43, "swords", "Swords", "weapon" },
    { 44, "axes", "Axes", "weapon" },
    { 45, "bows", "Bows", "weapon" },
    { 46, "guns", "Guns", "weapon" },
    { 54, "maces", "Maces", "weapon" },
    { 55, "twohandedswords", "Two-Handed Swords", "weapon" },
    { 95, "defense", "Defense", "weapon" },
    { 118, "dualwield", "Dual Wield", "weapon" },
    { 136, "staves", "Staves", "weapon" },
    { 160, "twohandedmaces", "Two-Handed Maces", "weapon" },
    { 162, "unarmed", "Unarmed", "weapon" },
    { 172, "twohandedaxes", "Two-Handed Axes", "weapon" },
    { 173, "daggers", "Daggers", "weapon" },
    { 176, "thrown", "Thrown", "weapon" },
    { 226, "crossbows", "Crossbows", "weapon" },
    { 228, "wands", "Wands", "weapon" },
    { 229, "polearms", "Polearms", "weapon" },
    { 473, "fistweapons", "Fist Weapons", "weapon" },
    { 293, "platemail", "Plate Mail", "armor" },
    { 413, "mail", "Mail", "armor" },
    { 414, "leather", "Leather", "armor" },
    { 415, "cloth", "Cloth", "armor" },
    { 433, "shield", "Shield", "armor" },
}};

struct PvpStatsData
{
    std::string name;
    uint32 arenaPoints = 0;
    uint32 honorPoints = 0;

    struct TeamData
    {
        std::string name;
        uint32 rating = 0;
    };

    std::array<TeamData, 3> teams;
};


struct QuestEntryData
{
    uint32 questId = 0;
    bool completed = false;
};

struct QuestProgressRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, QuestProgressRateState> sQuestProgressRateStates;

void PruneQuestProgressRateState(
    QuestProgressRateState& state,
    std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() &&
           now - state.requests.front() >= kQuestProgressRateWindow)
    {
        state.requests.pop_front();
    }
}

bool ConsumeQuestProgressRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();

    auto stateIt = sQuestProgressRateStates.find(key);

    if (stateIt == sQuestProgressRateStates.end())
    {
        if (sQuestProgressRateStates.size() >= kQuestProgressMaxRequesterStates)
        {
            for (auto it = sQuestProgressRateStates.begin();
                 it != sQuestProgressRateStates.end();)
            {
                PruneQuestProgressRateState(it->second, now);

                if (it->second.requests.empty())
                    it = sQuestProgressRateStates.erase(it);
                else
                    ++it;
            }
        }

        if (sQuestProgressRateStates.size() >= kQuestProgressMaxRequesterStates)
            return false;

        stateIt =
            sQuestProgressRateStates.emplace(key, QuestProgressRateState()).first;
    }

    QuestProgressRateState& state = stateIt->second;
    PruneQuestProgressRateState(state, now);

    if (state.requests.size() >= kQuestProgressRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

struct QuestProgressSubscriber
{
    ObjectGuid requesterGuid;
    ChatMsg replyType = CHAT_MSG_WHISPER;
    uint64 lastFingerprint = 0;
};

struct QuestProgressWatchState
{
    uint32 elapsedMs = 0;
    std::vector<QuestProgressSubscriber> subscribers;
};

std::mutex gQuestProgressWatchMutex;
std::map<uint32, QuestProgressWatchState> gQuestProgressWatches;
uint32 gQuestProgressPushSequence = 0;

struct TalentSpecEntryData
{
    uint32 index = 0;
    std::string name;
    std::string build;
};

std::string GetRaceName(uint8 raceId)
{
    switch (raceId)
    {
        case 1:
            return "Human";
        case 2:
            return "Orc";
        case 3:
            return "Dwarf";
        case 4:
            return "Night Elf";
        case 5:
            return "Undead";
        case 6:
            return "Tauren";
        case 7:
            return "Gnome";
        case 8:
            return "Troll";
        case 10:
            return "Blood Elf";
        case 11:
            return "Draenei";
        default:
            return "Unknown";
    }
}

std::string GetClassName(uint8 classId)
{
    switch (classId)
    {
        case 1:
            return "Warrior";
        case 2:
            return "Paladin";
        case 3:
            return "Hunter";
        case 4:
            return "Rogue";
        case 5:
            return "Priest";
        case 6:
            return "DeathKnight";
        case 7:
            return "Shaman";
        case 8:
            return "Mage";
        case 9:
            return "Warlock";
        case 11:
            return "Druid";
        default:
            return "Unknown";
    }
}

uint32 GetTalentRankPoints(TalentEntry const* talentInfo, uint32 spellId)
{
    if (!talentInfo)
        return 1;

    for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
    {
        if (talentInfo->RankID[rank] == spellId)
            return rank + 1;
    }

    return 1;
}

std::array<uint32, 3> BuildTalentTabPoints(Player* bot)
{
    std::array<uint32, 3> tabs = {0, 0, 0};
    if (!bot)
        return tabs;

    uint8 const activeSpecMask = bot->GetActiveSpecMask();

    for (PlayerTalentMap::const_iterator it = bot->GetTalentMap().begin(); it != bot->GetTalentMap().end(); ++it)
    {
        PlayerTalent const* const playerTalent = it->second;
        if (!playerTalent)
            continue;

        if (playerTalent->State == PLAYERSPELL_REMOVED)
            continue;

        if (playerTalent->specMask && !(playerTalent->specMask & activeSpecMask))
            continue;

        TalentEntry const* const talentInfo = sTalentStore.LookupEntry(playerTalent->talentID);
        if (!talentInfo)
            continue;

        TalentTabEntry const* const talentTab = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTab || talentTab->tabpage >= tabs.size())
            continue;

        tabs[talentTab->tabpage] += GetTalentRankPoints(talentInfo, it->first);
    }

    return tabs;
}

uint32 BuildItemLevelScore(Player* bot)
{
    if (!bot)
        return 0;

    uint32 score = 0;
    bool hasMainHand = false;
    bool mainHandIsTwoHanded = false;
    bool hasOffHand = false;
    bool const hasTitanGrip = bot->HasSpell(49152);

    for (uint8 slot = EQUIPMENT_SLOT_START; slot <= EQUIPMENT_SLOT_RANGED; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_BODY)
            continue;

        Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        ItemTemplate const* const proto = item->GetTemplate();
        if (!proto)
            continue;

        if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            hasMainHand = true;
            mainHandIsTwoHanded = proto->InventoryType == INVTYPE_2HWEAPON;
        }
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
            hasOffHand = true;

        score += proto->ItemLevel;
    }

    uint32 const divisor = ((hasMainHand && !mainHandIsTwoHanded) || (hasMainHand && hasTitanGrip) || hasOffHand) ? 17 : 16;
    if (!divisor)
        return 0;

    return score / divisor;
}

BotDetailData BuildBotDetail(Player* bot)
{
    BotDetailData detail;
    if (!bot)
        return detail;

    detail.name = bot->GetName();
    detail.race = GetRaceName(static_cast<uint8>(bot->getRace()));
    detail.gender = static_cast<uint8>(bot->getGender()) == 1 ? "Female" : "Male";
    detail.className = GetClassName(static_cast<uint8>(bot->getClass()));
    detail.level = bot->GetLevel();
    detail.talentTabs = BuildTalentTabPoints(bot);
    detail.itemLevelScore = BuildItemLevelScore(bot);
    return detail;
}

std::string BuildBotDetailPayload(Player* bot)
{
    BotDetailData const detail = BuildBotDetail(bot);
    if (detail.name.empty())
        return "";

    std::ostringstream out;
    out << UrlEncodeField(detail.name) << kFieldSeparator << UrlEncodeField(detail.race) << kFieldSeparator
        << UrlEncodeField(detail.gender) << kFieldSeparator << UrlEncodeField(detail.className) << kFieldSeparator
        << detail.level << kFieldSeparator << detail.talentTabs[0] << kFieldSeparator << detail.talentTabs[1]
        << kFieldSeparator << detail.talentTabs[2] << kFieldSeparator << detail.itemLevelScore;
    return out.str();
}

SkillLineAbilityEntry const* GetSkillLineAbilityForSpell(uint32 spellId)
{
    static bool initialized = false;
    static std::map<uint32, SkillLineAbilityEntry const*> spellSkillLines;

    if (!initialized)
    {
        initialized = true;
        for (uint32 index = 0; index < sSkillLineAbilityStore.GetNumRows(); ++index)
            if (SkillLineAbilityEntry const* const skillLine = sSkillLineAbilityStore.LookupEntry(index))
                if (skillLine->Spell)
                    spellSkillLines[skillLine->Spell] = skillLine;
    }

    auto const it = spellSkillLines.find(spellId);
    return it != spellSkillLines.end() ? it->second : nullptr;
}

bool IsSecondarySkillLine(uint32 skillId)
{
    for (SkillDefinition const& definition : kSecondarySkillDefinitions)
        if (definition.skillId == skillId)
            return true;

    return false;
}

bool IsSpellbookExcludedSkillLine(uint32 skillId)
{
    SkillLineEntry const* const skillLine = sSkillLineStore.LookupEntry(skillId);
    if (skillLine && skillLine->categoryId == SKILL_CATEGORY_PROFESSION)
        return true;

    return IsSecondarySkillLine(skillId);
}

bool IsSpellbookExcludedSpell(uint32 spellId)
{
    SkillLineAbilityEntry const* const skillLine = GetSkillLineAbilityForSpell(spellId);
    return skillLine && IsSpellbookExcludedSkillLine(skillLine->SkillLine);
}

void AddSkillEntry(Player* bot, SkillDefinition const& definition, std::vector<BotSkillEntryData>& entries, std::set<uint32>& seen)
{
    if (!bot || !definition.skillId || !seen.insert(definition.skillId).second)
        return;

    uint32 const value = bot->GetSkillValue(definition.skillId);
    uint32 const maxValue = bot->GetMaxSkillValue(definition.skillId);
    if (!value && !maxValue)
        return;

    BotSkillEntryData entry;
    entry.skillId = definition.skillId;
    entry.category = definition.category;
    entry.key = definition.key;
    entry.name = definition.name;
    entry.value = value;
    entry.maxValue = maxValue;
    entries.push_back(entry);
}

std::vector<BotSkillEntryData> BuildBotSkillEntries(Player* bot)
{
    std::vector<BotSkillEntryData> entries;
    std::set<uint32> seen;
    if (!bot)
        return entries;

    for (SkillDefinition const& definition : kClassSkillDefinitions)
        AddSkillEntry(bot, definition, entries, seen);

    for (SkillDefinition const& definition : kPrimaryProfessionSkillDefinitions)
        AddSkillEntry(bot, definition, entries, seen);

    for (SkillDefinition const& definition : kSecondarySkillDefinitions)
        AddSkillEntry(bot, definition, entries, seen);

    for (SkillDefinition const& definition : kWeaponSkillDefinitions)
        AddSkillEntry(bot, definition, entries, seen);

    return entries;
}

std::string BuildBotSkillEntryPayload(Player* bot, std::string const& token, BotSkillEntryData const& entry)
{
    std::ostringstream out;
    out << UrlEncodeField(bot->GetName())
        << kFieldSeparator << token
        << kFieldSeparator << UrlEncodeField(entry.category)
        << kFieldSeparator << entry.skillId
        << kFieldSeparator << UrlEncodeField(entry.key)
        << kFieldSeparator << UrlEncodeField(entry.name)
        << kFieldSeparator << entry.value
        << kFieldSeparator << entry.maxValue;
    return out.str();
}

int32 GetReputationRankBase(ReputationRank rank)
{
    int32 base = ReputationMgr::Reputation_Cap + 1;
    for (int32 i = MAX_REPUTATION_RANK - 1; i >= static_cast<int32>(rank); --i)
        base -= ReputationMgr::PointsInRank[i];

    return base;
}

BotReputationEntryData BuildBotReputationEntry(Player* bot, FactionEntry const* entry)
{
    BotReputationEntryData data;
    if (!bot || !entry)
        return data;

    ReputationMgr& reputationMgr = bot->GetReputationMgr();
    ReputationRank const rank = reputationMgr.GetRank(entry);
    int32 const reputation = reputationMgr.GetReputation(entry->ID);
    int32 const maxValue = ReputationMgr::PointsInRank[rank];
    int32 value = reputation - GetReputationRankBase(rank);

    if (value < 0)
        value = 0;
    if (value > maxValue)
        value = maxValue;

    data.factionId = entry->ID;
    data.name = entry->name[0];
    data.rank = static_cast<uint32>(rank);
    data.value = value;
    data.maxValue = maxValue;
    return data;
}

std::vector<BotReputationEntryData> BuildBotReputationEntries(Player* bot)
{
    std::vector<BotReputationEntryData> entries;
    if (!bot)
        return entries;

    ReputationMgr& reputationMgr = bot->GetReputationMgr();
    FactionStateList const& stateList = reputationMgr.GetStateList();
    entries.reserve(stateList.size());

    for (auto const& itr : stateList)
    {
        FactionState const& faction = itr.second;
        if (!(faction.Flags & FACTION_FLAG_VISIBLE))
            continue;

        if (faction.Flags & (FACTION_FLAG_HIDDEN | FACTION_FLAG_INVISIBLE_FORCED) &&
            !(faction.Flags & FACTION_FLAG_SPECIAL))
            continue;

        FactionEntry const* const entry = sFactionStore.LookupEntry(faction.ID);
        if (!entry)
            continue;

        entries.push_back(BuildBotReputationEntry(bot, entry));
    }

    std::sort(entries.begin(), entries.end(), [](BotReputationEntryData const& left, BotReputationEntryData const& right)
    {
        return left.name < right.name;
    });

    return entries;
}

std::string BuildBotReputationEntryPayload(Player* bot, std::string const& token, BotReputationEntryData const& entry)
{
    std::ostringstream out;
    out << UrlEncodeField(bot->GetName())
        << kFieldSeparator << token
        << kFieldSeparator << entry.factionId
        << kFieldSeparator << UrlEncodeField(entry.name)
        << kFieldSeparator << entry.rank
        << kFieldSeparator << entry.value
        << kFieldSeparator << entry.maxValue;
    return out.str();
}

std::string BuildBotProfessionPayload(Player* bot)
{
    if (!bot)
        return "";

    std::ostringstream out;
    out << UrlEncodeField(bot->GetName()) << kFieldSeparator;

    bool first = true;
    for (ProfessionSkillDefinition const& profession : kProfessionSkillDefinitions)
    {
        uint32 const value = bot->GetSkillValue(profession.skillId);
        uint32 const maxValue = bot->GetMaxSkillValue(profession.skillId);
        if (!value && !maxValue)
            continue;

        if (!first)
            out << ';';
        first = false;

        std::ostringstream token;
        token << profession.key << ':' << value << '/' << maxValue;
        out << UrlEncodeField(token.str());
    }

    return first ? "" : out.str();
}

uint8 ArenaTeamTypeToPayloadIndex(uint8 type)
{
    switch (type)
    {
        case 2:
            return 0;
        case 3:
            return 1;
        case 5:
            return 2;
        default:
            return 3;
    }
}

PvpStatsData BuildPvpStatsData(Player* bot)
{
    PvpStatsData data;
    if (!bot)
        return data;

    data.name = bot->GetName();
    data.arenaPoints = bot->GetArenaPoints();
    data.honorPoints = bot->GetHonorPoints();

    QueryResult result = CharacterDatabase.Query(
        "SELECT at.type, at.name, at.rating "
        "FROM arena_team_member atm "
        "INNER JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId "
        "WHERE atm.guid = {}",
        bot->GetGUID().GetCounter());

    if (!result)
        return data;

    do
    {
        Field* const fields = result->Fetch();
        uint8 const type = fields[0].Get<uint8>();
        uint8 const index = ArenaTeamTypeToPayloadIndex(type);
        if (index >= data.teams.size())
            continue;

        data.teams[index].name = fields[1].Get<std::string>();
        data.teams[index].rating = fields[2].Get<uint32>();
    }
    while (result->NextRow());

    return data;
}

std::string BuildPvpStatsPayload(Player* bot)
{
    PvpStatsData const data = BuildPvpStatsData(bot);
    if (data.name.empty())
        return "";

    std::ostringstream out;
    out << UrlEncodeField(data.name)
        << kFieldSeparator << data.arenaPoints
        << kFieldSeparator << data.honorPoints;

    for (PvpStatsData::TeamData const& team : data.teams)
    {
        out << kFieldSeparator << UrlEncodeField(team.name)
            << kFieldSeparator << team.rating;
    }

    return out.str();
}

uint32 CountTalentLinkTreePoints(std::string const& tree)
{
    uint32 points = 0;
    for (char const c : tree)
    {
        if (c >= '0' && c <= '9')
            points += static_cast<uint32>(c - '0');
    }

    return points;
}

std::string BuildTalentLinkPointSummary(std::string const& link)
{
    std::array<std::string, 3> trees = {"", "", ""};
    uint8 treeIndex = 0;

    for (char const c : link)
    {
        if (c == '-')
        {
            if (treeIndex < 2)
                ++treeIndex;
            continue;
        }

        if (treeIndex < trees.size())
            trees[treeIndex].push_back(c);
    }

    std::ostringstream out;
    out << CountTalentLinkTreePoints(trees[0]) << '-' << CountTalentLinkTreePoints(trees[1])
        << '-' << CountTalentLinkTreePoints(trees[2]);
    return out.str();
}

std::string GetPremadeSpecConfigString(std::string const& key)
{
    return Trim(sConfigMgr->GetOption<std::string>(key, ""));
}

std::string GetPremadeSpecLink(uint8 classId, uint32 specIndex, uint32 botLevel)
{
    std::vector<uint32> levels;
    levels.push_back(botLevel);
    levels.push_back(80);
    levels.push_back(70);
    levels.push_back(60);
    levels.push_back(40);
    levels.push_back(20);

    std::set<uint32> seen;
    for (uint32 const level : levels)
    {
        if (!level || seen.find(level) != seen.end())
            continue;

        seen.insert(level);

        std::ostringstream key;
        key << "AiPlayerbot.PremadeSpecLink." << static_cast<uint32>(classId) << '.' << specIndex << '.' << level;

        std::string const link = GetPremadeSpecConfigString(key.str());
        if (!link.empty())
            return link;
    }

    return "";
}

// MB_TALENT_SPEC_LEVEL_ADJUSTED_VERIFY_V1_BEGIN
void SimulateTalentSpecLearn(
    Player* bot,
    TalentEntry const* talent,
    uint32 talentRank,
    uint32& freePoints,
    std::map<uint32, uint32>& currentRanks,
    std::map<uint32, uint32>& pointsByTalentTab,
    std::array<uint32, 3>& expectedTabs)
{
    if (!bot || !talent || !freePoints || talentRank >= MAX_TALENT_RANK)
        return;

    TalentTabEntry const* const tabInfo = sTalentTabStore.LookupEntry(talent->TalentTab);
    if (!tabInfo || tabInfo->tabpage >= expectedTabs.size() ||
        !(tabInfo->ClassMask & bot->getClassMask()))
    {
        return;
    }

    uint32 const currentRank = currentRanks[talent->TalentID];
    uint32 const targetRank = talentRank + 1;
    if (currentRank >= targetRank)
        return;

    uint32 const pointCost = targetRank - currentRank;
    if (freePoints < pointCost)
        return;

    if (talent->DependsOn > 0)
    {
        if (TalentEntry const* const dependency = sTalentStore.LookupEntry(talent->DependsOn))
        {
            uint32 const dependencyRank = currentRanks[dependency->TalentID];
            if (dependencyRank <= talent->DependsOnRank)
                return;
        }
    }

    if (talent->Row > 0 &&
        pointsByTalentTab[talent->TalentTab] < talent->Row * MAX_TALENT_RANK)
    {
        return;
    }

    uint32 const spellId = talent->RankID[talentRank];
    if (!spellId || !sSpellMgr->GetSpellInfo(spellId))
        return;

    currentRanks[talent->TalentID] = targetRank;
    pointsByTalentTab[talent->TalentTab] += pointCost;
    expectedTabs[tabInfo->tabpage] += pointCost;
    freePoints -= pointCost;
}

bool BuildLevelAdjustedTalentSpecTabs(
    Player* bot,
    uint32 specIndex,
    std::array<uint32, 3>& expectedTabs)
{
    expectedTabs = {0, 0, 0};

    if (!bot || specIndex >= MAX_SPECNO)
        return false;

    uint32 const classId = bot->getClass();
    if (classId >= MAX_CLASSES)
        return false;

    uint32 freePoints = bot->CalculateTalentsPoints();
    if (!freePoints)
        return false;

    std::map<uint32, TalentEntry const*> talentsByPosition;
    uint32 const classMask = bot->getClassMask();
    for (uint32 index = 0; index < sTalentStore.GetNumRows(); ++index)
    {
        TalentEntry const* const talent = sTalentStore.LookupEntry(index);
        if (!talent)
            continue;

        TalentTabEntry const* const tabInfo = sTalentTabStore.LookupEntry(talent->TalentTab);
        if (!tabInfo || tabInfo->tabpage >= expectedTabs.size() ||
            !(tabInfo->ClassMask & classMask))
        {
            continue;
        }

        uint32 const positionKey =
            (static_cast<uint32>(tabInfo->tabpage) << 16) |
            (static_cast<uint32>(talent->Row) << 8) |
            static_cast<uint32>(talent->Col);
        talentsByPosition[positionKey] = talent;
    }

    std::map<uint32, uint32> currentRanks;
    std::map<uint32, uint32> pointsByTalentTab;

    int startLevel = static_cast<int>(bot->GetLevel());
    while (startLevel > 1 && startLevel < 80 &&
           sPlayerbotAIConfig.parsedSpecLinkOrder[classId][specIndex][startLevel].empty())
    {
        --startLevel;
    }

    bool sawTemplate = false;
    for (int level = startLevel; level <= 80 && freePoints; ++level)
    {
        std::vector<std::vector<uint32>> const& order =
            sPlayerbotAIConfig.parsedSpecLinkOrder[classId][specIndex][level];
        if (order.empty())
            continue;

        sawTemplate = true;
        for (std::vector<uint32> const& parsedTalent : order)
        {
            if (parsedTalent.size() != 4 ||
                parsedTalent[0] >= expectedTabs.size() ||
                !parsedTalent[3])
            {
                return false;
            }

            uint32 const positionKey =
                (parsedTalent[0] << 16) |
                (parsedTalent[1] << 8) |
                parsedTalent[2];

            auto const talentIt = talentsByPosition.find(positionKey);
            if (talentIt == talentsByPosition.end())
                return false;

            TalentEntry const* const talent = talentIt->second;

            if (talent->DependsOn && freePoints)
            {
                if (TalentEntry const* const dependency =
                        sTalentStore.LookupEntry(talent->DependsOn))
                {
                    uint32 const dependencyRank =
                        std::min<uint32>(talent->DependsOnRank, freePoints - 1);
                    SimulateTalentSpecLearn(
                        bot,
                        dependency,
                        dependencyRank,
                        freePoints,
                        currentRanks,
                        pointsByTalentTab,
                        expectedTabs);
                }
            }

            if (!freePoints)
                break;

            uint32 const talentRank =
                std::min<uint32>(parsedTalent[3], freePoints) - 1;
            SimulateTalentSpecLearn(
                bot,
                talent,
                talentRank,
                freePoints,
                currentRanks,
                pointsByTalentTab,
                expectedTabs);

            if (!freePoints)
                break;
        }
    }

    return sawTemplate &&
        (expectedTabs[0] + expectedTabs[1] + expectedTabs[2]) > 0;
}

std::string FormatTalentSpecPointSummary(std::array<uint32, 3> const& tabs)
{
    std::ostringstream summary;
    summary << tabs[0] << '-' << tabs[1] << '-' << tabs[2];
    return summary.str();
}
// MB_TALENT_SPEC_LEVEL_ADJUSTED_VERIFY_V1_END

std::vector<TalentSpecEntryData> BuildTalentSpecEntries(Player* bot)
{
    std::vector<TalentSpecEntryData> entries;
    if (!bot)
        return entries;

    uint8 const classId = static_cast<uint8>(bot->getClass());

    for (uint32 specIndex = 0; specIndex <= 30; ++specIndex)
    {
        std::ostringstream nameKey;
        nameKey << "AiPlayerbot.PremadeSpecName." << static_cast<uint32>(classId) << '.' << specIndex;

        std::string const specName = GetPremadeSpecConfigString(nameKey.str());
        if (specName.empty())
            continue;

        TalentSpecEntryData entry;
        entry.index = specIndex;
        entry.name = specName;

        std::string const link = GetPremadeSpecLink(classId, specIndex, bot->GetLevel());
        std::array<uint32, 3> levelAdjustedTabs = {0, 0, 0};
        if (!link.empty() &&
            !BuildTalentLinkPointSummary(link).empty() &&
            BuildLevelAdjustedTalentSpecTabs(bot, specIndex, levelAdjustedTabs))
        {
            entry.build = FormatTalentSpecPointSummary(levelAdjustedTabs);
        }

        entries.push_back(entry);
    }

    return entries;
}

void SendTalentSpecListPackets(Player* requester, ChatMsg replyType, std::string const& botNameValue, std::string const& tokenValue)
{
    std::string const requestedBotName = Trim(botNameValue);
    std::string const token = Trim(tokenValue);
    Player* const bot = FindBotByName(requester, requestedBotName);

    std::string const effectiveBotName = bot ? bot->GetName() : requestedBotName;
    std::string const headerPayload = UrlEncodeField(effectiveBotName) + std::string(1, kFieldSeparator) + token;

    SendAddonPacket(requester, replyType, "TALENT_SPEC_BEGIN", headerPayload);

    if (bot)
    {
        std::array<uint32, 3> const currentTabs = BuildTalentTabPoints(bot);
        std::ostringstream currentPayload;
        currentPayload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << token
            << kFieldSeparator << (static_cast<uint32>(bot->GetActiveSpec()) + 1)
            << kFieldSeparator << currentTabs[0]
            << kFieldSeparator << currentTabs[1]
            << kFieldSeparator << currentTabs[2];
        SendAddonPacket(requester, replyType, "TALENT_SPEC_CURRENT", currentPayload.str());

        std::vector<TalentSpecEntryData> const specs = BuildTalentSpecEntries(bot);
        for (TalentSpecEntryData const& spec : specs)
        {
            std::ostringstream payload;
            payload << UrlEncodeField(bot->GetName())
                << kFieldSeparator << token
                << kFieldSeparator << spec.index
                << kFieldSeparator << UrlEncodeField(spec.name)
                << kFieldSeparator << spec.build;

            SendAddonPacket(requester, replyType, "TALENT_SPEC_ITEM", payload.str());
        }
    }

    SendAddonPacket(requester, replyType, "TALENT_SPEC_END", headerPayload);
}

uint32 FindGlyphItemId(uint32 glyphId, uint32 spellId)
{
    if (!glyphId && !spellId)
        return 0;

    static std::map<uint32, uint32> glyphItemCache;
    uint32 const cacheKey = glyphId ? glyphId : spellId;
    std::map<uint32, uint32>::const_iterator const cached = glyphItemCache.find(cacheKey);
    if (cached != glyphItemCache.end())
        return cached->second;

    uint32 itemId = 0;
    if (spellId)
    {
        QueryResult direct = WorldDatabase.Query(
            "SELECT entry FROM item_template "
            "WHERE class = 16 AND (spellid_1 = {} OR spellid_2 = {} OR spellid_3 = {} OR spellid_4 = {} OR spellid_5 = {}) "
            "LIMIT 1",
            spellId, spellId, spellId, spellId, spellId);

        if (direct)
            itemId = direct->Fetch()[0].Get<uint32>();
    }

    if (!itemId && glyphId)
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT entry, spellid_1, spellid_2, spellid_3, spellid_4, spellid_5 "
            "FROM item_template WHERE class = 16");

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                for (uint8 i = 0; i < 5; ++i)
                {
                    uint32 const itemSpellId = fields[i + 1].Get<uint32>();
                    if (!itemSpellId)
                        continue;

                    SpellInfo const* const itemSpellInfo = sSpellMgr->GetSpellInfo(itemSpellId);
                    if (!itemSpellInfo)
                        continue;

                    for (uint8 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
                    {
                        if (itemSpellInfo->Effects[effectIndex].MiscValue == static_cast<int32>(glyphId))
                        {
                            itemId = fields[0].Get<uint32>();
                            break;
                        }
                    }

                    if (itemId)
                        break;
                }
            } while (!itemId && result->NextRow());
        }
    }

    glyphItemCache[cacheKey] = itemId;
    return itemId;
}

void SendGlyphPackets(Player* requester, ChatMsg replyType, std::string const& botNameValue, std::string const& tokenValue)
{
    std::string const requestedBotName = Trim(botNameValue);
    std::string const token = Trim(tokenValue);
    Player* const bot = FindBotByName(requester, requestedBotName);

    std::string const effectiveBotName = bot ? bot->GetName() : requestedBotName;
    std::string const headerPayload = UrlEncodeField(effectiveBotName) + std::string(1, kFieldSeparator) + token;

    SendAddonPacket(requester, replyType, "GLYPHS_BEGIN", headerPayload);

    if (bot)
    {
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        {
            uint32 const glyphId = bot->GetGlyph(slot);
            if (!glyphId)
                continue;

            GlyphPropertiesEntry const* const glyph = sGlyphPropertiesStore.LookupEntry(glyphId);
            uint32 const spellId = glyph ? glyph->SpellId : 0;
            uint32 const itemId = FindGlyphItemId(glyphId, spellId);

            std::ostringstream payload;
            payload << UrlEncodeField(bot->GetName())
                << kFieldSeparator << token
                << kFieldSeparator << static_cast<uint32>(slot + 1)
                << kFieldSeparator << itemId
                << kFieldSeparator << glyphId
                << kFieldSeparator << spellId
                << kFieldSeparator;

            SendAddonPacket(requester, replyType, "GLYPHS_ITEM", payload.str());
        }
    }

    SendAddonPacket(requester, replyType, "GLYPHS_END", headerPayload);
}

std::string NormalizeQuestMode(std::string const& mode)
{
    std::string normalized = ToUpper(Trim(mode));
    if (normalized != "INCOMPLETED" && normalized != "COMPLETED" && normalized != "ALL")
        normalized = "ALL";

    return normalized;
}

bool ShouldSendQuestForMode(std::string const& mode, bool completed)
{
    if (mode == "ALL")
        return true;

    if (mode == "COMPLETED")
        return completed;

    return !completed;
}

void AppendQuestEntry(std::vector<QuestEntryData>& entries, std::set<uint32>& seen, uint32 questId, bool completed, std::string const& mode)
{
    if (!questId || seen.find(questId) != seen.end())
        return;

    if (!ShouldSendQuestForMode(mode, completed))
        return;

    QuestEntryData entry;
    entry.questId = questId;
    entry.completed = completed;
    entries.push_back(entry);
    seen.insert(questId);
}

void SortQuestEntries(std::vector<QuestEntryData>& entries)
{
    std::sort(entries.begin(), entries.end(), [](QuestEntryData const& left, QuestEntryData const& right)
    {
        return left.questId < right.questId;
    });
}

std::vector<QuestEntryData> BuildQuestEntries(Player* bot, std::string const& mode)
{
    std::vector<QuestEntryData> entries;
    std::set<uint32> seen;
    if (!bot)
        return entries;

    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 const questId = bot->GetQuestSlotQuestId(slot);
        if (!questId)
            continue;

        QuestStatus const status = bot->GetQuestStatus(questId);
        if (status == QUEST_STATUS_COMPLETE)
            AppendQuestEntry(entries, seen, questId, true, mode);
        else if (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_FAILED)
            AppendQuestEntry(entries, seen, questId, false, mode);
    }

    if (!entries.empty())
    {
        SortQuestEntries(entries);
        return entries;
    }

    // Fallback DB uniquement si le quest log runtime est vide.
    // Certains forks stockent les quêtes actives avec un statut DB brut 0/1/3,
    // qui ne correspond pas toujours directement à l'enum runtime QuestStatus.

    QueryResult result = CharacterDatabase.Query(
        "SELECT quest, status FROM character_queststatus WHERE guid = {}",
        bot->GetGUID().GetCounter());

    if (!result)
        return entries;

    do
    {
        Field* const fields = result->Fetch();
        uint32 const questId = fields[0].Get<uint32>();
        uint8 const status = fields[1].Get<uint8>();

        bool completed = false;
        if (status == static_cast<uint8>(QUEST_STATUS_COMPLETE) || status == 1)
            completed = true;
        else if (status == static_cast<uint8>(QUEST_STATUS_INCOMPLETE) || status == static_cast<uint8>(QUEST_STATUS_FAILED) || status == 0 || status == 3)
            completed = false;
        else
            continue;

        AppendQuestEntry(entries, seen, questId, completed, mode);
    }
    while (result->NextRow());

    SortQuestEntries(entries);
    return entries;
}

void SendQuestPacketsForBot(Player* requester, ChatMsg replyType, Player* bot, std::string const& mode, std::string const& token)
{
    if (!requester || !bot)
        return;

    std::string const botName = bot->GetName();

    std::string const headerPayload = UrlEncodeField(botName) + std::string(1, kFieldSeparator) + token
        + std::string(1, kFieldSeparator) + mode;
    SendAddonPacket(requester, replyType, "QUESTS_BEGIN", headerPayload);

    std::vector<QuestEntryData> const entries = BuildQuestEntries(bot, mode);
    for (QuestEntryData const& entry : entries)
    {
        std::ostringstream payload;
        payload << UrlEncodeField(botName)
            << kFieldSeparator << token
            << kFieldSeparator << mode
            << kFieldSeparator << (entry.completed ? "C" : "I")
            << kFieldSeparator << entry.questId
            << kFieldSeparator << UrlEncodeField(std::to_string(entry.questId));

        SendAddonPacket(requester, replyType, "QUESTS_ITEM", payload.str());
    }

    SendAddonPacket(requester, replyType, "QUESTS_END", headerPayload);
}

void SendQuestPackets(Player* player, ChatMsg replyType, std::string const& modeValue, std::string const& botNameValue, std::string const& tokenValue)
{
    std::string const mode = NormalizeQuestMode(modeValue);
    std::string const botName = Trim(botNameValue);
    std::string const token = Trim(tokenValue);

    if (!botName.empty())
    {
        Player* const bot = FindBotByName(player, botName);
        if (bot)
            SendQuestPacketsForBot(player, replyType, bot, mode, token);

        SendAddonPacket(player, replyType, "QUESTS_DONE", token + std::string(1, kFieldSeparator) + mode);
        return;
    }

    for (Player* const bot : GetBridgeVisibleBots(player))
        SendQuestPacketsForBot(player, replyType, bot, mode, token);

    SendAddonPacket(player, replyType, "QUESTS_DONE", token + std::string(1, kFieldSeparator) + mode);
}

uint64 MixQuestProgressHash(uint64 hash, uint64 value)
{
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    return hash;
}

uint64 BuildQuestProgressFingerprint(Player* bot)
{
    if (!bot)
        return 0;

    uint64 hash = 1469598103934665603ULL;
    QuestStatusMap const& questStatusMap = bot->getQuestStatusMap();

    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 const questId = bot->GetQuestSlotQuestId(slot);
        hash = MixQuestProgressHash(hash, questId);

        if (!questId)
            continue;

        QuestStatus const status = bot->GetQuestStatus(questId);
        hash = MixQuestProgressHash(hash, static_cast<uint32>(status));

        auto const statusItr = questStatusMap.find(questId);
        if (statusItr == questStatusMap.end())
            continue;

        QuestStatusData const& progress = statusItr->second;

        for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            hash = MixQuestProgressHash(hash, progress.CreatureOrGOCount[i]);

        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            hash = MixQuestProgressHash(hash, progress.ItemCount[i]);

        hash = MixQuestProgressHash(hash, progress.PlayerCount);
        hash = MixQuestProgressHash(hash, progress.Explored ? 1 : 0);
    }

    return hash;
}

void SendQuestProgressObjectivePacket(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& token, uint32 questId, char objectiveType, uint8 sourceSlot, uint32 objectiveId, uint32 current, uint32 required)
{
    if (!requester || !required)
        return;

    current = std::min(current, required);

    std::ostringstream payload;
    payload << UrlEncodeField(botName)
        << kFieldSeparator << token
        << kFieldSeparator << questId
        << kFieldSeparator << objectiveType
        << kFieldSeparator << static_cast<uint32>(sourceSlot)
        << kFieldSeparator << objectiveId
        << kFieldSeparator << current
        << kFieldSeparator << required;

    SendAddonPacket(requester, replyType, "QUEST_PROGRESS_OBJECTIVE", payload.str());
}

void SendQuestProgressSnapshot(Player* requester, ChatMsg replyType, Player* bot, std::string const& token)
{
    if (!requester || !bot)
        return;

    std::string const botName = bot->GetName();
    std::string const headerPayload = UrlEncodeField(botName) + std::string(1, kFieldSeparator) + token;
    QuestStatusMap const& questStatusMap = bot->getQuestStatusMap();

    SendAddonPacket(requester, replyType, "QUEST_PROGRESS_BEGIN", headerPayload);

    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 const questId = bot->GetQuestSlotQuestId(slot);
        if (!questId)
            continue;

        QuestStatus const status = bot->GetQuestStatus(questId);
        if (status != QUEST_STATUS_INCOMPLETE && status != QUEST_STATUS_COMPLETE && status != QUEST_STATUS_FAILED)
            continue;

        std::ostringstream payload;
        payload << UrlEncodeField(botName)
            << kFieldSeparator << token
            << kFieldSeparator << questId
            << kFieldSeparator << (status == QUEST_STATUS_COMPLETE ? "C" : status == QUEST_STATUS_FAILED ? "F" : "I");

        SendAddonPacket(requester, replyType, "QUEST_PROGRESS_QUEST", payload.str());

        Quest const* const quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        auto const statusItr = questStatusMap.find(questId);
        if (statusItr == questStatusMap.end())
            continue;

        QuestStatusData const& progress = statusItr->second;

        for (uint8 objectiveIndex = 0; objectiveIndex < QUEST_OBJECTIVES_COUNT; ++objectiveIndex)
        {
            int32 const targetId = quest->RequiredNpcOrGo[objectiveIndex];
            uint32 const required = quest->RequiredNpcOrGoCount[objectiveIndex];

            if (!targetId || !required)
                continue;

            char const objectiveType = targetId > 0 ? 'm' : 'o';
            uint32 const objectiveId = targetId > 0 ? static_cast<uint32>(targetId) : static_cast<uint32>(-targetId);

            SendQuestProgressObjectivePacket(requester, replyType, botName, token, questId, objectiveType, objectiveIndex + 1, objectiveId, progress.CreatureOrGOCount[objectiveIndex], required);
        }

        for (uint8 objectiveIndex = 0; objectiveIndex < QUEST_ITEM_OBJECTIVES_COUNT; ++objectiveIndex)
        {
            uint32 const itemId = quest->RequiredItemId[objectiveIndex];
            uint32 const required = quest->RequiredItemCount[objectiveIndex];

            if (!itemId || !required)
                continue;

            SendQuestProgressObjectivePacket(requester, replyType, botName, token, questId, 'i', objectiveIndex + 1, itemId, progress.ItemCount[objectiveIndex], required);
        }

        uint32 const requiredPlayerKills = quest->GetPlayersSlain();
        if (requiredPlayerKills)
            SendQuestProgressObjectivePacket(requester, replyType, botName, token, questId, 'p', 1, 0, progress.PlayerCount, requiredPlayerKills);

        if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT))
            SendQuestProgressObjectivePacket(requester, replyType, botName, token, questId, 'e', 1, 0, progress.Explored ? 1 : 0, 1);
    }

    SendAddonPacket(requester, replyType, "QUEST_PROGRESS_END", headerPayload);
}

void RegisterQuestProgressWatch(Player* requester, ChatMsg replyType, Player* bot)
{
    if (!requester || !bot)
        return;

    Group* const botGroup = bot->GetGroup();
    if (!botGroup || requester->GetGroup() != botGroup)
        return;

    uint32 const botGuid = bot->GetGUID().GetCounter();
    uint64 const fingerprint = BuildQuestProgressFingerprint(bot);

    std::lock_guard<std::mutex> guard(gQuestProgressWatchMutex);

    QuestProgressWatchState& watch = gQuestProgressWatches[botGuid];

    for (QuestProgressSubscriber& subscriber : watch.subscribers)
    {
        if (subscriber.requesterGuid == requester->GetGUID())
        {
            subscriber.replyType = replyType;
            subscriber.lastFingerprint = fingerprint;
            return;
        }
    }

    QuestProgressSubscriber subscriber;
    subscriber.requesterGuid = requester->GetGUID();
    subscriber.replyType = replyType;
    subscriber.lastFingerprint = fingerprint;
    watch.subscribers.push_back(subscriber);
}

void SendQuestProgressPackets(Player* requester, ChatMsg replyType, std::string const& botNameValue, std::string const& token, bool watch)
{
    if (!requester)
        return;

    Player* const bot = FindBotByName(requester, botNameValue);
    if (!bot)
    {
        SendAddonPacket(requester, replyType, "QUEST_PROGRESS_END", UrlEncodeField(botNameValue) + std::string(1, kFieldSeparator) + token + std::string(1, kFieldSeparator) + "NO_BOT");
        return;
    }

    if (watch)
        RegisterQuestProgressWatch(requester, replyType, bot);

    SendQuestProgressSnapshot(requester, replyType, bot, token);
}

std::string MakeQuestProgressPushToken(Player* bot)
{
    ++gQuestProgressPushSequence;
    return "qpp" + std::to_string(bot->GetGUID().GetCounter()) + "x" + std::to_string(gQuestProgressPushSequence);
}

void UpdateQuestProgressWatch(Player* bot, uint32 diff)
{
    if (!bot)
        return;

    std::lock_guard<std::mutex> guard(gQuestProgressWatchMutex);

    uint32 const botGuid = bot->GetGUID().GetCounter();
    auto watchItr = gQuestProgressWatches.find(botGuid);
    if (watchItr == gQuestProgressWatches.end())
        return;

    QuestProgressWatchState& watch = watchItr->second;
    watch.elapsedMs += diff;

    if (watch.elapsedMs < kQuestProgressWatchIntervalMs)
        return;

    watch.elapsedMs = 0;

    Group* const botGroup = bot->GetGroup();
    if (!botGroup)
    {
        gQuestProgressWatches.erase(watchItr);
        return;
    }

    uint64 const fingerprint = BuildQuestProgressFingerprint(bot);

    for (auto subscriberItr = watch.subscribers.begin(); subscriberItr != watch.subscribers.end();)
    {
        Player* const requester = ObjectAccessor::FindConnectedPlayer(subscriberItr->requesterGuid);

        if (!requester || requester->GetGroup() != botGroup)
        {
            subscriberItr = watch.subscribers.erase(subscriberItr);
            continue;
        }

        if (fingerprint != subscriberItr->lastFingerprint)
        {
            SendQuestProgressSnapshot(requester, subscriberItr->replyType, bot, MakeQuestProgressPushToken(bot));
            subscriberItr->lastFingerprint = fingerprint;
        }

        ++subscriberItr;
    }

    if (watch.subscribers.empty())
        gQuestProgressWatches.erase(watchItr);
}

void RemoveQuestProgressWatchesForPlayer(Player* player)
{
    if (!player)
        return;

    ObjectGuid const playerGuid = player->GetGUID();
    uint32 const playerLowGuid = playerGuid.GetCounter();

    std::lock_guard<std::mutex> guard(gQuestProgressWatchMutex);

    gQuestProgressWatches.erase(playerLowGuid);

    for (auto watchItr = gQuestProgressWatches.begin(); watchItr != gQuestProgressWatches.end();)
    {
        std::vector<QuestProgressSubscriber>& subscribers = watchItr->second.subscribers;

        subscribers.erase(
            std::remove_if(
                subscribers.begin(),
                subscribers.end(),
                [&playerGuid](QuestProgressSubscriber const& subscriber)
                {
                    return subscriber.requesterGuid == playerGuid;
                }),
            subscribers.end());

        if (subscribers.empty())
            watchItr = gQuestProgressWatches.erase(watchItr);
        else
            ++watchItr;
    }
}

void AppendGameObjectUnitLines(PlayerbotAI* botAI, std::vector<std::string>& lines, std::string const& title, std::string const& valueName)
{
    lines.push_back(title);
    if (!botAI || !botAI->GetAiObjectContext())
        return;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    GuidVector const units = *context->GetValue<GuidVector>(valueName);
    for (ObjectGuid const guid : units)
        if (Unit* const unit = botAI->GetUnit(guid))
            lines.push_back(unit->GetNameForLocaleIdx(sWorld->GetDefaultDbcLocale()));
}

void AppendGameObjectLines(PlayerbotAI* botAI, std::vector<std::string>& lines, std::string const& title, std::string const& valueName)
{
    lines.push_back(title);
    if (!botAI || !botAI->GetAiObjectContext())
        return;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    GuidVector const objects = *context->GetValue<GuidVector>(valueName);
    for (ObjectGuid const guid : objects)
        if (GameObject* const go = botAI->GetGameObject(guid))
            lines.push_back(ChatHelper::FormatGameobject(go));
}

std::vector<std::string> BuildGameObjectResultLines(Player* bot)
{
    std::vector<std::string> lines;
    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    AppendGameObjectUnitLines(botAI, lines, "--- Targets ---", "possible targets");
    AppendGameObjectUnitLines(botAI, lines, "--- Targets (All) ---", "all targets");
    AppendGameObjectUnitLines(botAI, lines, "--- NPCs ---", "nearest npcs");
    AppendGameObjectUnitLines(botAI, lines, "--- Corpses ---", "nearest corpses");
    AppendGameObjectLines(botAI, lines, "--- Game objects ---", "nearest game objects");
    return lines;
}

void SendGameObjectPacketsForBot(Player* requester, ChatMsg replyType, Player* bot, std::string const& token)
{
    if (!requester || !bot)
        return;

    std::string const botName = bot->GetName();
    std::string const headerPayload = UrlEncodeField(botName) + std::string(1, kFieldSeparator) + token;
    SendAddonPacket(requester, replyType, "GAMEOBJECTS_BEGIN", headerPayload);

    for (std::string const& line : BuildGameObjectResultLines(bot))
        SendAddonPacket(requester, replyType, "GAMEOBJECTS_ITEM", headerPayload + std::string(1, kFieldSeparator) + UrlEncodeField(line));

    SendAddonPacket(requester, replyType, "GAMEOBJECTS_END", headerPayload);
}

void SendGameObjectPackets(Player* player, ChatMsg replyType, std::string const& botNameValue, std::string const& tokenValue)
{
    std::string const botName = Trim(botNameValue);
    std::string const token = Trim(tokenValue);

    if (!botName.empty())
    {
        if (Player* const bot = FindBotByName(player, botName))
            SendGameObjectPacketsForBot(player, replyType, bot, token);
        SendAddonPacket(player, replyType, "GAMEOBJECTS_DONE", token);
        return;
    }

    for (Player* const bot : GetBridgeVisibleBots(player))
        SendGameObjectPacketsForBot(player, replyType, bot, token);

    SendAddonPacket(player, replyType, "GAMEOBJECTS_DONE", token);
}

static bool CompareSpellbookEntries(SpellbookEntryData const& left, SpellbookEntryData const& right)
{
    if (left.schoolMask != right.schoolMask)
        return left.schoolMask > right.schoolMask;

    if (left.spellName != right.spellName)
        return left.spellName > right.spellName;

    return left.spellId < right.spellId;
}

std::vector<SpellbookEntryData> BuildSpellbookEntries(Player* bot)
{
    std::vector<SpellbookEntryData> entries;
    if (!bot)
        return entries;

    std::set<std::string> seenNames;

    for (PlayerSpellMap::const_iterator it = bot->GetSpellMap().begin(); it != bot->GetSpellMap().end(); ++it)
    {
        if (!it->second)
            continue;

        if (it->second->State == PLAYERSPELL_REMOVED || !it->second->Active)
            continue;

        if (!(it->second->specMask & bot->GetActiveSpecMask()))
            continue;

        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(it->first);
        if (!spellInfo || spellInfo->IsPassive() || !spellInfo->SpellName[0])
            continue;

        if (IsSpellbookExcludedSpell(it->first))
            continue;

        std::string const spellName = spellInfo->SpellName[0];
        if (spellName.empty())
            continue;

        if (!seenNames.insert(spellName).second)
            continue;

        SpellbookEntryData entry;
        entry.spellId = it->first;
        entry.schoolMask = spellInfo->SchoolMask;
        entry.spellName = spellName;
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), CompareSpellbookEntries);
    return entries;
}

struct ProfessionRecipeEntryData
{
    uint32 spellId = 0;
    std::string spellName;
    uint32 itemId = 0;
    std::string difficulty;
    uint32 craftable = 0;
    std::string materials;
};

std::map<uint32, uint32> BuildBotInventoryItemCounts(Player* bot)
{
    std::map<uint32, uint32> counts;
    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI)
        return counts;

    std::vector<Item*> const items = botAI->GetInventoryItems();
    for (Item* const item : items)
    {
        if (!item)
            continue;

        ItemTemplate const* const proto = item->GetTemplate();
        if (!proto)
            continue;

        counts[proto->ItemId] += item->GetCount();
    }

    return counts;
}

std::string GetRecipeDifficulty(Player* bot, SkillLineAbilityEntry const* skillLine)
{
    if (!bot || !skillLine || !skillLine->SkillLine)
        return "";

    uint32 const grayLevel = skillLine->TrivialSkillLineRankHigh;
    uint32 const greenLevel = (skillLine->TrivialSkillLineRankHigh + skillLine->MinSkillLineRank) / 2;
    uint32 const yellowLevel = skillLine->MinSkillLineRank;
    uint32 const skillValue = bot->GetSkillValue(skillLine->SkillLine);

    if (skillValue >= grayLevel)
        return "gray";
    if (skillValue >= greenLevel)
        return "green";
    if (skillValue >= yellowLevel)
        return "yellow";
    return "orange";
}

std::string BuildRecipeMaterialsPayload(SpellInfo const* spellInfo, std::map<uint32, uint32> const& itemCounts, uint32& craftable)
{
    std::ostringstream materials;
    bool first = true;
    bool hasReagents = false;
    craftable = 0;

    for (uint32 index = 0; index < MAX_SPELL_REAGENTS; ++index)
    {
        if (spellInfo->Reagent[index] <= 0 || spellInfo->ReagentCount[index] <= 0)
            continue;

        uint32 const itemId = static_cast<uint32>(spellInfo->Reagent[index]);
        uint32 const required = spellInfo->ReagentCount[index];
        uint32 const available = itemCounts.count(itemId) ? itemCounts.at(itemId) : 0;
        uint32 const possible = required ? available / required : 0;

        if (!hasReagents || craftable > possible)
            craftable = possible;
        hasReagents = true;

        if (!first)
            materials << ';';
        first = false;
        materials << itemId << ':' << required << ':' << available;
    }

    if (!hasReagents)
        craftable = 0;

    return materials.str();
}

bool BotHasRecipeRequiredTools(Player* bot, SpellInfo const* spellInfo)
{
    if (!bot || !spellInfo)
        return false;

    for (uint32 index = 0; index < 2; ++index)
    {
        if (spellInfo->Totem[index] && !bot->HasItemCount(spellInfo->Totem[index]))
            return false;

        if (spellInfo->TotemCategory[index] && !bot->HasItemTotemCategory(spellInfo->TotemCategory[index]))
            return false;
    }

    return true;
}

uint32 GetRecipeCreatedItemId(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return 0;

    for (uint32 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
        if (spellInfo->Effects[effectIndex].Effect == SPELL_EFFECT_CREATE_ITEM && spellInfo->Effects[effectIndex].ItemType > 0)
            return spellInfo->Effects[effectIndex].ItemType;

    return 0;
}

bool IsKnownActiveBotSpell(Player* bot, uint32 spellId)
{
    if (!bot || !spellId)
        return false;

    PlayerSpellMap::const_iterator const it = bot->GetSpellMap().find(spellId);
    if (it == bot->GetSpellMap().end() || !it->second)
        return false;

    if (it->second->State == PLAYERSPELL_REMOVED || !it->second->Active)
        return false;

    return (it->second->specMask & bot->GetActiveSpecMask()) != 0;
}

std::string GetSpellCastFailureReason(SpellCastResult result)
{
    switch (result)
    {
        case SPELL_CAST_OK:
            return "OK";
        case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
            return "REQUIRES_SPELL_FOCUS";
        case SPELL_FAILED_REQUIRES_AREA:
            return "REQUIRES_AREA";
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND:
        case SPELL_FAILED_TOTEM_CATEGORY:
        case SPELL_FAILED_TOTEMS:
            return "MISSING_TOOLS";
        case SPELL_FAILED_REAGENTS:
        case SPELL_FAILED_NEED_MORE_ITEMS:
            return "NO_MATERIALS";
        case SPELL_FAILED_MOVING:
            return "MOVING";
        case SPELL_FAILED_NOT_STANDING:
            return "NOT_STANDING";
        case SPELL_FAILED_NOT_READY:
        case SPELL_FAILED_SPELL_IN_PROGRESS:
            return "NOT_READY";
        case SPELL_FAILED_OUT_OF_RANGE:
            return "OUT_OF_RANGE";
        case SPELL_FAILED_NOT_TRADING:
            return "NO_TRADE";
        case SPELL_FAILED_ITEM_ALREADY_ENCHANTED:
            return "ALREADY_ENCHANTED";
        case SPELL_FAILED_NOT_TRADEABLE:
            return "NOT_TRADEABLE";
        case SPELL_FAILED_BAD_TARGETS:
        case SPELL_FAILED_ITEM_ENCHANT_TRADE_WINDOW:
            return "BAD_TARGET";
        case SPELL_FAILED_AFFECTING_COMBAT:
            return "IN_COMBAT";
        case SPELL_FAILED_TRY_AGAIN:
            return "TRY_AGAIN";
        default:
            std::ostringstream reason;
            reason << "CAST_FAILED_" << static_cast<uint32>(result);
            return reason.str();
    }
}

std::string ValidateProfessionRecipeCraft(Player* bot, uint32 skillId, uint32 spellId, uint32 expectedItemId, uint32& actualItemId)
{
    actualItemId = expectedItemId;

    if (!bot)
        return "NO_BOT";

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI)
        return "NO_AI";

    if (!skillId || !spellId)
        return "BAD_REQUEST";

    if (!IsKnownActiveBotSpell(bot, spellId))
        return "UNKNOWN_RECIPE";

    SkillLineAbilityEntry const* const skillLine = GetSkillLineAbilityForSpell(spellId);
    if (!skillLine || skillLine->SkillLine != skillId)
        return "SKILL_MISMATCH";

    SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || spellInfo->IsPassive())
        return "BAD_RECIPE";

    actualItemId = GetRecipeCreatedItemId(spellInfo);
    if (expectedItemId && actualItemId && expectedItemId != actualItemId)
        return "ITEM_MISMATCH";

    if (!BotHasRecipeRequiredTools(bot, spellInfo))
        return "MISSING_TOOLS";

    uint32 craftable = 0;
    BuildRecipeMaterialsPayload(spellInfo, BuildBotInventoryItemCounts(bot), craftable);
    if (!craftable)
        return "NO_MATERIALS";

    return "OK";
}


// MB_CRAFT_RECIPE_TARGET_V1_BEGIN
bool ProfessionRecipeRequiresExactItemTarget(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    for (uint32 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
        if (spellInfo->Effects[effectIndex].Effect == SPELL_EFFECT_OPEN_LOCK)
            return false;

    return (spellInfo->Targets & TARGET_FLAG_ITEM) != 0 ||
        (spellInfo->Targets & TARGET_FLAG_GAMEOBJECT_ITEM) != 0;
}

bool IsAllowedProfessionRecipeTargetPosition(uint32 bag, uint32 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0)
    {
        bool const equipment = slot >= EQUIPMENT_SLOT_START && slot < EQUIPMENT_SLOT_END;
        bool const backpack = slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END;
        return equipment || backpack;
    }

    return bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END;
}

struct CraftRecipeTargetRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, CraftRecipeTargetRateState> sCraftRecipeTargetRateStates;

void PruneCraftRecipeTargetRateState(CraftRecipeTargetRateState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kCraftRecipeTargetRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kCraftRecipeTargetReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kCraftRecipeTargetMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeCraftRecipeTargetRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sCraftRecipeTargetRateStates.find(key);

    if (stateIt == sCraftRecipeTargetRateStates.end())
    {
        if (sCraftRecipeTargetRateStates.size() >= kCraftRecipeTargetMaxRequesterStates)
        {
            for (auto it = sCraftRecipeTargetRateStates.begin(); it != sCraftRecipeTargetRateStates.end();)
            {
                PruneCraftRecipeTargetRateState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sCraftRecipeTargetRateStates.erase(it);
                else
                    ++it;
            }
        }

        if (sCraftRecipeTargetRateStates.size() >= kCraftRecipeTargetMaxRequesterStates)
            return false;

        stateIt = sCraftRecipeTargetRateStates.emplace(key, CraftRecipeTargetRateState()).first;
    }

    CraftRecipeTargetRateState& state = stateIt->second;
    PruneCraftRecipeTargetRateState(state, now);
    if (state.requests.size() >= kCraftRecipeTargetRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterCraftRecipeTargetToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sCraftRecipeTargetRateStates.find(requester->GetName());
    if (stateIt == sCraftRecipeTargetRateStates.end())
        return false;

    CraftRecipeTargetRateState& state = stateIt->second;
    PruneCraftRecipeTargetRateState(state, now);

    for (auto const& entry : state.recentTokens)
        if (entry.first == token)
            return false;

    state.recentTokens.push_back({token, now});
    while (state.recentTokens.size() > kCraftRecipeTargetMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}

std::string CastProfessionRecipeTarget(Player* bot, uint32 spellId, Item* targetItem)
{
    if (!bot || !spellId || !targetItem)
        return "BAD_REQUEST";

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
        return "NO_AI";

    SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !ProfessionRecipeRequiresExactItemTarget(spellInfo))
        return "NOT_ITEM_TARGET_RECIPE";

    if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL))
        return "LOST_CONTROL";

    if (bot->IsFlying() || bot->HasUnitState(UNIT_STATE_IN_FLIGHT))
        return "IN_FLIGHT";

    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return "CHANNELING";

    if (bot->HasSpellCooldown(spellId))
        return "NOT_READY";

    if (!bot->IsStandState())
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
        return "NOT_STANDING";
    }

    uint32 const castTime = !spellInfo->IsChanneled() ? spellInfo->CalcCastTime(bot) : spellInfo->GetDuration();
    if ((castTime || spellInfo->IsAutoRepeatRangedSpell()) && bot->isMoving())
        return "MOVING";

    Spell spell(bot, spellInfo, TRIGGERED_NONE);
    SpellCastTargets targets;
    targets.SetItemTarget(targetItem);
    spell.InitExplicitTargets(targets);

    SpellCastResult const checkResult = spell.CheckCast(true);
    if (checkResult == SPELL_FAILED_BAD_TARGETS ||
        checkResult == SPELL_FAILED_ITEM_ENCHANT_TRADE_WINDOW ||
        checkResult == SPELL_FAILED_NOT_TRADEABLE ||
        checkResult == SPELL_FAILED_EQUIPPED_ITEM_CLASS)
    {
        return "INVALID_TARGET_ITEM";
    }
    if (checkResult != SPELL_CAST_OK)
        return GetSpellCastFailureReason(checkResult);

    return botAI->CastSpell(spellId, bot, targetItem) ? "OK" : "TRY_AGAIN";
}
// MB_CRAFT_RECIPE_TARGET_V1_HELPERS_END

void BuildProfessionRecipeCastTargets(Player* bot, PlayerbotAI* botAI, SpellInfo const* spellInfo, SpellCastTargets& targets)
{
    if (!bot || !spellInfo)
        return;

    if (spellInfo->Effects[0].Effect != SPELL_EFFECT_OPEN_LOCK &&
        (spellInfo->Targets & TARGET_FLAG_ITEM || spellInfo->Targets & TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        Item* item = nullptr;
        if (botAI && botAI->GetAiObjectContext())
            item = botAI->GetAiObjectContext()->GetValue<Item*>("item for spell", spellInfo->Id)->Get();

        targets.SetItemTarget(item);
    }
    else if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
        targets.SetDst(*bot);
    else if (spellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
        targets.SetDst(*bot);
    else
        targets.SetUnitTarget(bot);
}

std::string CheckProfessionRecipeCast(Player* bot, PlayerbotAI* botAI, SpellInfo const* spellInfo)
{
    if (!bot || !spellInfo)
        return "BAD_RECIPE";

    Spell* const spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
    SpellCastTargets targets;
    BuildProfessionRecipeCastTargets(bot, botAI, spellInfo, targets);

    SpellCastResult const result = spell->CheckCast(true);
    delete spell;

    return GetSpellCastFailureReason(result);
}

std::string CastProfessionRecipe(Player* bot, uint32 spellId)
{
    if (!bot || !spellId)
        return "BAD_REQUEST";

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
        return "NO_AI";

    SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return "BAD_RECIPE";

    if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL))
        return "LOST_CONTROL";

    if (bot->IsFlying() || bot->HasUnitState(UNIT_STATE_IN_FLIGHT))
        return "IN_FLIGHT";

    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return "CHANNELING";

    if (bot->HasSpellCooldown(spellId))
        return "NOT_READY";

    if (!bot->IsStandState())
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
        return "NOT_STANDING";
    }

    uint32 const castTime = !spellInfo->IsChanneled() ? spellInfo->CalcCastTime(bot) : spellInfo->GetDuration();
    if ((castTime || spellInfo->IsAutoRepeatRangedSpell()) && bot->isMoving())
        return "MOVING";

    std::string const checkResult = CheckProfessionRecipeCast(bot, botAI, spellInfo);
    if (checkResult != "OK")
        return checkResult;

    return botAI->CastSpell(spellId, bot) ? "OK" : "TRY_AGAIN";
}

std::vector<ProfessionRecipeEntryData> BuildProfessionRecipeEntries(Player* bot, uint32 skillId)
{
    std::vector<ProfessionRecipeEntryData> entries;
    if (!bot || !skillId)
        return entries;

    std::set<std::string> seenNames;
    std::map<uint32, uint32> const itemCounts = BuildBotInventoryItemCounts(bot);

    for (PlayerSpellMap::const_iterator it = bot->GetSpellMap().begin(); it != bot->GetSpellMap().end(); ++it)
    {
        if (!it->second)
            continue;

        if (it->second->State == PLAYERSPELL_REMOVED || !it->second->Active)
            continue;

        if (!(it->second->specMask & bot->GetActiveSpecMask()))
            continue;

        SkillLineAbilityEntry const* const skillLine = GetSkillLineAbilityForSpell(it->first);
        if (!skillLine || skillLine->SkillLine != skillId)
            continue;

        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(it->first);
        if (!spellInfo || spellInfo->IsPassive() || !spellInfo->SpellName[0])
            continue;

        std::string const spellName = spellInfo->SpellName[0];
        if (spellName.empty() || !seenNames.insert(spellName).second)
            continue;

        ProfessionRecipeEntryData entry;
        entry.spellId = it->first;
        entry.spellName = spellName;
        entry.difficulty = GetRecipeDifficulty(bot, skillLine);
        entry.materials = BuildRecipeMaterialsPayload(spellInfo, itemCounts, entry.craftable);
        if (entry.craftable && !BotHasRecipeRequiredTools(bot, spellInfo))
            entry.craftable = 0;

        entry.itemId = GetRecipeCreatedItemId(spellInfo);

        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](ProfessionRecipeEntryData const& left, ProfessionRecipeEntryData const& right)
    {
        if (left.difficulty != right.difficulty)
            return left.difficulty < right.difficulty;
        if (left.spellName != right.spellName)
            return left.spellName < right.spellName;
        return left.spellId < right.spellId;
    });

    return entries;
}

struct EnchantTradeMaterialData
{
    uint32 itemId = 0;
    uint32 required = 0;
    uint32 available = 0;
};

struct EnchantTradeEntryData
{
    uint32 spellId = 0;
    std::string spellName;
    std::string difficulty;
    uint32 available = 0;
    uint32 hasTools = 0;
    std::vector<EnchantTradeMaterialData> materials;
};

bool IsEnchantTradeSpell(SpellInfo const* spellInfo)
{
    if (!spellInfo || spellInfo->IsPassive())
        return false;

    for (uint32 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
    {
        if (spellInfo->Effects[effectIndex].Effect != SPELL_EFFECT_ENCHANT_ITEM)
            continue;

        SpellItemEnchantmentEntry const* const enchantEntry =
            sSpellItemEnchantmentStore.LookupEntry(spellInfo->Effects[effectIndex].MiscValue);
        if (enchantEntry && !(enchantEntry->slot & ENCHANTMENT_CAN_SOULBOUND))
            return true;
    }

    return false;
}

void BuildEnchantTradeMaterials(SpellInfo const* spellInfo, std::map<uint32, uint32> const& itemCounts,
    std::vector<EnchantTradeMaterialData>& materials, uint32& available)
{
    materials.clear();
    available = spellInfo ? 1 : 0;
    if (!spellInfo)
        return;

    for (uint32 index = 0; index < MAX_SPELL_REAGENTS; ++index)
    {
        if (spellInfo->Reagent[index] <= 0 || spellInfo->ReagentCount[index] <= 0)
            continue;

        EnchantTradeMaterialData material;
        material.itemId = static_cast<uint32>(spellInfo->Reagent[index]);
        material.required = spellInfo->ReagentCount[index];
        material.available = itemCounts.count(material.itemId) ? itemCounts.at(material.itemId) : 0;
        if (material.available < material.required)
            available = 0;
        materials.push_back(material);
    }
}

std::string ValidateEnchantTradeSpellIdentity(Player* bot, uint32 spellId, SpellInfo const*& spellInfo)
{
    spellInfo = nullptr;
    if (!bot || !spellId)
        return "BAD_REQUEST";

    if (!bot->HasSkill(SKILL_ENCHANTING))
        return "NOT_ENCHANTER";

    if (!IsKnownActiveBotSpell(bot, spellId))
        return "UNKNOWN_ENCHANT";

    SkillLineAbilityEntry const* const skillLine = GetSkillLineAbilityForSpell(spellId);
    if (!skillLine || skillLine->SkillLine != SKILL_ENCHANTING)
        return "BAD_ENCHANT";

    spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!IsEnchantTradeSpell(spellInfo))
        return "BAD_ENCHANT";

    return "OK";
}

std::vector<EnchantTradeEntryData> BuildEnchantTradeEntries(Player* bot)
{
    std::vector<EnchantTradeEntryData> entries;
    if (!bot || !bot->HasSkill(SKILL_ENCHANTING))
        return entries;

    std::map<uint32, uint32> const itemCounts = BuildBotInventoryItemCounts(bot);
    for (PlayerSpellMap::const_iterator it = bot->GetSpellMap().begin(); it != bot->GetSpellMap().end(); ++it)
    {
        SpellInfo const* spellInfo = nullptr;
        if (ValidateEnchantTradeSpellIdentity(bot, it->first, spellInfo) != "OK" || !spellInfo || !spellInfo->SpellName[0])
            continue;

        SkillLineAbilityEntry const* const skillLine = GetSkillLineAbilityForSpell(it->first);
        if (!skillLine)
            continue;

        EnchantTradeEntryData entry;
        entry.spellId = it->first;
        entry.spellName = spellInfo->SpellName[0];
        entry.difficulty = GetRecipeDifficulty(bot, skillLine);
        BuildEnchantTradeMaterials(spellInfo, itemCounts, entry.materials, entry.available);
        entry.hasTools = BotHasRecipeRequiredTools(bot, spellInfo) ? 1 : 0;
        if (!entry.hasTools)
            entry.available = 0;
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](EnchantTradeEntryData const& left, EnchantTradeEntryData const& right)
    {
        if (left.spellName != right.spellName)
            return left.spellName < right.spellName;
        return left.spellId < right.spellId;
    });

    if (entries.size() > kMaxEnchantTradeEntries)
        entries.resize(kMaxEnchantTradeEntries);

    return entries;
}

InventorySummaryData BuildInventorySummary(Player* bot)
{
    InventorySummaryData summary;
    if (!bot)
        return summary;

    uint32 const money = bot->GetMoney();
    summary.gold = money / 10000;
    summary.silver = (money % 10000) / 100;
    summary.copper = money % 100;

    uint32 used = 0;
    uint32 total = 16;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            ++used;
    }

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag const* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag))
        {
            ItemTemplate const* const proto = pBag->GetTemplate();
            if (proto && proto->Class == ITEM_CLASS_CONTAINER && proto->SubClass == ITEM_SUBCLASS_CONTAINER)
            {
                total += pBag->GetBagSize();
                used += pBag->GetBagSize() - pBag->GetFreeSlots();
            }
        }
    }

    summary.bagUsed = used;
    summary.bagTotal = total;
    return summary;
}

uint32 BuildDurabilityPct(Player* bot)
{
    if (!bot)
        return 0;

    uint32 current = 0;
    uint32 maximum = 0;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint32 const itemMax = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
        if (!itemMax)
            continue;

        uint32 const itemCurrent = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        maximum += itemMax;
        current += std::min(itemCurrent, itemMax);
    }

    if (!maximum)
        return 100;

    return std::min<uint32>(100, (current * 100u) / maximum);
}

uint32 BuildXpPct(Player* bot)
{
    if (!bot)
        return 0;

    uint32 const nextLevelXp = bot->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    if (!nextLevelXp)
        return 0;

    return std::min<uint32>(100, (bot->GetUInt32Value(PLAYER_XP) * 100u) / nextLevelXp);
}

StatsData BuildStatsData(Player* bot)
{
    StatsData data;
    if (!bot)
        return data;

    data.name = bot->GetName();
    data.level = bot->GetLevel();

    uint32 const money = bot->GetMoney();
    data.gold = money / 10000;
    data.silver = (money % 10000) / 100;
    data.copper = money % 100;

    InventorySummaryData const inventory = BuildInventorySummary(bot);
    data.bagUsed = inventory.bagUsed;
    data.bagTotal = inventory.bagTotal;
    data.durabilityPct = BuildDurabilityPct(bot);
    data.xpPct = BuildXpPct(bot);
    data.manaPct = GetPct(bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA));

    return data;
}

std::string BuildStatsPayload(Player* bot)
{
    StatsData const data = BuildStatsData(bot);
    if (data.name.empty())
        return "";

    std::ostringstream out;
    out << UrlEncodeField(data.name)
        << kFieldSeparator << data.level
        << kFieldSeparator << data.gold
        << kFieldSeparator << data.silver
        << kFieldSeparator << data.copper
        << kFieldSeparator << data.bagUsed
        << kFieldSeparator << data.bagTotal
        << kFieldSeparator << data.durabilityPct
        << kFieldSeparator << data.xpPct
        << kFieldSeparator << data.manaPct;

    return out.str();
}

void SendInventorySnapshot(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::string const prefixPayload = trimmedBotName + std::string(1, kFieldSeparator) + requestToken;
    SendAddonPacket(requester, replyType, "INV_BEGIN", prefixPayload);

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "INV_END", prefixPayload);
        return;
    }

    InventorySummaryData const summary = BuildInventorySummary(bot);
    std::ostringstream summaryPayload;
    summaryPayload << bot->GetName() << kFieldSeparator << requestToken << kFieldSeparator << summary.gold << kFieldSeparator
                   << summary.silver << kFieldSeparator << summary.copper << kFieldSeparator << summary.bagUsed
                   << kFieldSeparator << summary.bagTotal;
    SendAddonPacket(requester, replyType, "INV_SUMMARY", summaryPayload.str());

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (botAI)
    {
        std::map<uint32, uint32> itemCounts;
        std::map<uint32, ItemTemplate const*> itemTemplates;
        std::map<uint32, bool> soulboundByEntry;

        std::vector<Item*> const items = botAI->GetInventoryItems();
        for (Item* const item : items)
        {
            if (!item)
                continue;

            ItemTemplate const* const proto = item->GetTemplate();
            if (!proto)
                continue;

            uint32 const itemId = proto->ItemId;
            itemCounts[itemId] += item->GetCount();
            itemTemplates[itemId] = proto;
            if (item->IsSoulBound())
                soulboundByEntry[itemId] = true;
        }

        for (auto const& entry : itemCounts)
        {
            ItemTemplate const* const proto = itemTemplates[entry.first];
            if (!proto)
                continue;

            std::string line = ChatHelper::FormatItem(proto, entry.second);
            if (soulboundByEntry[entry.first])
                line += " (soulbound)";

            std::string payload = bot->GetName();
            payload += kFieldSeparator;
            payload += requestToken;
            payload += kFieldSeparator;
            payload += UrlEncodeField(line);
            SendAddonPacket(requester, replyType, "INV_ITEM", payload);
        }
    }

    SendAddonPacket(requester, replyType, "INV_END", bot->GetName() + std::string(1, kFieldSeparator) + requestToken);
}

struct InventoryExactRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, InventoryExactRateState> sInventoryExactRateStates;

bool ConsumeInventoryExactRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    InventoryExactRateState& state = sInventoryExactRateStates[key];

    while (!state.requests.empty() && now - state.requests.front() >= kInventoryExactRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kInventoryExactRateLimit)
        return false;

    state.requests.push_back(now);

    if (sInventoryExactRateStates.size() > 512)
    {
        for (auto it = sInventoryExactRateStates.begin(); it != sInventoryExactRateStates.end();)
        {
            while (!it->second.requests.empty() && now - it->second.requests.front() >= kInventoryExactRateWindow)
                it->second.requests.pop_front();

            if (it->second.requests.empty() && it->first != key)
                it = sInventoryExactRateStates.erase(it);
            else
                ++it;
        }
    }

    return true;
}

struct BankSnapshotRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, BankSnapshotRateState> sBankSnapshotRateStates;

void PruneBankSnapshotRateState(BankSnapshotRateState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kBankSnapshotRateWindow)
        state.requests.pop_front();
}

bool ConsumeBankSnapshotRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sBankSnapshotRateStates.find(key);

    if (stateIt == sBankSnapshotRateStates.end())
    {
        if (sBankSnapshotRateStates.size() >= kBankSnapshotMaxRequesterStates)
        {
            for (auto it = sBankSnapshotRateStates.begin(); it != sBankSnapshotRateStates.end();)
            {
                PruneBankSnapshotRateState(it->second, now);
                if (it->second.requests.empty())
                    it = sBankSnapshotRateStates.erase(it);
                else
                    ++it;
            }
        }

        if (sBankSnapshotRateStates.size() >= kBankSnapshotMaxRequesterStates)
            return false;

        stateIt = sBankSnapshotRateStates.emplace(key, BankSnapshotRateState()).first;
    }

    BankSnapshotRateState& state = stateIt->second;
    PruneBankSnapshotRateState(state, now);

    if (state.requests.size() >= kBankSnapshotRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

struct InventoryItemMoveRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

struct InventoryItemMovePositionState
{
    bool present;
    uint64 guidCounter;
    uint32 itemId;
    uint32 count;
};

std::map<std::string, InventoryItemMoveRequestState> sInventoryItemMoveRequestStates;

void PruneInventoryItemMoveRequestState(InventoryItemMoveRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemMoveRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemMoveReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemMoveMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemMoveRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemMoveRequestStates.find(key);

    if (stateIt == sInventoryItemMoveRequestStates.end())
    {
        if (sInventoryItemMoveRequestStates.size() >= kInventoryItemMoveMaxRequesterStates)
        {
            for (auto it = sInventoryItemMoveRequestStates.begin(); it != sInventoryItemMoveRequestStates.end();)
            {
                PruneInventoryItemMoveRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemMoveRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemMoveRequestStates.size() >= kInventoryItemMoveMaxRequesterStates)
            return false;

        stateIt = sInventoryItemMoveRequestStates.emplace(key, InventoryItemMoveRequestState()).first;
    }

    InventoryItemMoveRequestState& state = stateIt->second;
    PruneInventoryItemMoveRequestState(state, now);

    if (state.requests.size() >= kInventoryItemMoveRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemMoveToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemMoveRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemMoveRequestStates.end())
        return false;

    InventoryItemMoveRequestState& state = stateIt->second;
    PruneInventoryItemMoveRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemMoveMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}

struct InventoryItemDepositExactRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemDepositExactRequestState> sInventoryItemDepositExactRequestStates;

void PruneInventoryItemDepositExactRequestState(
    InventoryItemDepositExactRequestState& state,
    std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemDepositExactRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemDepositExactReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemDepositExactMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemDepositExactRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemDepositExactRequestStates.find(key);

    if (stateIt == sInventoryItemDepositExactRequestStates.end())
    {
        if (sInventoryItemDepositExactRequestStates.size() >= kInventoryItemDepositExactMaxRequesterStates)
        {
            for (auto it = sInventoryItemDepositExactRequestStates.begin();
                 it != sInventoryItemDepositExactRequestStates.end();)
            {
                PruneInventoryItemDepositExactRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemDepositExactRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemDepositExactRequestStates.size() >= kInventoryItemDepositExactMaxRequesterStates)
            return false;

        stateIt = sInventoryItemDepositExactRequestStates.emplace(
            key, InventoryItemDepositExactRequestState()).first;
    }

    InventoryItemDepositExactRequestState& state = stateIt->second;
    PruneInventoryItemDepositExactRequestState(state, now);

    if (state.requests.size() >= kInventoryItemDepositExactRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemDepositExactToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemDepositExactRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemDepositExactRequestStates.end())
        return false;

    InventoryItemDepositExactRequestState& state = stateIt->second;
    PruneInventoryItemDepositExactRequestState(state, now);

    for (auto const& entry : state.recentTokens)
        if (entry.first == token)
            return false;

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemDepositExactMaxRecentTokens)
        state.recentTokens.pop_front();

    return true;
}

// MB_LOOT_RULE_ITEM_V1_RATE_BEGIN
struct LootRuleItemRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, LootRuleItemRequestState> sLootRuleItemRequestStates;

void PruneLootRuleItemRequestState(
    LootRuleItemRequestState& state,
    std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kLootRuleItemRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kLootRuleItemReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kLootRuleItemMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeLootRuleItemRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sLootRuleItemRequestStates.find(key);
    if (stateIt == sLootRuleItemRequestStates.end())
    {
        if (sLootRuleItemRequestStates.size() >= kLootRuleItemMaxRequesterStates)
        {
            for (auto it = sLootRuleItemRequestStates.begin(); it != sLootRuleItemRequestStates.end();)
            {
                PruneLootRuleItemRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sLootRuleItemRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sLootRuleItemRequestStates.size() >= kLootRuleItemMaxRequesterStates)
            return false;

        stateIt = sLootRuleItemRequestStates.emplace(key, LootRuleItemRequestState()).first;
    }

    LootRuleItemRequestState& state = stateIt->second;
    PruneLootRuleItemRequestState(state, now);
    if (state.requests.size() >= kLootRuleItemRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterLootRuleItemToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sLootRuleItemRequestStates.find(requester->GetName());
    if (stateIt == sLootRuleItemRequestStates.end())
        return false;

    LootRuleItemRequestState& state = stateIt->second;
    PruneLootRuleItemRequestState(state, now);
    for (auto const& entry : state.recentTokens)
        if (entry.first == token)
            return false;

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kLootRuleItemMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}
// MB_LOOT_RULE_ITEM_PERSISTENCE_BUDGET_BEGIN
std::deque<std::chrono::steady_clock::time_point> sLootRuleItemPersistenceSaves;

bool ReserveLootRuleItemPersistenceBudget(std::size_t count)
{
    if (count == 0)
        return true;
    if (count > kLootRuleItemPersistenceBudget)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    while (!sLootRuleItemPersistenceSaves.empty() &&
           now - sLootRuleItemPersistenceSaves.front() >= kLootRuleItemPersistenceWindow)
    {
        sLootRuleItemPersistenceSaves.pop_front();
    }

    if (sLootRuleItemPersistenceSaves.size() > kLootRuleItemPersistenceBudget - count)
        return false;

    for (std::size_t i = 0; i < count; ++i)
        sLootRuleItemPersistenceSaves.push_back(now);

    return true;
}
// MB_LOOT_RULE_ITEM_PERSISTENCE_BUDGET_END
// MB_LOOT_RULE_ITEM_V1_RATE_END
struct InventoryItemTradeRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemTradeRequestState> sInventoryItemTradeRequestStates;

void PruneInventoryItemTradeRequestState(InventoryItemTradeRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemTradeRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemTradeReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemTradeMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemTradeRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemTradeRequestStates.find(key);

    if (stateIt == sInventoryItemTradeRequestStates.end())
    {
        if (sInventoryItemTradeRequestStates.size() >= kInventoryItemTradeMaxRequesterStates)
        {
            for (auto it = sInventoryItemTradeRequestStates.begin(); it != sInventoryItemTradeRequestStates.end();)
            {
                PruneInventoryItemTradeRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemTradeRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemTradeRequestStates.size() >= kInventoryItemTradeMaxRequesterStates)
            return false;

        stateIt = sInventoryItemTradeRequestStates.emplace(key, InventoryItemTradeRequestState()).first;
    }

    InventoryItemTradeRequestState& state = stateIt->second;
    PruneInventoryItemTradeRequestState(state, now);

    if (state.requests.size() >= kInventoryItemTradeRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemTradeToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemTradeRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemTradeRequestStates.end())
        return false;

    InventoryItemTradeRequestState& state = stateIt->second;
    PruneInventoryItemTradeRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemTradeMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}

struct InventoryItemEquipRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemEquipRequestState> sInventoryItemEquipRequestStates;

void PruneInventoryItemEquipRequestState(InventoryItemEquipRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemEquipRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemEquipReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemEquipMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemEquipRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemEquipRequestStates.find(key);

    if (stateIt == sInventoryItemEquipRequestStates.end())
    {
        if (sInventoryItemEquipRequestStates.size() >= kInventoryItemEquipMaxRequesterStates)
        {
            for (auto it = sInventoryItemEquipRequestStates.begin(); it != sInventoryItemEquipRequestStates.end();)
            {
                PruneInventoryItemEquipRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemEquipRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemEquipRequestStates.size() >= kInventoryItemEquipMaxRequesterStates)
            return false;

        stateIt = sInventoryItemEquipRequestStates.emplace(key, InventoryItemEquipRequestState()).first;
    }

    InventoryItemEquipRequestState& state = stateIt->second;
    PruneInventoryItemEquipRequestState(state, now);

    if (state.requests.size() >= kInventoryItemEquipRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemEquipToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemEquipRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemEquipRequestStates.end())
        return false;

    InventoryItemEquipRequestState& state = stateIt->second;
    PruneInventoryItemEquipRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemEquipMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}

struct InventoryItemUnequipRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemUnequipRequestState> sInventoryItemUnequipRequestStates;

void PruneInventoryItemUnequipRequestState(InventoryItemUnequipRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemUnequipRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemUnequipReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemUnequipMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemUnequipRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemUnequipRequestStates.find(key);

    if (stateIt == sInventoryItemUnequipRequestStates.end())
    {
        if (sInventoryItemUnequipRequestStates.size() >= kInventoryItemUnequipMaxRequesterStates)
        {
            for (auto it = sInventoryItemUnequipRequestStates.begin(); it != sInventoryItemUnequipRequestStates.end();)
            {
                PruneInventoryItemUnequipRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemUnequipRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemUnequipRequestStates.size() >= kInventoryItemUnequipMaxRequesterStates)
            return false;

        stateIt = sInventoryItemUnequipRequestStates.emplace(key, InventoryItemUnequipRequestState()).first;
    }

    InventoryItemUnequipRequestState& state = stateIt->second;
    PruneInventoryItemUnequipRequestState(state, now);

    if (state.requests.size() >= kInventoryItemUnequipRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemUnequipToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemUnequipRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemUnequipRequestStates.end())
        return false;

    InventoryItemUnequipRequestState& state = stateIt->second;
    PruneInventoryItemUnequipRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemUnequipMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}

// MB_ITEM_DESTROY_RATE_V1_BEGIN
struct InventoryItemDestroyRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemDestroyRequestState> sInventoryItemDestroyRequestStates;

void PruneInventoryItemDestroyRequestState(InventoryItemDestroyRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemDestroyRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemDestroyReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemDestroyMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemDestroyRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemDestroyRequestStates.find(key);

    if (stateIt == sInventoryItemDestroyRequestStates.end())
    {
        if (sInventoryItemDestroyRequestStates.size() >= kInventoryItemDestroyMaxRequesterStates)
        {
            for (auto it = sInventoryItemDestroyRequestStates.begin(); it != sInventoryItemDestroyRequestStates.end();)
            {
                PruneInventoryItemDestroyRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemDestroyRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemDestroyRequestStates.size() >= kInventoryItemDestroyMaxRequesterStates)
            return false;

        stateIt = sInventoryItemDestroyRequestStates.emplace(key, InventoryItemDestroyRequestState()).first;
    }

    InventoryItemDestroyRequestState& state = stateIt->second;
    PruneInventoryItemDestroyRequestState(state, now);

    if (state.requests.size() >= kInventoryItemDestroyRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemDestroyToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemDestroyRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemDestroyRequestStates.end())
        return false;

    InventoryItemDestroyRequestState& state = stateIt->second;
    PruneInventoryItemDestroyRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemDestroyMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}
// MB_ITEM_DESTROY_RATE_V1_END
// MB_ITEM_USE_RATE_V1_BEGIN
struct InventoryItemUseRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemUseRequestState> sInventoryItemUseRequestStates;

void PruneInventoryItemUseRequestState(InventoryItemUseRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemUseRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemUseReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemUseMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemUseRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemUseRequestStates.find(key);

    if (stateIt == sInventoryItemUseRequestStates.end())
    {
        if (sInventoryItemUseRequestStates.size() >= kInventoryItemUseMaxRequesterStates)
        {
            for (auto it = sInventoryItemUseRequestStates.begin(); it != sInventoryItemUseRequestStates.end();)
            {
                PruneInventoryItemUseRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemUseRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemUseRequestStates.size() >= kInventoryItemUseMaxRequesterStates)
            return false;

        stateIt = sInventoryItemUseRequestStates.emplace(key, InventoryItemUseRequestState()).first;
    }

    InventoryItemUseRequestState& state = stateIt->second;
    PruneInventoryItemUseRequestState(state, now);

    if (state.requests.size() >= kInventoryItemUseRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemUseToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemUseRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemUseRequestStates.end())
        return false;

    InventoryItemUseRequestState& state = stateIt->second;
    PruneInventoryItemUseRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemUseMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}
// MB_ITEM_USE_RATE_V1_END

// MB_ITEM_SELL_SINGLE_RATE_V1_BEGIN
struct InventoryItemSellRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, InventoryItemSellRequestState> sInventoryItemSellRequestStates;

void PruneInventoryItemSellRequestState(InventoryItemSellRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kInventoryItemSellRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kInventoryItemSellReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kInventoryItemSellMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeInventoryItemSellRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sInventoryItemSellRequestStates.find(key);

    if (stateIt == sInventoryItemSellRequestStates.end())
    {
        if (sInventoryItemSellRequestStates.size() >= kInventoryItemSellMaxRequesterStates)
        {
            for (auto it = sInventoryItemSellRequestStates.begin(); it != sInventoryItemSellRequestStates.end();)
            {
                PruneInventoryItemSellRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sInventoryItemSellRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sInventoryItemSellRequestStates.size() >= kInventoryItemSellMaxRequesterStates)
            return false;

        stateIt = sInventoryItemSellRequestStates.emplace(key, InventoryItemSellRequestState()).first;
    }

    InventoryItemSellRequestState& state = stateIt->second;
    PruneInventoryItemSellRequestState(state, now);

    if (state.requests.size() >= kInventoryItemSellRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterInventoryItemSellToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sInventoryItemSellRequestStates.find(requester->GetName());
    if (stateIt == sInventoryItemSellRequestStates.end())
        return false;

    InventoryItemSellRequestState& state = stateIt->second;
    PruneInventoryItemSellRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kInventoryItemSellMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}
// MB_ITEM_SELL_SINGLE_RATE_V1_END
// MB_VENDOR_BUYBACK_RATE_V1_BEGIN
struct VendorBuybackRequestState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, VendorBuybackRequestState> sVendorBuybackRequestStates;

void PruneVendorBuybackRequestState(VendorBuybackRequestState& state, std::chrono::steady_clock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kVendorBuybackRateWindow)
        state.requests.pop_front();
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kVendorBuybackReplayTtl)
        state.recentTokens.pop_front();
    while (state.recentTokens.size() > kVendorBuybackMaxRecentTokens)
        state.recentTokens.pop_front();
}

bool ConsumeVendorBuybackRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sVendorBuybackRequestStates.find(key);

    if (stateIt == sVendorBuybackRequestStates.end())
    {
        if (sVendorBuybackRequestStates.size() >= kVendorBuybackMaxRequesterStates)
        {
            for (auto it = sVendorBuybackRequestStates.begin(); it != sVendorBuybackRequestStates.end();)
            {
                PruneVendorBuybackRequestState(it->second, now);
                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sVendorBuybackRequestStates.erase(it);
                else
                    ++it;
            }
        }

        if (sVendorBuybackRequestStates.size() >= kVendorBuybackMaxRequesterStates)
            return false;

        stateIt = sVendorBuybackRequestStates.emplace(key, VendorBuybackRequestState()).first;
    }

    VendorBuybackRequestState& state = stateIt->second;
    PruneVendorBuybackRequestState(state, now);

    if (state.requests.size() >= kVendorBuybackRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterVendorBuybackToken(Player* requester, std::string const& token)
{
    if (!requester || token.empty())
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    auto stateIt = sVendorBuybackRequestStates.find(requester->GetName());
    if (stateIt == sVendorBuybackRequestStates.end())
        return false;

    VendorBuybackRequestState& state = stateIt->second;
    PruneVendorBuybackRequestState(state, now);

    for (auto const& entry : state.recentTokens)
    {
        if (entry.first == token)
            return false;
    }

    state.recentTokens.push_back(std::make_pair(token, now));
    while (state.recentTokens.size() > kVendorBuybackMaxRecentTokens)
        state.recentTokens.pop_front();
    return true;
}
// MB_VENDOR_BUYBACK_RATE_V1_END


bool IsInventoryItemEquipSourcePositionAllowed(Player* bot, uint8 bag, uint8 slot)
{
    if (!bot)
        return false;

    if (bag == INVENTORY_SLOT_BAG_0)
        return slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END;

    if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
    {
        Bag* const container = bot->GetBagByPos(bag);
        return container && static_cast<uint32>(slot) < container->GetBagSize();
    }

    return false;
}

bool IsInventoryItemUnequipDestinationPositionAllowed(Player* bot, uint8 bag, uint8 slot)
{
    return IsInventoryItemEquipSourcePositionAllowed(bot, bag, slot);
}

bool IsInventoryItemMovePositionAllowed(Player* bot, uint8 bag, uint8 slot)
{
    if (!bot)
        return false;

    if (bag == INVENTORY_SLOT_BAG_0)
    {
        if (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
            return true;

        uint32 const keyringEnd = static_cast<uint32>(KEYRING_SLOT_START) + bot->GetMaxKeyringSize();
        return slot >= KEYRING_SLOT_START && static_cast<uint32>(slot) < keyringEnd;
    }

    if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
    {
        Bag* const container = bot->GetBagByPos(bag);
        return container && static_cast<uint32>(slot) < container->GetBagSize();
    }

    return false;
}

InventoryItemMovePositionState ReadInventoryItemMovePositionState(Player* bot, uint8 bag, uint8 slot)
{
    InventoryItemMovePositionState state = {};
    if (!bot)
        return state;

    Item* const item = bot->GetItemByPos(bag, slot);
    if (!item)
        return state;

    state.present = true;
    state.guidCounter = static_cast<uint64>(item->GetGUID().GetCounter());
    state.itemId = item->GetEntry();
    state.count = item->GetCount();
    return state;
}

bool InventoryItemMoveStateMatchesExpected(InventoryItemMovePositionState const& state, uint32 itemId, uint32 count)
{
    if (itemId == 0 || count == 0)
        return itemId == 0 && count == 0 && !state.present;

    return state.present && state.itemId == itemId && state.count == count;
}

bool InventoryItemMoveStatesEqual(InventoryItemMovePositionState const& left, InventoryItemMovePositionState const& right)
{
    return left.present == right.present &&
        left.guidCounter == right.guidCounter &&
        left.itemId == right.itemId &&
        left.count == right.count;
}

void SendInventoryExactBagPacket(
    Player* requester,
    ChatMsg replyType,
    Player* bot,
    std::string const& requestToken,
    char const* kind,
    uint8 bag,
    uint8 slotStart,
    uint32 slotCount,
    uint32 bagItemId)
{
    if (!requester || !bot || !kind)
        return;

    std::ostringstream payload;
    payload << bot->GetName()
            << kFieldSeparator << requestToken
            << kFieldSeparator << kind
            << kFieldSeparator << static_cast<uint32>(bag)
            << kFieldSeparator << static_cast<uint32>(slotStart)
            << kFieldSeparator << slotCount
            << kFieldSeparator << bagItemId;
    SendAddonPacket(requester, replyType, "INV_BAG", payload.str());
}

void SendInventoryExactSnapshot(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::string const prefixPayload = trimmedBotName + std::string(1, kFieldSeparator) + requestToken;

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "INV_EXACT_BEGIN", prefixPayload);
        SendAddonPacket(
            requester,
            replyType,
            "INV_EXACT_ERROR",
            prefixPayload + std::string(1, kFieldSeparator) + "NO_BOT");
        SendAddonPacket(requester, replyType, "INV_EXACT_END", prefixPayload);
        return;
    }

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI || !botAI->GetSecurity() ||
        !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
    {
        SendAddonPacket(requester, replyType, "INV_EXACT_BEGIN", prefixPayload);
        SendAddonPacket(
            requester,
            replyType,
            "INV_EXACT_ERROR",
            prefixPayload + std::string(1, kFieldSeparator) + "FORBIDDEN");
        SendAddonPacket(requester, replyType, "INV_EXACT_END", prefixPayload);
        return;
    }

    SendAddonPacket(requester, replyType, "INV_EXACT_BEGIN", prefixPayload);

    SendInventoryExactBagPacket(
        requester,
        replyType,
        bot,
        requestToken,
        "BACKPACK",
        INVENTORY_SLOT_BAG_0,
        INVENTORY_SLOT_ITEM_START,
        static_cast<uint32>(INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START),
        0);

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* const container = static_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag));
        ItemTemplate const* const proto = container ? container->GetTemplate() : nullptr;
        SendInventoryExactBagPacket(
            requester,
            replyType,
            bot,
            requestToken,
            "BAG",
            bag,
            0,
            container ? container->GetBagSize() : 0,
            proto ? proto->ItemId : 0);
    }

    SendInventoryExactBagPacket(
        requester,
        replyType,
        bot,
        requestToken,
        "KEYRING",
        INVENTORY_SLOT_BAG_0,
        KEYRING_SLOT_START,
        bot->GetMaxKeyringSize(),
        0);

    std::vector<Item*> const items = botAI->GetInventoryItems();
    for (Item* const item : items)
    {
        if (!item)
            continue;

        ItemTemplate const* const proto = item->GetTemplate();
        if (!proto)
            continue;

        std::ostringstream payload;
        payload << bot->GetName()
                << kFieldSeparator << requestToken
                << kFieldSeparator << static_cast<uint32>(item->GetBagSlot())
                << kFieldSeparator << static_cast<uint32>(item->GetSlot())
                << kFieldSeparator << proto->ItemId
                << kFieldSeparator << item->GetCount()
                << kFieldSeparator << (item->IsSoulBound() ? 1 : 0);
        SendAddonPacket(requester, replyType, "INV_ITEM_LOC", payload.str());
    }

    SendAddonPacket(requester, replyType, "INV_EXACT_END", bot->GetName() + std::string(1, kFieldSeparator) + requestToken);
}

PlayerbotAI* GetBotAI(Player* bot)
{
    if (!bot)
        return nullptr;

    return sPlayerbotsMgr.GetPlayerbotAI(bot);
}

Creature* FindNearbyNpcWithFlag(Player* bot, uint32 npcFlag)
{
    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetAiObjectContext())
        return nullptr;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    GuidVector const npcs = *context->GetValue<GuidVector>("nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Unit* const unit = botAI->GetUnit(guid);
        if (!unit || unit->IsHostileTo(bot) || !unit->HasNpcFlag(static_cast<NPCFlags>(npcFlag)))
            continue;

        if (Creature* const creature = unit->ToCreature())
            return creature;
    }

    return nullptr;
}

Creature* FindNearbyVendorSellingItem(Player* bot, uint32 itemId, uint32& vendorSlot, uint32& vendorExtendedCost, bool& sawVendor)
{
    vendorSlot = 0;
    vendorExtendedCost = 0;
    sawVendor = false;

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetAiObjectContext() || !itemId)
        return nullptr;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    GuidVector const npcs = *context->GetValue<GuidVector>("nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* const creature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR);
        if (!creature)
            continue;

        sawVendor = true;
        VendorItemData const* const vendorItems = creature->GetVendorItems();
        if (!vendorItems)
            continue;

        for (uint32 slot = 0; slot < vendorItems->GetItemCount(); ++slot)
        {
            VendorItem const* const vendorItem = vendorItems->GetItem(slot);
            if (vendorItem && vendorItem->item == itemId)
            {
                vendorSlot = slot;
                vendorExtendedCost = vendorItem->ExtendedCost;
                return creature;
            }
        }
    }

    return nullptr;
}

GameObject* FindNearbyGuildBank(Player* bot)
{
    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetAiObjectContext())
        return nullptr;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    GuidVector const objects = *context->GetValue<GuidVector>("nearest game objects");
    for (ObjectGuid const guid : objects)
        if (GameObject* const go = botAI->GetGameObject(guid))
            if (bot->GetGameObjectIfCanInteractWith(go->GetGUID(), GAMEOBJECT_TYPE_GUILD_BANK))
                return go;

    return nullptr;
}

Item* FindBagItemByEntry(Player* bot, uint32 itemEntry)
{
    if (!bot || !itemEntry)
        return nullptr;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == itemEntry)
                return item;

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag const* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            if (Item* const item = bot->GetItemByPos(bag, slot))
                if (item->GetEntry() == itemEntry)
                    return item;
    }

    return nullptr;
}

Item* FindBankItemByEntry(Player* bot, uint32 itemEntry)
{
    if (!bot || !itemEntry)
        return nullptr;

    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        if (Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == itemEntry)
                return item;

    for (uint8 bag = BANK_SLOT_BAG_START; bag < BANK_SLOT_BAG_END; ++bag)
    {
        Bag const* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            if (Item* const item = bot->GetItemByPos(bag, slot))
                if (item->GetEntry() == itemEntry)
                    return item;
    }

    return nullptr;
}

void AddBankItemToSnapshot(std::map<uint32, uint32>& itemCounts, std::map<uint32, ItemTemplate const*>& itemTemplates, std::map<uint32, bool>& soulboundByEntry, Item* item)
{
    if (!item)
        return;

    ItemTemplate const* const proto = item->GetTemplate();
    if (!proto)
        return;

    uint32 const itemId = proto->ItemId;
    itemCounts[itemId] += item->GetCount();
    itemTemplates[itemId] = proto;
    if (item->IsSoulBound())
        soulboundByEntry[itemId] = true;
}

void AddItemEntryToSnapshot(std::map<uint32, uint32>& itemCounts, std::map<uint32, ItemTemplate const*>& itemTemplates, uint32 itemId, uint32 count)
{
    if (!itemId || !count)
        return;

    ItemTemplate const* const proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return;

    itemCounts[itemId] += count;
    itemTemplates[itemId] = proto;
}

int32 GetGuildBankTabWithdrawRemaining(Guild* guild, Player* player, uint8 tabId)
{
    if (!guild || !player)
        return 0;

    Guild::Member const* const member = guild->GetMember(player->GetGUID());
    if (!member)
        return 0;

    if (member->IsRank(GR_GUILDMASTER) || guild->GetLeaderGUID() == player->GetGUID())
        return std::numeric_limits<int32>::max();

    QueryResult result = CharacterDatabase.Query(
        "SELECT gbright, SlotPerDay FROM guild_bank_right "
        "WHERE guildid = {} AND TabId = {} AND rid = {}",
        guild->GetId(), uint32(tabId), uint32(member->GetRankId()));

    if (!result)
        return 0;

    Field* const fields = result->Fetch();
    uint32 const rights = fields[0].Get<uint32>();
    uint32 const slotsPerDay = fields[1].Get<uint32>();

    if ((rights & GUILD_BANK_RIGHT_VIEW_TAB) == 0 || slotsPerDay == 0)
        return 0;

    if (slotsPerDay == uint32(GUILD_WITHDRAW_SLOT_UNLIMITED))
        return std::numeric_limits<int32>::max();

    int32 const used = member->GetBankWithdrawValue(tabId);
    int64 const remaining = int64(slotsPerDay) - int64(used > 0 ? used : 0);
    return remaining > 0 ? int32(std::min<int64>(remaining, std::numeric_limits<int32>::max())) : 0;
}

int32 GetGuildBankWithdrawRemaining(Guild* guild, Player* player)
{
    int32 bestRemaining = 0;
    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
    {
        int32 const remaining = GetGuildBankTabWithdrawRemaining(guild, player, tabId);
        if (remaining == std::numeric_limits<int32>::max())
            return remaining;

        if (remaining > bestRemaining)
            bestRemaining = remaining;
    }

    return bestRemaining;
}

int32 GetEffectiveGuildBankWithdrawRemaining(Guild* guild, Player* requester, Player* bot)
{
    if (!guild || !requester || !bot)
        return 0;

    int32 bestRemaining = 0;
    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
    {
        int32 const requesterRemaining = GetGuildBankTabWithdrawRemaining(guild, requester, tabId);
        int32 const botRemaining = GetGuildBankTabWithdrawRemaining(guild, bot, tabId);
        int32 const effectiveRemaining = std::min(requesterRemaining, botRemaining);

        if (effectiveRemaining == std::numeric_limits<int32>::max())
            return effectiveRemaining;

        if (effectiveRemaining > bestRemaining)
            bestRemaining = effectiveRemaining;
    }

    return bestRemaining;
}

bool ConsumeGuildBankWithdrawSlot(Guild* guild, Player* player, uint8 tabId)
{
    if (!guild || !player)
        return false;

    int32 const remaining = GetGuildBankTabWithdrawRemaining(guild, player, tabId);
    if (remaining == std::numeric_limits<int32>::max())
        return true;

    if (remaining <= 0)
        return false;

    Guild::Member* const member = guild->GetMember(player->GetGUID());
    if (!member)
        return false;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    member->UpdateBankWithdrawValue(trans, tabId, 1);
    CharacterDatabase.CommitTransaction(trans);
    return true;
}

void SendBankPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string const prefixPayload = UrlEncodeField(effectiveBotName) + std::string(1, kFieldSeparator) + requestToken;

    SendAddonPacket(requester, replyType, "BANK_BEGIN", prefixPayload);

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "BANK_ERROR", prefixPayload + std::string(1, kFieldSeparator) + "NO_BOT");
        SendAddonPacket(requester, replyType, "BANK_END", prefixPayload);
        return;
    }

    if (!FindNearbyNpcWithFlag(bot, UNIT_NPC_FLAG_BANKER))
    {
        SendAddonPacket(requester, replyType, "BANK_ERROR", prefixPayload + std::string(1, kFieldSeparator) + "BANKER_NOT_FOUND");
        SendAddonPacket(requester, replyType, "BANK_END", prefixPayload);
        return;
    }

    std::map<uint32, uint32> itemCounts;
    std::map<uint32, ItemTemplate const*> itemTemplates;
    std::map<uint32, bool> soulboundByEntry;

    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        AddBankItemToSnapshot(itemCounts, itemTemplates, soulboundByEntry, bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bag = BANK_SLOT_BAG_START; bag < BANK_SLOT_BAG_END; ++bag)
    {
        Bag const* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            AddBankItemToSnapshot(itemCounts, itemTemplates, soulboundByEntry, bot->GetItemByPos(bag, slot));
    }

    for (auto const& entry : itemCounts)
    {
        ItemTemplate const* const proto = itemTemplates[entry.first];
        if (!proto)
            continue;

        std::string line = ChatHelper::FormatItem(proto, entry.second);
        if (soulboundByEntry[entry.first])
            line += " (soulbound)";

        SendAddonPacket(requester, replyType, "BANK_ITEM", prefixPayload + std::string(1, kFieldSeparator) + UrlEncodeField(line));
    }

    SendAddonPacket(requester, replyType, "BANK_END", prefixPayload);
}

void SendGuildBankPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string const prefixPayload = UrlEncodeField(effectiveBotName) + std::string(1, kFieldSeparator) + requestToken;

    SendAddonPacket(requester, replyType, "GBANK_BEGIN", prefixPayload);

    auto sendErrorAndEnd = [&](std::string const& reason)
    {
        SendAddonPacket(requester, replyType, "GBANK_ERROR", prefixPayload + std::string(1, kFieldSeparator) + reason);
        SendAddonPacket(requester, replyType, "GBANK_END", prefixPayload);
    };

    if (!bot)
    {
        sendErrorAndEnd("NO_BOT");
        return;
    }

    if (!bot->GetGuildId())
    {
        sendErrorAndEnd("BOT_NOT_IN_GUILD");
        return;
    }

    if (requester->GetGuildId() != bot->GetGuildId())
    {
        sendErrorAndEnd("NOT_IN_SAME_GUILD");
        return;
    }

    Guild* const guild = sGuildMgr->GetGuildById(bot->GetGuildId());
    if (!guild)
    {
        sendErrorAndEnd("BOT_NOT_IN_GUILD");
        return;
    }

    int32 const withdrawRemaining = GetEffectiveGuildBankWithdrawRemaining(guild, requester, bot);
    SendAddonPacket(
        requester,
        replyType,
        "GBANK_RIGHTS",
        prefixPayload + std::string(1, kFieldSeparator)
            + (withdrawRemaining != 0 ? "1" : "0")
            + std::string(1, kFieldSeparator)
            + std::to_string(withdrawRemaining));

    std::map<uint32, uint32> itemCounts;
    std::map<uint32, ItemTemplate const*> itemTemplates;

    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
    {
        if (!guild->MemberHasTabRights(requester->GetGUID(), tabId, GUILD_BANK_RIGHT_VIEW_TAB)
            || !guild->MemberHasTabRights(bot->GetGUID(), tabId, GUILD_BANK_RIGHT_VIEW_TAB))
            continue;

        QueryResult result = CharacterDatabase.Query(
            "SELECT ii.itemEntry, ii.count "
            "FROM guild_bank_item gbi "
            "INNER JOIN item_instance ii ON ii.guid = gbi.item_guid "
            "WHERE gbi.guildid = {} AND gbi.TabId = {} "
            "ORDER BY gbi.SlotId",
            guild->GetId(), uint32(tabId));

        if (!result)
            continue;

        do
        {
            Field* const fields = result->Fetch();
            AddItemEntryToSnapshot(itemCounts, itemTemplates, fields[0].Get<uint32>(), fields[1].Get<uint32>());
        }
        while (result->NextRow());
    }

    for (auto const& entry : itemCounts)
    {
        ItemTemplate const* const proto = itemTemplates[entry.first];
        if (!proto)
            continue;

        SendAddonPacket(requester, replyType, "GBANK_ITEM", prefixPayload + std::string(1, kFieldSeparator) + UrlEncodeField(ChatHelper::FormatItem(proto, entry.second)));
    }

    SendAddonPacket(requester, replyType, "GBANK_END", prefixPayload);
}

void SendSpellbookSnapshot(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::string const prefixPayload = trimmedBotName + std::string(1, kFieldSeparator) + requestToken;
    SendAddonPacket(requester, replyType, "SB_BEGIN", prefixPayload);

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "SB_END", prefixPayload);
        return;
    }

    std::vector<SpellbookEntryData> const entries = BuildSpellbookEntries(bot);
    for (SpellbookEntryData const& entry : entries)
    {
        std::ostringstream payload;
        payload << bot->GetName() << kFieldSeparator << requestToken << kFieldSeparator << entry.spellId;
        SendAddonPacket(requester, replyType, "SB_ITEM", payload.str());
    }

    SendAddonPacket(requester, replyType, "SB_END", bot->GetName() + std::string(1, kFieldSeparator) + requestToken);
}

void SendBotSkillPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::string const prefixPayload = UrlEncodeField(trimmedBotName) + std::string(1, kFieldSeparator) + requestToken;
    SendAddonPacket(requester, replyType, "BOT_SKILLS_BEGIN", prefixPayload);

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "BOT_SKILLS_END", prefixPayload);
        return;
    }

    for (BotSkillEntryData const& entry : BuildBotSkillEntries(bot))
        SendAddonPacket(requester, replyType, "BOT_SKILLS_ITEM", BuildBotSkillEntryPayload(bot, requestToken, entry));

    SendAddonPacket(requester, replyType, "BOT_SKILLS_END", UrlEncodeField(bot->GetName()) + std::string(1, kFieldSeparator) + requestToken);
}

void SendBotReputationPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::string const prefixPayload = UrlEncodeField(trimmedBotName) + std::string(1, kFieldSeparator) + requestToken;
    SendAddonPacket(requester, replyType, "BOT_REPUTATIONS_BEGIN", prefixPayload);

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "BOT_REPUTATIONS_END", prefixPayload);
        return;
    }

    for (BotReputationEntryData const& entry : BuildBotReputationEntries(bot))
        SendAddonPacket(requester, replyType, "BOT_REPUTATION_ITEM", BuildBotReputationEntryPayload(bot, requestToken, entry));

    SendAddonPacket(requester, replyType, "BOT_REPUTATIONS_END", UrlEncodeField(bot->GetName()) + std::string(1, kFieldSeparator) + requestToken);
}

void SendBotEmblemPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    static std::array<uint32, 6> const emblemIds = {{
        29434, // Badge of Justice
        40752, // Emblem of Heroism
        40753, // Emblem of Valor
        45624, // Emblem of Conquest
        47241, // Emblem of Triumph
        49426  // Emblem of Frost
    }};

    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::string const prefixPayload = UrlEncodeField(trimmedBotName) + std::string(1, kFieldSeparator) + requestToken;
    SendAddonPacket(requester, replyType, "BOT_EMBLEMS_BEGIN", prefixPayload);

    if (!bot)
    {
        SendAddonPacket(requester, replyType, "BOT_EMBLEMS_END", prefixPayload);
        return;
    }

    for (uint32 const itemId : emblemIds)
    {
        ItemTemplate const* const proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;

        std::ostringstream payload;
        payload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << requestToken
            << kFieldSeparator << itemId
            << kFieldSeparator << bot->GetItemCount(itemId, false);
        SendAddonPacket(requester, replyType, "BOT_EMBLEM_ITEM", payload.str());
    }

    std::ostringstream moneyPayload;
    moneyPayload << UrlEncodeField(bot->GetName())
        << kFieldSeparator << requestToken
        << kFieldSeparator << bot->GetMoney();
    SendAddonPacket(requester, replyType, "BOT_EMBLEMS_MONEY", moneyPayload.str());

    SendAddonPacket(requester, replyType, "BOT_EMBLEMS_END", UrlEncodeField(bot->GetName()) + std::string(1, kFieldSeparator) + requestToken);
}

struct TrainerSpellEntryData
{
    uint32 spellId = 0;
    uint32 cost = 0;
    bool canAfford = false;
};

bool BotHasGoldCheat(Player* bot)
{
    PlayerbotAI* const botAI = GetBotAI(bot);
    return botAI && botAI->HasCheat(BotCheatMask::gold);
}

uint32 GetBotTrainerFreeMoney(Player* bot)
{
    if (!bot)
        return 0;

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
        return bot->GetMoney();

    if (botAI->HasCheat(BotCheatMask::gold))
        return std::numeric_limits<uint32>::max();

    AiObjectContext* const context = botAI->GetAiObjectContext();
    if (!context)
        return bot->GetMoney();

    return AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::spells));
}

Creature* GetSelectedTrainer(Player* requester, uint32 expectedEntry, std::string& reason)
{
    reason = "NO_TRAINER_TARGET";
    if (!requester)
        return nullptr;

    Unit* const selectedUnit = requester->GetSelectedUnit();
    Creature* const creature = selectedUnit ? selectedUnit->ToCreature() : nullptr;
    if (!creature || !creature->IsTrainer())
        return nullptr;

    if (expectedEntry && creature->GetEntry() != expectedEntry)
    {
        reason = "TRAINER_CHANGED";
        return nullptr;
    }

    if (!sObjectMgr->GetTrainer(creature->GetEntry()))
    {
        reason = "NO_TRAINER";
        return nullptr;
    }

    reason = "OK";
    return creature;
}

std::vector<TrainerSpellEntryData> BuildTrainerSpellEntries(Player* bot, Creature* trainerCreature)
{
    std::vector<TrainerSpellEntryData> entries;
    if (!bot || !trainerCreature)
        return entries;

    Trainer::Trainer* const trainer = sObjectMgr->GetTrainer(trainerCreature->GetEntry());
    if (!trainer || !trainer->IsTrainerValidForPlayer(bot))
        return entries;

    float const reputationDiscount = bot->GetReputationPriceDiscount(trainerCreature);
    uint32 const freeMoney = GetBotTrainerFreeMoney(bot);
    bool const hasGoldCheat = BotHasGoldCheat(bot);

    for (auto const& spell : trainer->GetSpells())
    {
        Trainer::Spell const* const trainerSpell = trainer->GetSpell(spell.SpellId);
        if (!trainerSpell || !trainer->CanTeachSpell(bot, trainerSpell))
            continue;

        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(trainerSpell->SpellId);
        if (!spellInfo)
            continue;

        TrainerSpellEntryData entry;
        entry.spellId = trainerSpell->SpellId;
        entry.cost = static_cast<uint32>(floor(trainerSpell->MoneyCost * reputationDiscount));
        entry.canAfford = hasGoldCheat || freeMoney >= entry.cost;
        entries.push_back(entry);
    }

    return entries;
}

std::string BuildTrainerHeaderPayload(std::string const& botName, std::string const& requestToken, uint32 trainerEntry, std::string const& trainerName)
{
    std::ostringstream payload;
    payload << UrlEncodeField(botName)
        << kFieldSeparator << Trim(requestToken)
        << kFieldSeparator << trainerEntry
        << kFieldSeparator << UrlEncodeField(trainerName);
    return payload.str();
}

void SendTrainerErrorPacket(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, uint32 trainerEntry, std::string const& reason)
{
    std::ostringstream payload;
    payload << UrlEncodeField(botName)
        << kFieldSeparator << Trim(requestToken)
        << kFieldSeparator << trainerEntry
        << kFieldSeparator << UrlEncodeField(reason);
    SendAddonPacket(requester, replyType, "TRAINER_ERROR", payload.str());
}

void SendTrainerPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string trainerReason;
    Creature* const trainerCreature = GetSelectedTrainer(requester, 0, trainerReason);
    uint32 const trainerEntry = trainerCreature ? trainerCreature->GetEntry() : 0;
    std::string const trainerName = trainerCreature ? trainerCreature->GetName() : "";
    std::string const headerPayload = BuildTrainerHeaderPayload(effectiveBotName, requestToken, trainerEntry, trainerName);

    SendAddonPacket(requester, replyType, "TRAINER_BEGIN", headerPayload);

    if (!bot)
    {
        SendTrainerErrorPacket(requester, replyType, effectiveBotName, requestToken, trainerEntry, "NO_BOT");
        SendAddonPacket(requester, replyType, "TRAINER_END", headerPayload);
        return;
    }

    if (!trainerCreature)
    {
        SendTrainerErrorPacket(requester, replyType, effectiveBotName, requestToken, trainerEntry, trainerReason);
        SendAddonPacket(requester, replyType, "TRAINER_END", headerPayload);
        return;
    }

    Trainer::Trainer* const trainer = sObjectMgr->GetTrainer(trainerCreature->GetEntry());
    if (!trainer || !trainer->IsTrainerValidForPlayer(bot))
    {
        SendTrainerErrorPacket(requester, replyType, effectiveBotName, requestToken, trainerEntry, "INVALID_TRAINER");
        SendAddonPacket(requester, replyType, "TRAINER_END", headerPayload);
        return;
    }

    for (TrainerSpellEntryData const& entry : BuildTrainerSpellEntries(bot, trainerCreature))
    {
        std::ostringstream payload;
        payload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << Trim(requestToken)
            << kFieldSeparator << trainerEntry
            << kFieldSeparator << entry.spellId
            << kFieldSeparator << entry.cost
            << kFieldSeparator << (entry.canAfford ? "1" : "0");
        SendAddonPacket(requester, replyType, "TRAINER_ITEM", payload.str());
    }

    SendAddonPacket(requester, replyType, "TRAINER_END", headerPayload);
}

bool LearnTrainerSpell(Player* bot, SpellInfo const* spellInfo, uint32 cost, std::string& reason)
{
    if (!bot || !spellInfo)
    {
        reason = "NO_SPELL";
        return false;
    }

    if (!BotHasGoldCheat(bot))
    {
        if (GetBotTrainerFreeMoney(bot) < cost)
        {
            reason = "TOO_EXPENSIVE";
            return false;
        }

        bot->ModifyMoney(-static_cast<int32>(cost));
    }

    if (spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
        bot->CastSpell(bot, spellInfo->Id, true);
    else
        bot->learnSpell(spellInfo->Id, false);

    reason = "OK";
    return true;
}

void SendTrainerLearnResult(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, uint32 trainerEntry, std::string const& spellIdValue, bool ok, std::string const& reason, uint32 learnedCount, uint32 spent)
{
    std::ostringstream payload;
    payload << UrlEncodeField(botName)
        << kFieldSeparator << Trim(requestToken)
        << kFieldSeparator << trainerEntry
        << kFieldSeparator << UrlEncodeField(Trim(spellIdValue))
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << learnedCount
        << kFieldSeparator << spent;
    SendAddonPacket(requester, replyType, "TRAINER_LEARN", payload.str());
}

void RunTrainerLearnCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& trainerEntryValue, std::string const& spellIdValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 expectedTrainerEntry = 0;
    TryParseUint32Field(Trim(trainerEntryValue), 1, std::numeric_limits<uint32>::max(), expectedTrainerEntry);

    std::string const requestedSpell = ToUpper(Trim(spellIdValue));
    bool const learnAll = requestedSpell == "ALL";
    uint32 requestedSpellId = 0;
    if (!learnAll)
        TryParseUint32Field(requestedSpell, 1, std::numeric_limits<uint32>::max(), requestedSpellId);

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendTrainerLearnResult(requester, replyType, effectiveBotName, token, expectedTrainerEntry, spellIdValue, false, "NO_BOT", 0, 0);
        return;
    }

    if (!expectedTrainerEntry)
    {
        SendTrainerLearnResult(requester, replyType, effectiveBotName, token, expectedTrainerEntry, spellIdValue, false, "NO_TRAINER", 0, 0);
        return;
    }

    if (!learnAll && !requestedSpellId)
    {
        SendTrainerLearnResult(requester, replyType, effectiveBotName, token, expectedTrainerEntry, spellIdValue, false, "NO_SPELL", 0, 0);
        return;
    }

    std::string trainerReason;
    Creature* const trainerCreature = GetSelectedTrainer(requester, expectedTrainerEntry, trainerReason);
    if (!trainerCreature)
    {
        SendTrainerLearnResult(requester, replyType, effectiveBotName, token, expectedTrainerEntry, spellIdValue, false, trainerReason, 0, 0);
        return;
    }

    Trainer::Trainer* const trainer = sObjectMgr->GetTrainer(trainerCreature->GetEntry());
    if (!trainer || !trainer->IsTrainerValidForPlayer(bot))
    {
        SendTrainerLearnResult(requester, replyType, effectiveBotName, token, expectedTrainerEntry, spellIdValue, false, "INVALID_TRAINER", 0, 0);
        return;
    }

    std::vector<TrainerSpellEntryData> const entries = BuildTrainerSpellEntries(bot, trainerCreature);
    uint32 learnedCount = 0;
    uint32 spent = 0;
    std::string reason = "NO_MATCHING_SPELL";

    for (TrainerSpellEntryData const& entry : entries)
    {
        if (!learnAll && entry.spellId != requestedSpellId)
            continue;

        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(entry.spellId);
        std::string learnReason;
        if (LearnTrainerSpell(bot, spellInfo, entry.cost, learnReason))
        {
            ++learnedCount;
            spent += entry.cost;
            reason = "OK";
        }
        else if (learnReason != "OK" && learnedCount == 0)
            reason = learnReason;

        if (!learnAll)
            break;
    }

    SendTrainerLearnResult(requester, replyType, effectiveBotName, token, expectedTrainerEntry, spellIdValue, learnedCount > 0, reason, learnedCount, spent);
}

void SendProfessionRecipePackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& skillIdValue, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    uint32 skillId = 0;
    TryParseUint32Field(Trim(skillIdValue), 1, std::numeric_limits<uint32>::max(), skillId);
    Player* const bot = FindBotByName(requester, trimmedBotName);

    std::ostringstream beginPayload;
    beginPayload << UrlEncodeField(trimmedBotName) << kFieldSeparator << requestToken << kFieldSeparator << skillId;
    SendAddonPacket(requester, replyType, "PROFESSION_RECIPES_BEGIN", beginPayload.str());

    if (!bot || !skillId)
    {
        SendAddonPacket(requester, replyType, "PROFESSION_RECIPES_END", beginPayload.str());
        return;
    }

    for (ProfessionRecipeEntryData const& entry : BuildProfessionRecipeEntries(bot, skillId))
    {
        std::ostringstream payload;
        payload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << requestToken
            << kFieldSeparator << skillId
            << kFieldSeparator << entry.spellId
            << kFieldSeparator << entry.itemId
            << kFieldSeparator << UrlEncodeField(entry.difficulty)
            << kFieldSeparator << entry.craftable
            << kFieldSeparator << UrlEncodeField(entry.materials);
        SendAddonPacket(requester, replyType, "PROFESSION_RECIPES_ITEM", payload.str());
    }

    std::ostringstream endPayload;
    endPayload << UrlEncodeField(bot->GetName()) << kFieldSeparator << requestToken << kFieldSeparator << skillId;
    SendAddonPacket(requester, replyType, "PROFESSION_RECIPES_END", endPayload.str());
}

struct OutfitSetSnapshot
{
    std::string name;
    std::vector<std::string> items;
};

std::string BuildOutfitRawLine(OutfitSetSnapshot const& outfit)
{
    std::ostringstream out;
    out << outfit.name << ":";

    for (std::string const& item : outfit.items)
    {
        if (!item.empty())
            out << ' ' << item;
    }

    return out.str();
}

void AppendOutfitItemLink(OutfitSetSnapshot& outfit, uint32 itemEntry)
{
    if (!itemEntry)
        return;

    ItemTemplate const* const proto = sObjectMgr->GetItemTemplate(itemEntry);
    if (!proto)
        return;

    outfit.items.push_back(ChatHelper::FormatItem(proto, 1));
}

std::vector<uint32> ParseOutfitItemEntries(std::string const& value)
{
    std::vector<uint32> entries;
    std::stringstream in(value);
    std::string item;

    while (std::getline(in, item, ','))
    {
        item = Trim(item);
        if (item.empty())
            continue;

        uint32 itemEntry = 0;
        if (TryParseUint32Field(item, 1, std::numeric_limits<uint32>::max(), itemEntry))
            entries.push_back(itemEntry);
    }

    return entries;
}

std::vector<OutfitSetSnapshot> BuildOutfitSnapshots(Player* bot)
{
    std::vector<OutfitSetSnapshot> outfits;
    if (!bot)
        return outfits;

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI)
        return outfits;

    auto* context = botAI->GetAiObjectContext();
    if (!context)
        return outfits;

    std::vector<std::string>& savedOutfits = AI_VALUE(std::vector<std::string>&, "outfit list");

    for (std::string const& savedOutfit : savedOutfits)
    {
        std::string const trimmed = Trim(savedOutfit);
        if (trimmed.empty())
            continue;

        size_t const separator = trimmed.find('=');
        if (separator == std::string::npos)
            continue;

        OutfitSetSnapshot outfit;
        outfit.name = Trim(trimmed.substr(0, separator));
        if (outfit.name.empty())
            outfit.name = "Outfit";

        std::vector<uint32> const entries = ParseOutfitItemEntries(trimmed.substr(separator + 1));
        for (uint32 const itemEntry : entries)
            AppendOutfitItemLink(outfit, itemEntry);

        outfits.push_back(outfit);
    }

    return outfits;
}

void SendOutfitPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string const headerPayload = UrlEncodeField(effectiveBotName) + std::string(1, kFieldSeparator) + Trim(requestToken);

    SendAddonPacket(requester, replyType, "OUTFITS_BEGIN", headerPayload);

    if (bot)
    {
        std::vector<OutfitSetSnapshot> const outfits = BuildOutfitSnapshots(bot);
        for (OutfitSetSnapshot const& outfit : outfits)
        {
            std::ostringstream payload;
            payload << UrlEncodeField(bot->GetName())
                << kFieldSeparator << Trim(requestToken)
                << kFieldSeparator << UrlEncodeField(BuildOutfitRawLine(outfit));

            SendAddonPacket(requester, replyType, "OUTFITS_ITEM", payload.str());
        }
    }

    SendAddonPacket(requester, replyType, "OUTFITS_END", headerPayload);
}

struct OutfitCommandParts
{
    std::string name;
    std::string action;
};

OutfitCommandParts ParseOutfitCommandSuffix(std::string const& suffix)
{
    OutfitCommandParts parts;

    std::string const cleaned = Trim(suffix);
    std::size_t const lastSpace = cleaned.find_last_of(' ');
    if (lastSpace == std::string::npos || lastSpace == 0 || lastSpace + 1 >= cleaned.size())
        return parts;

    parts.name = Trim(cleaned.substr(0, lastSpace));
    parts.action = ToUpper(Trim(cleaned.substr(lastSpace + 1)));
    return parts;
}

bool IsAllowedOutfitCommandSuffix(std::string const& suffix)
{
    OutfitCommandParts const parts = ParseOutfitCommandSuffix(suffix);
    if (parts.name.empty())
        return false;

    return parts.action == "EQUIP" || parts.action == "REPLACE" || parts.action == "UPDATE" || parts.action == "RESET";
}



std::string SanitizeOutfitCommandSuffix(std::string suffix)
{
    suffix.erase(std::remove(suffix.begin(), suffix.end(), '\r'), suffix.end());
    suffix.erase(std::remove(suffix.begin(), suffix.end(), '\n'), suffix.end());
    return Trim(suffix);
}

std::vector<uint32> CollectCurrentEquippedOutfitEntries(Player* bot)
{
    std::set<uint32> uniqueEntries;

    if (bot)
    {
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item const* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item && item->GetEntry())
                uniqueEntries.insert(item->GetEntry());
        }
    }

    return std::vector<uint32>(uniqueEntries.begin(), uniqueEntries.end());
}

bool SaveOutfitEntries(PlayerbotAI* botAI, std::string const& outfitName, std::vector<uint32> const& entries)
{
    if (!botAI)
        return false;

    auto* context = botAI->GetAiObjectContext();
    if (!context)
        return false;

    std::string const name = Trim(outfitName);
    if (name.empty())
        return false;

    std::vector<std::string>& savedOutfits = AI_VALUE(std::vector<std::string>&, "outfit list");

    for (std::vector<std::string>::iterator it = savedOutfits.begin(); it != savedOutfits.end(); ++it)
    {
        std::string const existing = Trim(*it);
        std::size_t const separator = existing.find('=');
        std::string const existingName = Trim(separator == std::string::npos ? existing : existing.substr(0, separator));
        if (existingName == name)
        {
            savedOutfits.erase(it);
            break;
        }
    }

    if (entries.empty())
        return true;

    std::ostringstream out;
    out << name << '=';
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        if (index)
            out << ',';
        out << entries[index];
    }

    savedOutfits.push_back(out.str());
    return true;
}

bool ApplyBridgeNativeOutfitCommand(Player* bot, std::string const& suffix, bool persist)
{
    if (!bot)
        return false;

    OutfitCommandParts const parts = ParseOutfitCommandSuffix(suffix);
    if (parts.name.empty())
        return false;

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI)
        return false;

    if (parts.action == "EQUIP" || parts.action == "REPLACE")
    {
        std::vector<uint32> entries;

        {
            auto* context = botAI->GetAiObjectContext();
            if (!context)
                return false;

            std::string const outfitName = Trim(parts.name);
            if (outfitName.empty())
                return false;

            std::vector<std::string>& savedOutfits = AI_VALUE(std::vector<std::string>&, "outfit list");
            for (std::string const& savedOutfit : savedOutfits)
            {
                std::string const existing = Trim(savedOutfit);
                std::size_t const separator = existing.find('=');
                if (separator == std::string::npos)
                    continue;

                std::string const existingName = Trim(existing.substr(0, separator));
                if (existingName != outfitName)
                    continue;

                entries = ParseOutfitItemEntries(existing.substr(separator + 1));
                break;
            }
        }

        if (entries.empty())
            return false;

        auto findItemByEntry = [bot](uint32 itemEntry) -> Item*
        {
            if (!bot || !itemEntry)
                return nullptr;

            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (item && item->GetEntry() == itemEntry)
                    return item;
            }

            for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            {
                Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (item && item->GetEntry() == itemEntry)
                    return item;
            }

            for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
            {
                Bag* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
                if (!pBag)
                    continue;

                for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
                {
                    Item* const item = bot->GetItemByPos(bag, slot);
                    if (item && item->GetEntry() == itemEntry)
                        return item;
                }
            }

            return nullptr;
        };

        auto equipItemByEntry = [bot, botAI, &findItemByEntry](uint32 itemEntry) -> bool
        {
            if (!bot || !bot->GetSession() || !botAI || !itemEntry)
                return false;

            Item* const item = findItemByEntry(itemEntry);
            if (!item)
                return false;

            ItemTemplate const* const itemProto = item->GetTemplate();
            if (!itemProto)
                return false;

            if (itemProto->InventoryType == INVTYPE_AMMO)
            {
                bot->SetAmmo(itemProto->ItemId);
                return true;
            }

            if (itemProto->Class == ITEM_CLASS_CONTAINER)
                return false;

            uint8 dstSlot = NULL_SLOT;
            if (itemProto->InventoryType == INVTYPE_RANGED || itemProto->InventoryType == INVTYPE_THROWN || itemProto->InventoryType == INVTYPE_RANGEDRIGHT)
                dstSlot = EQUIPMENT_SLOT_RANGED;
            else
                dstSlot = botAI->FindEquipSlot(itemProto, NULL_SLOT, true);

            if (dstSlot == NULL_SLOT)
                return false;

            if ((dstSlot == EQUIPMENT_SLOT_FINGER1 || dstSlot == EQUIPMENT_SLOT_TRINKET1)
                && bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot)
                && !bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot + 1))
            {
                ++dstSlot;
            }

            if (item->GetBagSlot() == INVENTORY_SLOT_BAG_0 && item->GetSlot() == dstSlot)
                return true;

            WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
            ObjectGuid itemGuid = item->GetGUID();
            packet << itemGuid << dstSlot;

            WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
            nicePacket.Read();
            bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);
            return true;
        };

        if (parts.action == "REPLACE")
        {
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                Item const* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (!item)
                    continue;

                uint8 const bagIndex = item->GetBagSlot();
                uint8 const dstBag = NULL_BAG;

                WorldPacket packet(CMSG_AUTOSTORE_BAG_ITEM, 3);
                packet << bagIndex << slot << dstBag;

                WorldPackets::Item::AutoStoreBagItem nicePacket(std::move(packet));
                nicePacket.Read();
                bot->GetSession()->HandleAutoStoreBagItemOpcode(nicePacket);
            }
        }

        bool equippedAny = false;
        for (uint32 const itemEntry : entries)
        {
            if (equipItemByEntry(itemEntry))
                equippedAny = true;
        }

        if (!equippedAny)
            return false;

        return true;
    }

    if (parts.action == "UPDATE")
    {
        std::vector<uint32> const entries = CollectCurrentEquippedOutfitEntries(bot);
        if (entries.empty())
            return false;

        if (!SaveOutfitEntries(botAI, parts.name, entries))
            return false;

        if (persist)
            PlayerbotRepository::instance().Save(botAI);
        return true;
    }

    if (parts.action == "RESET")
    {
        if (!SaveOutfitEntries(botAI, parts.name, std::vector<uint32>()))
            return false;

        if (persist)
            PlayerbotRepository::instance().Save(botAI);
        return true;
    }

    return false;
}

bool ExecuteSilentBotCommand(Player* requester, Player* bot, std::string const& command)
{
    if (!requester || !bot || command.empty())
        return false;

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI)
        return false;

    botAI->HandleCommand(CHAT_MSG_WHISPER, command, requester);
    return true;
}

uint32 MoveMatchingBagItemsToBank(Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    if (!FindNearbyNpcWithFlag(bot, UNIT_NPC_FLAG_BANKER))
    {
        reason = "BANKER_NOT_FOUND";
        return 0;
    }

    uint32 moved = 0;
    while (Item* const item = FindBagItemByEntry(bot, itemId))
    {
        uint32 const stackCount = item->GetCount();
        ItemPosCountVec dest;
        InventoryResult const msg = bot->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            reason = "BANK_FULL";
            return moved;
        }

        bot->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
        bot->BankItem(dest, item, true);
        moved += stackCount;

        if (requestedCount > 0 && moved >= requestedCount)
            break;
    }

    if (!moved)
        reason = "ITEM_NOT_FOUND";

    return moved;
}

uint32 MoveMatchingBankItemsToBags(Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    if (!FindNearbyNpcWithFlag(bot, UNIT_NPC_FLAG_BANKER))
    {
        reason = "BANKER_NOT_FOUND";
        return 0;
    }

    uint32 moved = 0;
    while (Item* const item = FindBankItemByEntry(bot, itemId))
    {
        uint32 const stackCount = item->GetCount();
        ItemPosCountVec dest;
        InventoryResult const msg = bot->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            reason = "BAGS_FULL";
            return moved;
        }

        bot->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
        bot->StoreItem(dest, item, true);
        moved += stackCount;

        if (requestedCount > 0 && moved >= requestedCount)
            break;
    }

    if (!moved)
        reason = "ITEM_NOT_FOUND";

    return moved;
}

bool TryDepositGuildBankStack(
    Guild* guild,
    Player* requester,
    Player* bot,
    uint8 playerBag,
    uint8 playerSlot,
    ObjectGuid const& itemGuid,
    bool& hasAuthorizedTab)
{
    hasAuthorizedTab = false;
    if (!guild || !requester || !bot)
        return false;

    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
    {
        if (!guild->MemberHasTabRights(
                requester->GetGUID(), tabId, GUILD_BANK_RIGHT_DEPOSIT_ITEM) ||
            !guild->MemberHasTabRights(
                bot->GetGUID(), tabId, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
        {
            continue;
        }

        hasAuthorizedTab = true;
        guild->SwapItemsWithInventory(
            bot, false, tabId, 255, playerBag, playerSlot, 0);

        Item* const remaining = bot->GetItemByPos(playerBag, playerSlot);
        if (!remaining || remaining->GetGUID() != itemGuid)
            return true;
    }

    return false;
}
uint32 MoveMatchingBagItemsToGuildBank(Player* requester, Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!requester || !bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    if (!bot->GetGuildId())
    {
        reason = "BOT_NOT_IN_GUILD";
        return 0;
    }

    if (requester->GetGuildId() != bot->GetGuildId())
    {
        reason = "NOT_IN_SAME_GUILD";
        return 0;
    }

    Guild* const guild = sGuildMgr->GetGuildById(bot->GetGuildId());
    if (!guild)
    {
        reason = "BOT_NOT_IN_GUILD";
        return 0;
    }

    if (!FindNearbyGuildBank(bot))
    {
        reason = "GUILD_BANK_NOT_FOUND";
        return 0;
    }


    uint32 moved = 0;
    while (Item* const item = FindBagItemByEntry(bot, itemId))
    {
        uint32 const stackCount = item->GetCount();
        uint8 const playerSlot = item->GetSlot();
        uint8 const playerBag = item->GetBagSlot();
        ObjectGuid const itemGuid = item->GetGUID();

        bool hasAuthorizedTab = false;
        if (!TryDepositGuildBankStack(
                guild, requester, bot, playerBag, playerSlot, itemGuid, hasAuthorizedTab))
        {
            reason = hasAuthorizedTab ? "GUILD_BANK_FULL" : "NO_GUILD_BANK_RIGHTS";
            return moved;
        }

        moved += stackCount;

        if (requestedCount > 0 && moved >= requestedCount)
            break;
    }

    if (!moved)
        reason = "ITEM_NOT_FOUND";

    return moved;
}

uint32 MoveMatchingGuildBankItemsToBags(Player* requester, Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!requester || !bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    if (!bot->GetGuildId())
    {
        reason = "BOT_NOT_IN_GUILD";
        return 0;
    }

    if (requester->GetGuildId() != bot->GetGuildId())
    {
        reason = "NOT_IN_SAME_GUILD";
        return 0;
    }

    Guild* const guild = sGuildMgr->GetGuildById(bot->GetGuildId());
    if (!guild)
    {
        reason = "BOT_NOT_IN_GUILD";
        return 0;
    }

    if (!FindNearbyGuildBank(bot))
    {
        reason = "GUILD_BANK_NOT_FOUND";
        return 0;
    }

    if (GetEffectiveGuildBankWithdrawRemaining(guild, requester, bot) == 0)
    {
        reason = "NO_GUILD_BANK_RIGHTS";
        return 0;
    }

    QueryResult result = CharacterDatabase.Query(
        "SELECT gbi.TabId, gbi.SlotId, ii.count "
        "FROM guild_bank_item gbi "
        "INNER JOIN item_instance ii ON ii.guid = gbi.item_guid "
        "WHERE gbi.guildid = {} AND ii.itemEntry = {} "
        "ORDER BY gbi.TabId, gbi.SlotId",
        guild->GetId(), itemId);

    if (!result)
    {
        reason = "ITEM_NOT_FOUND";
        return 0;
    }

    bool foundAny = false;
    bool foundWithdrawable = false;
    uint32 moved = 0;

    do
    {
        Field* const fields = result->Fetch();
        uint8 const tabId = fields[0].Get<uint8>();
        uint8 const slotId = fields[1].Get<uint8>();
        uint32 const stackCount = fields[2].Get<uint32>();
        foundAny = true;

        if (GetGuildBankTabWithdrawRemaining(guild, requester, tabId) == 0
            || GetGuildBankTabWithdrawRemaining(guild, bot, tabId) == 0)
            continue;

        foundWithdrawable = true;

        uint32 splitCount = 0;
        if (requestedCount > 0)
        {
            uint32 const remainingRequest = requestedCount > moved ? requestedCount - moved : 0;
            if (!remainingRequest)
                break;

            splitCount = std::min(stackCount, remainingRequest);
            if (splitCount >= stackCount)
                splitCount = 0;
        }

        uint32 const before = bot->GetItemCount(itemId, false);
        guild->SwapItemsWithInventory(bot, true, tabId, slotId, NULL_BAG, NULL_SLOT, splitCount);
        uint32 const after = bot->GetItemCount(itemId, false);

        if (after > before)
        {
            moved += after - before;
            if (requester->GetGUID() != bot->GetGUID())
                ConsumeGuildBankWithdrawSlot(guild, requester, tabId);
        }

        if (requestedCount > 0 && moved >= requestedCount)
            break;
    }
    while (result->NextRow());

    if (!moved)
    {
        if (!foundAny)
            reason = "ITEM_NOT_FOUND";
        else if (!foundWithdrawable)
            reason = "NO_GUILD_BANK_RIGHTS";
        else
            reason = "BAGS_FULL";
    }

    return moved;
}

bool IsBridgeSellGreyCandidate(PlayerbotAI* botAI, Item* item)
{
    if (!botAI || !item)
        return false;

    ItemTemplate const* const proto = item->GetTemplate();
    if (!proto)
        return false;

    if (proto->Quality != ITEM_QUALITY_POOR || !proto->SellPrice)
        return false;

    if (proto->Class == ITEM_CLASS_QUEST || proto->Class == ITEM_CLASS_KEY)
        return false;

    // Keep the same explicit protection already enforced by the addon.
    if (item->GetEntry() == 6948)
        return false;

    // An active quest objective can require a poor-quality sellable item whose
    // template is not ITEM_CLASS_QUEST. Reuse Playerbots' audited item-usage
    // classification and keep the item when it is currently needed for a quest.
    AiObjectContext* const context = botAI->GetAiObjectContext();
    if (!context)
        return false;

    ItemUsage const usage = context->GetValue<ItemUsage>("item usage", item->GetEntry())->Get();
    if (usage == ITEM_USAGE_QUEST)
        return false;

    return true;
}

Creature* FindNearbyInteractiveVendor(Player* bot)
{
    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!bot || !botAI || !botAI->GetAiObjectContext())
        return nullptr;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    GuidVector const npcs = *context->GetValue<GuidVector>("nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* const creature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR);
        if (creature)
            return creature;
    }

    return nullptr;
}

void CollectBridgeSellGreyCandidate(PlayerbotAI* botAI, Item* item, std::vector<ObjectGuid>& itemGuids)
{
    if (IsBridgeSellGreyCandidate(botAI, item))
        itemGuids.push_back(item->GetGUID());
}

uint32 SellGreyBagItems(Player* bot, std::string& reason)
{
    if (!bot || !bot->GetSession())
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    Creature* const vendor = FindNearbyInteractiveVendor(bot);
    if (!vendor)
    {
        reason = "VENDOR_NOT_FOUND";
        return 0;
    }

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetAiObjectContext())
    {
        reason = "FAILED";
        return 0;
    }

    std::vector<ObjectGuid> itemGuids;

    // Snapshot candidate GUIDs before invoking the sell handler so inventory
    // mutations cannot invalidate the bag iteration.
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        CollectBridgeSellGreyCandidate(botAI, bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), itemGuids);

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* const pBag = static_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag));
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            CollectBridgeSellGreyCandidate(botAI, pBag->GetItemByPos(slot), itemGuids);
    }

    if (itemGuids.empty())
    {
        reason = "ITEM_NOT_FOUND";
        return 0;
    }

    uint32 sold = 0;

    for (ObjectGuid const itemGuid : itemGuids)
    {
        Item* const item = bot->GetItemByGuid(itemGuid);
        if (!IsBridgeSellGreyCandidate(botAI, item))
            continue;

        uint32 const itemId = item->GetEntry();
        uint32 const before = bot->GetItemCount(itemId, false);
        uint32 const moneyBefore = bot->GetMoney();

        WorldPacket packet(CMSG_SELL_ITEM);
        packet << vendor->GetGUID() << itemGuid << uint32(0);

        WorldPackets::Item::SellItem sellPacket(std::move(packet));
        sellPacket.Read();
        bot->GetSession()->HandleSellItemOpcode(sellPacket);

        // Match Playerbots SellAction semantics when the gold cheat is active,
        // without calling SellAction/TellMaster and therefore without chat spam.
        if (botAI->HasCheat(BotCheatMask::gold))
            bot->SetMoney(moneyBefore);

        uint32 const after = bot->GetItemCount(itemId, false);
        if (before > after)
            sold += before - after;
    }

    if (!sold && reason.empty())
        reason = "FAILED";

    return sold;
}

void AddBridgeCraftOutputsFromSpellInfo(SpellInfo const* spellInfo, std::set<uint32>& itemIds)
{
    if (!spellInfo)
        return;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[i];
        if ((effect.Effect == SPELL_EFFECT_CREATE_ITEM || effect.Effect == SPELL_EFFECT_CREATE_ITEM_2) &&
            effect.ItemType > 0)
            itemIds.insert(static_cast<uint32>(effect.ItemType));
    }
}

void AddBridgeCraftOutputsFromSpell(uint32 spellId, std::set<uint32>& itemIds,
                                    std::set<uint32>* visitedSpellIds = nullptr)
{
    std::set<uint32> visitedSpells;
    std::vector<uint32> pendingSpells;
    pendingSpells.push_back(spellId);

    while (!pendingSpells.empty())
    {
        uint32 const currentSpellId = pendingSpells.back();
        pendingSpells.pop_back();

        if (!currentSpellId || !visitedSpells.insert(currentSpellId).second)
            continue;

        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(currentSpellId);
        if (!spellInfo)
            continue;

        if (visitedSpellIds)
            visitedSpellIds->insert(currentSpellId);

        AddBridgeCraftOutputsFromSpellInfo(spellInfo, itemIds);

        // Follow the complete TriggerSpell graph. The visited set prevents
        // cycles while preserving every concrete create-item output reachable
        // from a profession or recipe spell.
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            uint32 const triggerSpell = spellInfo->Effects[i].TriggerSpell;
            if (triggerSpell && visitedSpells.find(triggerSpell) == visitedSpells.end())
                pendingSpells.push_back(triggerSpell);
        }
    }
}

uint32 GetBridgeRecipeSkillId(ItemTemplate const* recipe)
{
    if (!recipe || recipe->Class != ITEM_CLASS_RECIPE)
        return 0;

    switch (recipe->SubClass)
    {
        case ITEM_SUBCLASS_LEATHERWORKING_PATTERN: return SKILL_LEATHERWORKING;
        case ITEM_SUBCLASS_TAILORING_PATTERN: return SKILL_TAILORING;
        case ITEM_SUBCLASS_ENGINEERING_SCHEMATIC: return SKILL_ENGINEERING;
        case ITEM_SUBCLASS_BLACKSMITHING: return SKILL_BLACKSMITHING;
        case ITEM_SUBCLASS_COOKING_RECIPE: return SKILL_COOKING;
        case ITEM_SUBCLASS_ALCHEMY_RECIPE: return SKILL_ALCHEMY;
        case ITEM_SUBCLASS_FIRST_AID_MANUAL: return SKILL_FIRST_AID;
        case ITEM_SUBCLASS_ENCHANTING_FORMULA: return SKILL_ENCHANTING;
        case ITEM_SUBCLASS_JEWELCRAFTING_RECIPE: return SKILL_JEWELCRAFTING;
        case ITEM_SUBCLASS_FISHING_MANUAL: return SKILL_FISHING;
        default: return 0;
    }
}

std::array<SkillType, 14> const& GetBridgeCraftProtectionSkills()
{
    static std::array<SkillType, 14> const skills = {
        SKILL_ALCHEMY,
        SKILL_ENCHANTING,
        SKILL_SKINNING,
        SKILL_TAILORING,
        SKILL_LEATHERWORKING,
        SKILL_ENGINEERING,
        SKILL_HERBALISM,
        SKILL_INSCRIPTION,
        SKILL_MINING,
        SKILL_BLACKSMITHING,
        SKILL_COOKING,
        SKILL_FIRST_AID,
        SKILL_FISHING,
        SKILL_JEWELCRAFTING
    };

    return skills;
}

using BridgeReferenceLootRows = std::map<uint32, std::vector<std::pair<uint32, uint32>>>;

BridgeReferenceLootRows BuildBridgeReferenceLootRows()
{
    BridgeReferenceLootRows rows;

    QueryResult result = WorldDatabase.Query(
        "SELECT Entry, Item, Reference FROM reference_loot_template");
    if (!result)
        return rows;

    do
    {
        Field* const fields = result->Fetch();
        uint32 const entryId = fields[0].Get<uint32>();
        uint32 const itemId = fields[1].Get<uint32>();
        uint32 const referenceId = fields[2].Get<uint32>();

        if (entryId)
            rows[entryId].push_back({itemId, referenceId});
    } while (result->NextRow());

    return rows;
}

BridgeReferenceLootRows BuildBridgeLootTableRows(char const* tableName)
{
    BridgeReferenceLootRows rows;

    QueryResult result = WorldDatabase.Query(
        "SELECT Entry, Item, Reference FROM {}", tableName);
    if (!result)
        return rows;

    do
    {
        Field* const fields = result->Fetch();
        uint32 const entryId = fields[0].Get<uint32>();
        uint32 const itemId = fields[1].Get<uint32>();
        uint32 const referenceId = fields[2].Get<uint32>();

        if (entryId)
            rows[entryId].push_back({itemId, referenceId});
    } while (result->NextRow());

    return rows;
}

void AddBridgeReferenceLootOutputs(std::set<uint32> const& initialReferences,
                                   BridgeReferenceLootRows const& referenceRows,
                                   std::set<uint32>& itemIds)
{
    std::set<uint32> visitedReferences;
    std::vector<uint32> pendingReferences(initialReferences.begin(), initialReferences.end());

    while (!pendingReferences.empty())
    {
        uint32 const referenceId = pendingReferences.back();
        pendingReferences.pop_back();

        if (!referenceId || !visitedReferences.insert(referenceId).second)
            continue;

        auto const rowsIt = referenceRows.find(referenceId);
        if (rowsIt == referenceRows.end())
            continue;

        for (auto const& row : rowsIt->second)
        {
            if (row.first)
                itemIds.insert(row.first);

            if (row.second && visitedReferences.find(row.second) == visitedReferences.end())
                pendingReferences.push_back(row.second);
        }
    }
}

void AddBridgeLootEntryOutputs(uint32 entryId,
                               BridgeReferenceLootRows const& lootRows,
                               BridgeReferenceLootRows const& referenceRows,
                               std::set<uint32>& itemIds)
{
    auto const rowsIt = lootRows.find(entryId);
    if (!entryId || rowsIt == lootRows.end())
        return;

    std::set<uint32> references;
    for (auto const& row : rowsIt->second)
    {
        if (row.first)
            itemIds.insert(row.first);

        if (row.second)
            references.insert(row.second);
    }

    AddBridgeReferenceLootOutputs(references, referenceRows, itemIds);
}

void AddBridgeCraftAndSpellLootOutputs(uint32 spellId,
                                       BridgeReferenceLootRows const& spellLootRows,
                                       BridgeReferenceLootRows const& referenceRows,
                                       std::set<uint32>& itemIds)
{
    std::set<uint32> visitedSpellIds;
    AddBridgeCraftOutputsFromSpell(spellId, itemIds, &visitedSpellIds);

    for (uint32 const visitedSpellId : visitedSpellIds)
        AddBridgeLootEntryOutputs(visitedSpellId, spellLootRows, referenceRows, itemIds);
}

void AddBridgeLootTableOutputs(char const* tableName,
                               BridgeReferenceLootRows const& referenceRows,
                               std::set<uint32>& itemIds)
{
    QueryResult result = WorldDatabase.Query("SELECT Item, Reference FROM {}", tableName);
    if (!result)
        return;

    std::set<uint32> references;
    do
    {
        Field* const fields = result->Fetch();
        uint32 const itemId = fields[0].Get<uint32>();
        uint32 const referenceId = fields[1].Get<uint32>();

        if (itemId)
            itemIds.insert(itemId);

        if (referenceId)
            references.insert(referenceId);
    } while (result->NextRow());

    AddBridgeReferenceLootOutputs(references, referenceRows, itemIds);
}
std::map<uint32, std::set<uint32>> const& GetBridgeCraftOutputsBySkill()
{
    // The source data is process-wide and immutable for normal runtime use.
    // Function-local static initialization is thread-safe, so the expensive
    // SkillLineAbility, item-template, and loot-table scans run only once.
    static std::map<uint32, std::set<uint32>> const outputsBySkill = []()
    {
        std::map<uint32, std::set<uint32>> outputs;

        BridgeReferenceLootRows const referenceLootRows = BuildBridgeReferenceLootRows();
        BridgeReferenceLootRows const spellLootRows =
            BuildBridgeLootTableRows("spell_loot_template");

        for (SkillType const skill : GetBridgeCraftProtectionSkills())
        {
            uint32 const skillId = static_cast<uint32>(skill);
            std::set<uint32>& skillOutputs = outputs[skillId];

            for (SkillLineAbilityEntry const* const ability : GetSkillLineAbilitiesBySkillLine(skillId))
            {
                if (ability)
                    AddBridgeCraftAndSpellLootOutputs(
                        ability->Spell, spellLootRows, referenceLootRows, skillOutputs);
            }
        }

        // Conservative recipe fallback audited from Playerbots. Scan the item
        // template store once, then attach each recognized recipe output to the
        // corresponding profession set. Random spell-loot outputs and every
        // reachable TriggerSpell are covered by the same helper.
        std::vector<ItemTemplate*> const* const itemTemplates = sObjectMgr->GetItemTemplateStoreFast();
        if (itemTemplates)
        {
            for (ItemTemplate const* const recipe : *itemTemplates)
            {
                uint32 const skillId = GetBridgeRecipeSkillId(recipe);
                auto const outputIt = outputs.find(skillId);
                if (!skillId || outputIt == outputs.end())
                    continue;

                for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                {
                    uint32 const spellId = recipe->Spells[i].SpellId;
                    if (spellId)
                        AddBridgeCraftAndSpellLootOutputs(
                            spellId, spellLootRows, referenceLootRows, outputIt->second);
                }
            }
        }

        // Prospecting, milling, and disenchanting use dedicated loot templates.
        // Include every possible output and follow reference_loot_template
        // recursively in the same process-wide cache used by SELL_VENDOR.
        AddBridgeLootTableOutputs(
            "prospecting_loot_template", referenceLootRows,
            outputs[static_cast<uint32>(SKILL_JEWELCRAFTING)]);
        AddBridgeLootTableOutputs(
            "milling_loot_template", referenceLootRows,
            outputs[static_cast<uint32>(SKILL_INSCRIPTION)]);
        AddBridgeLootTableOutputs(
            "disenchant_loot_template", referenceLootRows,
            outputs[static_cast<uint32>(SKILL_ENCHANTING)]);

        return outputs;
    }();

    return outputsBySkill;
}

std::set<uint32> BuildBridgeCraftProtectedItemIds(PlayerbotAI* botAI)
{
    std::set<uint32> itemIds;
    if (!botAI)
        return itemIds;

    std::map<uint32, std::set<uint32>> const& outputsBySkill = GetBridgeCraftOutputsBySkill();

    for (SkillType const skill : GetBridgeCraftProtectionSkills())
    {
        if (!botAI->HasSkill(skill))
            continue;

        auto const outputIt = outputsBySkill.find(static_cast<uint32>(skill));
        if (outputIt != outputsBySkill.end())
            itemIds.insert(outputIt->second.begin(), outputIt->second.end());
    }

    return itemIds;
}

bool IsBridgeSellVendorCandidate(Player* bot, PlayerbotAI* botAI, Item* item,
                                 std::set<uint32> const& protectedCraftItemIds)
{
    if (!bot || !botAI || !item)
        return false;

    ItemTemplate const* const proto = item->GetTemplate();
    if (!proto || !proto->SellPrice)
        return false;

    if (proto->Class == ITEM_CLASS_QUEST || proto->Class == ITEM_CLASS_KEY)
        return false;

    if (item->GetEntry() == 6948)
        return false;

    // Fail-safe workaround for the audited Playerbots ammo-class condition bug.
    if (proto->Class == ITEM_CLASS_PROJECTILE)
        return false;

    // Exact signed-craft protection.
    if (item->GetGuidValue(ITEM_FIELD_CREATOR) == bot->GetGUID())
        return false;

    // Conservative protection for unsigned/stackable profession outputs.
    if (protectedCraftItemIds.find(item->GetEntry()) != protectedCraftItemIds.end())
        return false;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    if (!context)
        return false;

    ItemUsage const usage = context->GetValue<ItemUsage>("item usage", item->GetEntry())->Get();
    return usage == ITEM_USAGE_VENDOR;
}

void CollectBridgeSellVendorCandidate(Player* bot, PlayerbotAI* botAI, Item* item,
                                      std::set<uint32> const& protectedCraftItemIds,
                                      std::vector<ObjectGuid>& itemGuids)
{
    if (IsBridgeSellVendorCandidate(bot, botAI, item, protectedCraftItemIds))
        itemGuids.push_back(item->GetGUID());
}

uint32 SellVendorBagItems(Player* bot, std::string& reason)
{
    if (!bot || !bot->GetSession())
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    Creature* const vendor = FindNearbyInteractiveVendor(bot);
    if (!vendor)
    {
        reason = "VENDOR_NOT_FOUND";
        return 0;
    }

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetAiObjectContext())
    {
        reason = "FAILED";
        return 0;
    }

    std::set<uint32> const protectedCraftItemIds = BuildBridgeCraftProtectedItemIds(botAI);
    std::vector<ObjectGuid> itemGuids;

    // Snapshot GUIDs before native selling. Uncertainty means keep.
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        CollectBridgeSellVendorCandidate(
            bot, botAI, bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), protectedCraftItemIds, itemGuids);

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* const pBag = static_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag));
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            CollectBridgeSellVendorCandidate(
                bot, botAI, pBag->GetItemByPos(slot), protectedCraftItemIds, itemGuids);
    }

    if (itemGuids.empty())
    {
        reason = "ITEM_NOT_FOUND";
        return 0;
    }

    uint32 sold = 0;

    for (ObjectGuid const itemGuid : itemGuids)
    {
        Item* const item = bot->GetItemByGuid(itemGuid);
        if (!IsBridgeSellVendorCandidate(bot, botAI, item, protectedCraftItemIds))
            continue;

        uint32 const itemId = item->GetEntry();
        uint32 const before = bot->GetItemCount(itemId, false);
        uint32 const moneyBefore = bot->GetMoney();

        WorldPacket packet(CMSG_SELL_ITEM);
        packet << vendor->GetGUID() << itemGuid << uint32(0);

        WorldPackets::Item::SellItem sellPacket(std::move(packet));
        sellPacket.Read();
        bot->GetSession()->HandleSellItemOpcode(sellPacket);

        if (botAI->HasCheat(BotCheatMask::gold))
            bot->SetMoney(moneyBefore);

        uint32 const after = bot->GetItemCount(itemId, false);
        if (before > after)
            sold += before - after;
    }

    if (!sold && reason.empty())
        reason = "FAILED";

    return sold;
}

uint32 BuyMatchingVendorItem(Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    ItemTemplate const* const proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
    {
        reason = "ITEM_NOT_FOUND";
        return 0;
    }

    uint32 vendorSlot = 0;
    uint32 vendorExtendedCost = 0;
    bool sawVendor = false;
    Creature* const vendor = FindNearbyVendorSellingItem(bot, itemId, vendorSlot, vendorExtendedCost, sawVendor);
    if (!vendor)
    {
        reason = sawVendor ? "VENDOR_DOES_NOT_SELL_ITEM" : "VENDOR_NOT_FOUND";
        return 0;
    }

    uint32 const desired = requestedCount > 0 ? requestedCount : 1;
    uint32 bought = 0;
    for (uint32 i = 0; i < desired; ++i)
    {
        uint32 const price = uint32(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
        if (price > 0 && bot->GetMoney() < price)
        {
            reason = "NOT_ENOUGH_MONEY";
            break;
        }

        uint32 const oldCount = bot->GetItemCount(itemId, false);
        bot->BuyItemFromVendorSlot(vendor->GetGUID(), vendorSlot, itemId, 1, NULL_BAG, NULL_SLOT);
        uint32 const newCount = bot->GetItemCount(itemId, false);
        if (newCount <= oldCount)
        {
            reason = vendorExtendedCost > 0 ? "VENDOR_REQUIRES_SPECIAL_CURRENCY" : "BUY_FAILED";
            break;
        }

        bought += newCount - oldCount;
    }

    return bought;
}

struct ItemActionRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, ItemActionRateState> sItemActionRateStates;

bool ConsumeItemActionRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    ItemActionRateState& state = sItemActionRateStates[key];

    while (!state.requests.empty() && now - state.requests.front() >= kItemActionRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kItemActionRateLimit)
        return false;

    state.requests.push_back(now);

    if (sItemActionRateStates.size() > 512)
    {
        for (auto it = sItemActionRateStates.begin(); it != sItemActionRateStates.end();)
        {
            while (!it->second.requests.empty() && now - it->second.requests.front() >= kItemActionRateWindow)
                it->second.requests.pop_front();

            if (it->second.requests.empty() && it->first != key)
                it = sItemActionRateStates.erase(it);
            else
                ++it;
        }
    }

    return true;
}

// MB_TALENT_APPLY_V1_BEGIN
struct TalentApplyRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, TalentApplyRateState> sTalentApplyRateStates;

bool ConsumeTalentApplyRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sTalentApplyRateStates.find(key);

    if (stateIt == sTalentApplyRateStates.end())
    {
        if (sTalentApplyRateStates.size() >= kTalentApplyMaxRequesterStates)
        {
            for (auto it = sTalentApplyRateStates.begin(); it != sTalentApplyRateStates.end();)
            {
                while (!it->second.requests.empty() && now - it->second.requests.front() >= kTalentApplyRateWindow)
                    it->second.requests.pop_front();
                while (!it->second.recentTokens.empty() && now - it->second.recentTokens.front().second >= kTalentApplyReplayTtl)
                    it->second.recentTokens.pop_front();

                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sTalentApplyRateStates.erase(it);
                else
                    ++it;
            }
        }

        if (sTalentApplyRateStates.size() >= kTalentApplyMaxRequesterStates)
            return false;

        stateIt = sTalentApplyRateStates.emplace(key, TalentApplyRateState{}).first;
    }

    TalentApplyRateState& state = stateIt->second;
    while (!state.requests.empty() && now - state.requests.front() >= kTalentApplyRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kTalentApplyRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterTalentApplyToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sTalentApplyRateStates.find(key);
    if (stateIt == sTalentApplyRateStates.end())
        return false;

    TalentApplyRateState& state = stateIt->second;
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kTalentApplyReplayTtl)
        state.recentTokens.pop_front();

    for (auto const& entry : state.recentTokens)
        if (entry.first == token)
            return false;

    state.recentTokens.push_back({token, now});
    while (state.recentTokens.size() > kTalentApplyMaxRecentTokens)
        state.recentTokens.pop_front();

    return true;
}

bool ValidateTalentApplyBuild(
    Player* bot,
    std::string const& build,
    std::vector<std::vector<uint32>>& parsed,
    std::array<uint32, 3>& expectedTabs)
{
    parsed.clear();
    expectedTabs = {0, 0, 0};

    if (!bot || build.empty() || build.size() > kMaxTalentApplyBuildLength)
        return false;

    std::array<std::string, 3> sections;
    std::size_t const firstDash = build.find('-');
    if (firstDash == std::string::npos)
        return false;
    std::size_t const secondDash = build.find('-', firstDash + 1);
    if (secondDash == std::string::npos || build.find('-', secondDash + 1) != std::string::npos)
        return false;

    sections[0] = build.substr(0, firstDash);
    sections[1] = build.substr(firstDash + 1, secondDash - firstDash - 1);
    sections[2] = build.substr(secondDash + 1);
    if (sections[0].empty() || sections[1].empty() || sections[2].empty())
        return false;

    std::array<std::vector<TalentEntry const*>, 3> classTalents;
    uint32 const classMask = bot->getClassMask();

    for (uint32 index = 0; index < sTalentStore.GetNumRows(); ++index)
    {
        TalentEntry const* const talent = sTalentStore.LookupEntry(index);
        if (!talent)
            continue;

        TalentTabEntry const* const tab = sTalentTabStore.LookupEntry(talent->TalentTab);
        if (!tab || tab->tabpage >= classTalents.size() || !(tab->ClassMask & classMask))
            continue;

        classTalents[tab->tabpage].push_back(talent);
    }

    std::map<uint32, uint32> requestedRanks;
    uint32 requestedPoints = 0;
    for (std::size_t tabIndex = 0; tabIndex < classTalents.size(); ++tabIndex)
    {
        std::sort(classTalents[tabIndex].begin(), classTalents[tabIndex].end(),
            [](TalentEntry const* left, TalentEntry const* right)
            {
                return left->Row != right->Row ? left->Row < right->Row : left->Col < right->Col;
            });

        if (sections[tabIndex].size() != classTalents[tabIndex].size())
            return false;

        for (std::size_t talentIndex = 0; talentIndex < sections[tabIndex].size(); ++talentIndex)
        {
            char const value = sections[tabIndex][talentIndex];
            if (value < '0' || value > '5')
                return false;

            uint32 const requestedRank = static_cast<uint32>(value - '0');
            TalentEntry const* const talent = classTalents[tabIndex][talentIndex];

            uint32 maxRank = 0;
            for (uint32 spellId : talent->RankID)
                if (spellId)
                    ++maxRank;

            if (requestedRank > maxRank)
                return false;

            requestedRanks[talent->TalentID] = requestedRank;
            expectedTabs[tabIndex] += requestedRank;
            requestedPoints += requestedRank;
        }
    }

    for (std::size_t tabIndex = 0; tabIndex < classTalents.size(); ++tabIndex)
    {
        uint32 pointsInPriorRows = 0;
        std::size_t talentIndex = 0;
        while (talentIndex < classTalents[tabIndex].size())
        {
            uint32 const row = classTalents[tabIndex][talentIndex]->Row;
            uint32 pointsInRow = 0;

            while (talentIndex < classTalents[tabIndex].size() &&
                   classTalents[tabIndex][talentIndex]->Row == row)
            {
                TalentEntry const* const talent = classTalents[tabIndex][talentIndex];
                uint32 const requestedRank =
                    static_cast<uint32>(sections[tabIndex][talentIndex] - '0');

                if (requestedRank)
                {
                    if (pointsInPriorRows < talent->Row * MAX_TALENT_RANK)
                        return false;

                    if (talent->DependsOn)
                    {
                        auto const dependency = requestedRanks.find(talent->DependsOn);
                        if (dependency == requestedRanks.end() ||
                            dependency->second <= talent->DependsOnRank)
                            return false;
                    }
                }

                pointsInRow += requestedRank;
                ++talentIndex;
            }

            pointsInPriorRows += pointsInRow;
        }
    }

    if (!requestedPoints || requestedPoints > bot->CalculateTalentsPoints())
        return false;

    parsed = PlayerbotAIConfig::ParseTempTalentsOrder(bot->getClass(), build);
    if (parsed.empty())
        return false;

    uint32 parsedPoints = 0;
    for (std::vector<uint32> const& entry : parsed)
    {
        if (entry.size() != 4 || entry[0] >= expectedTabs.size() || !entry[3] || entry[3] > MAX_TALENT_RANK)
            return false;
        parsedPoints += entry[3];
    }

    return parsedPoints == requestedPoints;
}

void RunTalentApplyCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    std::string const& build)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    PlayerbotAI* const botAI = bot ? GetBotAI(bot) : nullptr;

    std::string reason = "OK";
    std::array<uint32, 3> expectedTabs = {0, 0, 0};
    std::array<uint32, 3> actualTabs = {0, 0, 0};
    std::vector<std::vector<uint32>> parsed;

    if (!requester || !requester->GetSession())
        reason = "BAD_REQUEST";
    else if (!ConsumeTalentApplyRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!RegisterTalentApplyToken(requester, token))
        reason = "DUPLICATE";
    else if (!bot)
        reason = "NO_BOT";
    else if (!bot->GetSession() || !bot->IsInWorld())
        reason = "BOT_UNAVAILABLE";
    else if (!botAI)
        reason = "NO_AI";
    else if (!botAI->GetSecurity() ||
             !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
        reason = "FORBIDDEN";
    else if (!ValidateTalentApplyBuild(bot, build, parsed, expectedTabs))
        reason = "INVALID_BUILD";
    else
    {
        PlayerbotFactory::InitTalentsByParsedSpecLink(bot, parsed, true);
        actualTabs = BuildTalentTabPoints(bot);

        if (actualTabs != expectedTabs)
            reason = "VERIFY_FAILED";
        else
            botAI->ResetStrategies();
    }

    std::string const status = reason == "OK" ? "OK" : "ERR";
    std::ostringstream payload;
    payload << token
        << kFieldSeparator << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << status
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << actualTabs[0]
        << kFieldSeparator << actualTabs[1]
        << kFieldSeparator << actualTabs[2];

    SendAddonPacket(requester, replyType, "TALENT_APPLY_RESULT", payload.str());
}
// MB_TALENT_APPLY_V1_END
// MB_TALENT_SPEC_APPLY_V1_BEGIN
struct TalentSpecApplyRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, TalentSpecApplyRateState> sTalentSpecApplyRateStates;

bool ConsumeTalentSpecApplyRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sTalentSpecApplyRateStates.find(key);

    if (stateIt == sTalentSpecApplyRateStates.end())
    {
        if (sTalentSpecApplyRateStates.size() >= kTalentSpecApplyMaxRequesterStates)
        {
            for (auto it = sTalentSpecApplyRateStates.begin(); it != sTalentSpecApplyRateStates.end();)
            {
                while (!it->second.requests.empty() && now - it->second.requests.front() >= kTalentSpecApplyRateWindow)
                    it->second.requests.pop_front();
                while (!it->second.recentTokens.empty() && now - it->second.recentTokens.front().second >= kTalentSpecApplyReplayTtl)
                    it->second.recentTokens.pop_front();

                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sTalentSpecApplyRateStates.erase(it);
                else
                    ++it;
            }
        }

        if (sTalentSpecApplyRateStates.size() >= kTalentSpecApplyMaxRequesterStates)
            return false;

        stateIt = sTalentSpecApplyRateStates.emplace(key, TalentSpecApplyRateState{}).first;
    }

    TalentSpecApplyRateState& state = stateIt->second;
    while (!state.requests.empty() && now - state.requests.front() >= kTalentSpecApplyRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kTalentSpecApplyRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterTalentSpecApplyToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sTalentSpecApplyRateStates.find(key);
    if (stateIt == sTalentSpecApplyRateStates.end())
        return false;

    TalentSpecApplyRateState& state = stateIt->second;
    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kTalentSpecApplyReplayTtl)
        state.recentTokens.pop_front();

    for (auto const& entry : state.recentTokens)
        if (entry.first == token)
            return false;

    state.recentTokens.push_back({token, now});
    while (state.recentTokens.size() > kTalentSpecApplyMaxRecentTokens)
        state.recentTokens.pop_front();

    return true;
}

bool TryParseTalentSpecPointSummary(std::string const& summary, std::array<uint32, 3>& tabs)
{
    tabs = {0, 0, 0};

    std::size_t const firstDash = summary.find('-');
    if (firstDash == std::string::npos)
        return false;
    std::size_t const secondDash = summary.find('-', firstDash + 1);
    if (secondDash == std::string::npos || summary.find('-', secondDash + 1) != std::string::npos)
        return false;

    std::array<std::string, 3> const fields =
    {
        summary.substr(0, firstDash),
        summary.substr(firstDash + 1, secondDash - firstDash - 1),
        summary.substr(secondDash + 1)
    };

    for (std::size_t index = 0; index < fields.size(); ++index)
        if (!TryParseUint32Field(fields[index], 0, 255, tabs[index]))
            return false;

    return true;
}

bool FindTalentSpecEntry(Player* bot, uint32 specIndex, TalentSpecEntryData& selected)
{
    if (!bot || specIndex > 30)
        return false;

    std::vector<TalentSpecEntryData> const specs = BuildTalentSpecEntries(bot);
    for (TalentSpecEntryData const& entry : specs)
    {
        if (entry.index == specIndex)
        {
            selected = entry;
            return true;
        }
    }

    return false;
}

void RunTalentSpecApplyCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint32 slot,
    uint32 specIndex)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason = "OK";
    std::array<uint32, 3> expectedTabs = {0, 0, 0};
    std::array<uint32, 3> actualTabs = {0, 0, 0};
    TalentSpecEntryData selected;

    PlayerbotAI* const botAI = bot ? GetBotAI(bot) : nullptr;

    if (!requester || !requester->GetSession())
        reason = "BAD_REQUEST";
    else if (!ConsumeTalentSpecApplyRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!RegisterTalentSpecApplyToken(requester, token))
        reason = "DUPLICATE";
    else if (!bot)
        reason = "NO_BOT";
    else if (!bot->GetSession() || !bot->IsInWorld())
        reason = "BOT_UNAVAILABLE";
    else if (!botAI)
        reason = "NO_AI";
    else if (!botAI->GetSecurity() ||
             !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
        reason = "FORBIDDEN";
    else if (slot < 1 || slot > 2)
        reason = "BAD_SLOT";
    else if (!FindTalentSpecEntry(bot, specIndex, selected))
        reason = "SPEC_NOT_FOUND";
    else if (selected.build.empty() || !TryParseTalentSpecPointSummary(selected.build, expectedTabs))
        reason = "SPEC_BUILD_UNAVAILABLE";
    else
    {
        uint8 const requestedSpec = static_cast<uint8>(slot - 1);

        if (slot == 2 && bot->GetSpecsCount() < 2)
        {
            if (bot->GetLevel() < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))
                reason = "DUAL_SPEC_LEVEL";
            else
            {
                bot->CastSpell(bot, 63680, true, nullptr, nullptr, bot->GetGUID());
                bot->CastSpell(bot, 63624, true, nullptr, nullptr, bot->GetGUID());
                if (bot->GetSpecsCount() < 2)
                    reason = "DUAL_SPEC_FAILED";
            }
        }

        if (reason == "OK")
        {
            if (bot->IsNonMeleeSpellCast(false))
                bot->InterruptNonMeleeSpells(false);

            bot->ActivateSpec(requestedSpec);
            if (bot->GetActiveSpec() != requestedSpec)
                reason = "SWITCH_FAILED";
        }

        if (reason == "OK")
        {
            auto* const customGlyphs = botAI->GetAiObjectContext()->GetValue<bool>("custom_glyphs");
            if (customGlyphs && customGlyphs->Get())
                customGlyphs->Set(false);

            PlayerbotFactory::InitTalentsBySpecNo(bot, static_cast<int>(specIndex), true);

            PlayerbotFactory factory(bot, bot->GetLevel());
            factory.InitGlyphs(false);

            actualTabs = BuildTalentTabPoints(bot);
            if (actualTabs != expectedTabs)
                reason = "VERIFY_FAILED";
            else
                botAI->ResetStrategies();
        }
    }

    if (bot && actualTabs == std::array<uint32, 3>{0, 0, 0})
        actualTabs = BuildTalentTabPoints(bot);

    std::string const status = reason == "OK" ? "OK" : "ERR";
    std::ostringstream payload;
    payload << token
        << kFieldSeparator << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << status
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << slot
        << kFieldSeparator << specIndex
        << kFieldSeparator << actualTabs[0]
        << kFieldSeparator << actualTabs[1]
        << kFieldSeparator << actualTabs[2];

    SendAddonPacket(requester, replyType, "TALENT_SPEC_APPLY_RESULT", payload.str());
}
// MB_TALENT_SPEC_APPLY_V1_END// MB_QUEST_ABANDON_V1_BEGIN

struct QuestAbandonRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> recentTokens;
};

std::map<std::string, QuestAbandonRateState> sQuestAbandonRateStates;

bool ConsumeQuestAbandonRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sQuestAbandonRateStates.find(key);

    if (stateIt == sQuestAbandonRateStates.end())
    {
        if (sQuestAbandonRateStates.size() >= kQuestAbandonMaxRequesterStates)
        {
            for (auto it = sQuestAbandonRateStates.begin(); it != sQuestAbandonRateStates.end();)
            {
                while (!it->second.requests.empty() && now - it->second.requests.front() >= kQuestAbandonRateWindow)
                    it->second.requests.pop_front();
                while (!it->second.recentTokens.empty() && now - it->second.recentTokens.front().second >= kQuestAbandonReplayTtl)
                    it->second.recentTokens.pop_front();

                if (it->second.requests.empty() && it->second.recentTokens.empty())
                    it = sQuestAbandonRateStates.erase(it);
                else
                    ++it;
            }
        }

        if (sQuestAbandonRateStates.size() >= kQuestAbandonMaxRequesterStates)
            return false;
    }

    QuestAbandonRateState& state = sQuestAbandonRateStates[key];

    while (!state.requests.empty() && now - state.requests.front() >= kQuestAbandonRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kQuestAbandonRateLimit)
        return false;

    state.requests.push_back(now);
    return true;
}

bool RegisterQuestAbandonToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    auto stateIt = sQuestAbandonRateStates.find(key);
    if (stateIt == sQuestAbandonRateStates.end())
        return false;

    QuestAbandonRateState& state = stateIt->second;

    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kQuestAbandonReplayTtl)
        state.recentTokens.pop_front();

    for (auto const& entry : state.recentTokens)
        if (entry.first == token)
            return false;

    state.recentTokens.push_back({token, now});
    while (state.recentTokens.size() > kQuestAbandonMaxRecentTokens)
        state.recentTokens.pop_front();

    return true;
}

void RunQuestAbandonCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    uint32 questId)
{
    std::string const token = Trim(requestToken);
    std::string reason = "OK";
    uint32 matched = 0;
    uint32 foundQuest = 0;
    uint32 abandoned = 0;

    if (!requester || !requester->GetSession() || !questId)
        reason = "BAD_REQUEST";
    else if (!ConsumeQuestAbandonRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!RegisterQuestAbandonToken(requester, token))
        reason = "DUPLICATE";
    else
    {
        Group* const group = requester->GetGroup();
        if (!group)
            reason = "NO_GROUP";
        else
        {
            uint32 authorized = 0;
            for (Player* const bot : GetBridgeVisibleBots(requester))
            {
                if (!bot || bot->GetGroup() != group)
                    continue;

                ++matched;
                PlayerbotAI* const botAI = GetBotAI(bot);
                if (!botAI || !botAI->GetSecurity() ||
                    !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
                    continue;

                ++authorized;
                if (!bot->GetSession() || !bot->IsInWorld())
                    continue;

                uint8 questSlot = MAX_QUEST_LOG_SIZE;
                for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
                {
                    if (bot->GetQuestSlotQuestId(slot) == questId)
                    {
                        questSlot = slot;
                        break;
                    }
                }

                if (questSlot >= MAX_QUEST_LOG_SIZE)
                    continue;

                ++foundQuest;
                WorldPacket packet(CMSG_QUESTLOG_REMOVE_QUEST, 1);
                packet << questSlot;

                WorldPackets::Quest::QuestLogRemoveQuest removeQuestPacket(std::move(packet));
                removeQuestPacket.Read();
                bot->GetSession()->HandleQuestLogRemoveQuest(removeQuestPacket);

                if (bot->GetQuestSlotQuestId(questSlot) != questId)
                    ++abandoned;
            }

            if (!matched)
                reason = "NO_BOTS";
            else if (!authorized)
                reason = "FORBIDDEN";
            else if (!foundQuest)
                reason = "NO_QUEST";
            else if (abandoned != foundQuest)
                reason = "FAILED";
        }
    }

    std::string const status = reason == "OK" ? "OK" : "ERR";
    std::ostringstream payload;
    payload << token
        << kFieldSeparator << questId
        << kFieldSeparator << status
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << matched
        << kFieldSeparator << abandoned;

    SendAddonPacket(requester, replyType, "QUEST_ABANDON_RESULT", payload.str());
}
// MB_QUEST_ABANDON_V1_END
struct GroupRollRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, GroupRollRateState> sGroupRollRateStates;

bool ConsumeGroupRollRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    GroupRollRateState& state = sGroupRollRateStates[key];

    while (!state.requests.empty() && now - state.requests.front() >= kGroupRollRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kGroupRollRateLimit)
        return false;

    state.requests.push_back(now);

    if (sGroupRollRateStates.size() > 512)
    {
        for (auto it = sGroupRollRateStates.begin(); it != sGroupRollRateStates.end();)
        {
            while (!it->second.requests.empty() && now - it->second.requests.front() >= kGroupRollRateWindow)
                it->second.requests.pop_front();

            if (it->second.requests.empty() && it->first != key)
                it = sGroupRollRateStates.erase(it);
            else
                ++it;
        }
    }

    return true;
}

struct EnchantTradeRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, EnchantTradeRateState> sEnchantTradeRateStates;

bool ConsumeEnchantTradeRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    EnchantTradeRateState& state = sEnchantTradeRateStates[key];

    while (!state.requests.empty() && now - state.requests.front() >= kEnchantTradeRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kEnchantTradeRateLimit)
        return false;

    state.requests.push_back(now);

    if (sEnchantTradeRateStates.size() > 512)
    {
        for (auto it = sEnchantTradeRateStates.begin(); it != sEnchantTradeRateStates.end();)
        {
            while (!it->second.requests.empty() && now - it->second.requests.front() >= kEnchantTradeRateWindow)
                it->second.requests.pop_front();

            if (it->second.requests.empty() && it->first != key)
                it = sEnchantTradeRateStates.erase(it);
            else
                ++it;
        }
    }

    return true;
}

void RunGroupRollCommand(Player* requester, ChatMsg replyType, std::string const& requestToken, std::string const& modeValue, std::string const& encodedItemLink)
{
    std::string const token = Trim(requestToken);
    std::string const mode = ToUpper(Trim(modeValue));
    std::string itemLink;
    std::string scope = "NONE";
    std::string reason = "OK";
    uint32 matched = 0;
    uint32 invoked = 0;

    if (!requester || !IsValidRequestToken(token) || (mode != "NORMAL" && mode != "ITEM"))
    {
        reason = "BAD_REQUEST";
    }
    else if (mode == "ITEM" &&
             (!TryUrlDecodeField(encodedItemLink, itemLink, kMaxGroupRollItemLinkLength, false) ||
              itemLink.find("|Hitem:") == std::string::npos))
    {
        reason = "BAD_ITEM";
    }
    else if (mode == "NORMAL" && !encodedItemLink.empty())
    {
        reason = "BAD_REQUEST";
    }
    else if (!ConsumeGroupRollRateLimit(requester))
    {
        reason = "RATE_LIMIT";
    }
    else
    {
        Group* const group = requester->GetGroup();
        if (!group)
        {
            reason = "NO_GROUP";
        }
        else
        {
            scope = group->isRaidGroup() ? "RAID" : "PARTY";
            for (Player* const bot : GetBridgeVisibleBots(requester))
            {
                if (!bot || bot->GetGroup() != group)
                    continue;

                ++matched;
                PlayerbotAI* const botAI = GetBotAI(bot);
                if (!botAI || !botAI->GetSecurity() ||
                    !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
                {
                    continue;
                }

                botAI->DoSpecificAction("roll", Event("roll", itemLink, requester), true);
                ++invoked;
            }

            if (!matched)
                reason = "NO_BOTS";
            else if (!invoked)
                reason = "FORBIDDEN";
        }
    }

    std::string const status = reason == "OK" ? "OK" : "ERR";
    std::ostringstream payload;
    payload << token
        << kFieldSeparator << status
        << kFieldSeparator << mode
        << kFieldSeparator << scope
        << kFieldSeparator << matched
        << kFieldSeparator << invoked
        << kFieldSeparator << UrlEncodeField(reason);

    SendAddonPacket(requester, replyType, "GROUP_ROLL_ACK", payload.str());
}

void RunInventoryItemMoveCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount,
    uint8 dstBag,
    uint8 dstSlot,
    uint32 dstItemId,
    uint32 dstCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    bool changed = false;

    if (!ConsumeInventoryItemMoveRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemMoveToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!IsInventoryItemMovePositionAllowed(bot, srcBag, srcSlot) ||
            !IsInventoryItemMovePositionAllowed(bot, dstBag, dstSlot))
            reason = "BAD_POSITION";
        else if (srcBag == dstBag && srcSlot == dstSlot)
            reason = "SAME_POSITION";
        else
        {
            InventoryItemMovePositionState const beforeSource = ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);
            InventoryItemMovePositionState const beforeDestination = ReadInventoryItemMovePositionState(bot, dstBag, dstSlot);

            if (!InventoryItemMoveStateMatchesExpected(beforeSource, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else if (!InventoryItemMoveStateMatchesExpected(beforeDestination, dstItemId, dstCount))
                reason = "DEST_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                Item* const destinationItem = bot->GetItemByPos(dstBag, dstSlot);
                bool mergeExpected = false;
                uint32 mergedCount = 0;

                if (!sourceItem)
                    reason = "SOURCE_STALE";
                else
                {
                    if (destinationItem && !sourceItem->IsBag() && !destinationItem->IsBag())
                    {
                        ItemPosCountVec mergeDestination;
                        InventoryResult const mergeResult =
                            bot->CanStoreItem(dstBag, dstSlot, mergeDestination, sourceItem, false);
                        if (mergeResult == EQUIP_ERR_OK)
                        {
                            uint64 const combinedCount = static_cast<uint64>(beforeSource.count) +
                                static_cast<uint64>(beforeDestination.count);
                            uint32 const maxStackCount = sourceItem->GetMaxStackCount();
                            if (combinedCount > static_cast<uint64>(maxStackCount))
                                reason = "PARTIAL_STACK_UNSUPPORTED";
                            else
                            {
                                mergeExpected = true;
                                mergedCount = static_cast<uint32>(combinedCount);
                            }
                        }
                    }

                    if (reason.empty())
                    {
                        uint16 const sourcePosition = (static_cast<uint16>(srcBag) << 8) | srcSlot;
                        uint16 const destinationPosition = (static_cast<uint16>(dstBag) << 8) | dstSlot;
                        bot->SwapItem(sourcePosition, destinationPosition);

                        InventoryItemMovePositionState const afterSource =
                            ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);
                        InventoryItemMovePositionState const afterDestination =
                            ReadInventoryItemMovePositionState(bot, dstBag, dstSlot);

                        bool postconditionSatisfied = false;
                        if (!beforeDestination.present)
                        {
                            postconditionSatisfied =
                                InventoryItemMoveStateMatchesExpected(afterSource, 0, 0) &&
                                InventoryItemMoveStatesEqual(afterDestination, beforeSource);
                        }
                        else if (mergeExpected)
                        {
                            postconditionSatisfied =
                                InventoryItemMoveStateMatchesExpected(afterSource, 0, 0) &&
                                InventoryItemMoveStateMatchesExpected(
                                    afterDestination, beforeSource.itemId, mergedCount);
                        }
                        else
                        {
                            postconditionSatisfied =
                                InventoryItemMoveStatesEqual(afterSource, beforeDestination) &&
                                InventoryItemMoveStatesEqual(afterDestination, beforeSource);
                        }

                        changed = postconditionSatisfied;
                        reason = changed ? "OK" : "POSTCONDITION_FAILED";
                    }
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (changed ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << static_cast<uint32>(dstBag)
        << kFieldSeparator << static_cast<uint32>(dstSlot);

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_MOVE", payload.str());
}

void RunInventoryItemDepositExactCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    std::string const& actionValue,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    std::string const action = ToUpper(Trim(actionValue));
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    uint32 moved = 0;

    if (!ConsumeInventoryItemDepositExactRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemDepositExactToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (action != "BANK_DEPOSIT" && action != "GBANK_DEPOSIT")
            reason = "BAD_ACTION";
        else if (!IsInventoryItemEquipSourcePositionAllowed(bot, srcBag, srcSlot))
            reason = "BAD_POSITION";
        else
        {
            InventoryItemMovePositionState const sourceState =
                ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);

            if (!InventoryItemMoveStateMatchesExpected(sourceState, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                if (!sourceItem)
                    reason = "SOURCE_STALE";
                else if (action == "BANK_DEPOSIT")
                {
                    if (!FindNearbyNpcWithFlag(bot, UNIT_NPC_FLAG_BANKER))
                        reason = "BANKER_NOT_FOUND";
                    else
                    {
                        ItemPosCountVec dest;
                        InventoryResult const bankResult =
                            bot->CanBankItem(NULL_BAG, NULL_SLOT, dest, sourceItem, false);
                        if (bankResult != EQUIP_ERR_OK)
                            reason = "BANK_FULL";
                        else
                        {
                            ObjectGuid const sourceGuid = sourceItem->GetGUID();
                            bot->RemoveItem(srcBag, srcSlot, true);
                            bot->BankItem(dest, sourceItem, true);

                            Item* const remaining = bot->GetItemByPos(srcBag, srcSlot);
                            if (remaining && remaining->GetGUID() == sourceGuid)
                                reason = "POSTCONDITION_FAILED";
                            else
                                moved = srcCount;
                        }
                    }
                }
                else
                {
                    if (!bot->GetGuildId())
                        reason = "BOT_NOT_IN_GUILD";
                    else if (requester->GetGuildId() != bot->GetGuildId())
                        reason = "NOT_IN_SAME_GUILD";
                    else
                    {
                        Guild* const guild = sGuildMgr->GetGuildById(bot->GetGuildId());
                        if (!guild)
                            reason = "BOT_NOT_IN_GUILD";
                        else if (!FindNearbyGuildBank(bot))
                            reason = "GUILD_BANK_NOT_FOUND";
                        else
                        {
                            ObjectGuid const sourceGuid = sourceItem->GetGUID();
                            bool hasAuthorizedTab = false;
                            if (TryDepositGuildBankStack(
                                    guild, requester, bot, srcBag, srcSlot, sourceGuid, hasAuthorizedTab))
                                moved = srcCount;
                            else
                                reason = hasAuthorizedTab ? "GUILD_BANK_FULL" : "NO_GUILD_BANK_RIGHTS";
                        }
                    }
                }
            }
        }
    }

    bool const ok = moved == srcCount && moved > 0;
    if (ok)
        reason = "OK";
    else if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << action
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << srcItemId
        << kFieldSeparator << srcCount
        << kFieldSeparator << moved;

    SendAddonPacket(requester, replyType, "ITEM_DEPOSIT_EXACT", payload.str());
}

void RunInventoryItemTradeCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    bool added = false;
    uint8 tradeSlot = 255;

    if (!ConsumeInventoryItemTradeRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemTradeToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!IsInventoryItemMovePositionAllowed(bot, srcBag, srcSlot))
            reason = "BAD_POSITION";
        else
        {
            InventoryItemMovePositionState const sourceState =
                ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);

            if (!InventoryItemMoveStateMatchesExpected(sourceState, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                TradeData* const botTrade = bot->GetTradeData();
                TradeData* const requesterTrade = requester->GetTradeData();

                if (!sourceItem)
                    reason = "SOURCE_STALE";
                else if (!botTrade || !requesterTrade)
                    reason = "NO_TRADE";
                else if (botTrade->GetTrader() != requester || requesterTrade->GetTrader() != bot)
                    reason = "WRONG_TRADER";
                else if (botTrade->HasItem(sourceItem->GetGUID()))
                    reason = "ALREADY_IN_TRADE";
                else if (!sourceItem->CanBeTraded(false, true))
                    reason = "NOT_TRADABLE";
                else if (sourceItem->IsBindedNotWith(requester))
                    reason = "WRONG_BINDING";
                else
                {
                    for (uint8 candidate = 0; candidate < TRADE_SLOT_TRADED_COUNT; ++candidate)
                    {
                        if (!botTrade->GetItem(TradeSlots(candidate)))
                        {
                            tradeSlot = candidate;
                            break;
                        }
                    }

                    if (tradeSlot >= TRADE_SLOT_TRADED_COUNT)
                        reason = "TRADE_FULL";
                    else
                    {
                        ObjectGuid const sourceGuid = sourceItem->GetGUID();

                        WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
                        packet << tradeSlot << srcBag << srcSlot;
                        bot->GetSession()->HandleSetTradeItemOpcode(packet);

                        TradeData* const updatedBotTrade = bot->GetTradeData();
                        Item* const tradedItem = updatedBotTrade &&
                            updatedBotTrade->GetTrader() == requester ?
                            updatedBotTrade->GetItem(TradeSlots(tradeSlot)) : nullptr;

                        added = tradedItem && tradedItem->GetGUID() == sourceGuid;
                        reason = added ? "OK" : "TRADE_REJECTED";
                    }
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (added ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << srcItemId
        << kFieldSeparator << srcCount
        << kFieldSeparator << static_cast<uint32>(tradeSlot);

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_TRADE", payload.str());
}

void RunInventoryItemEquipCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    bool equipped = false;
    uint8 dstSlot = NULL_SLOT;

    if (!ConsumeInventoryItemEquipRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemEquipToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!IsInventoryItemEquipSourcePositionAllowed(bot, srcBag, srcSlot))
            reason = "BAD_POSITION";
        else
        {
            InventoryItemMovePositionState const sourceState = ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);
            if (!InventoryItemMoveStateMatchesExpected(sourceState, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                ItemTemplate const* const itemTemplate = sourceItem ? sourceItem->GetTemplate() : nullptr;
                if (!sourceItem || !itemTemplate)
                    reason = "SOURCE_STALE";
                else if (sourceItem->IsBag() || itemTemplate->InventoryType == INVTYPE_AMMO)
                    reason = "UNSUPPORTED_ITEM";
                else
                {
                    ObjectGuid const sourceGuid = sourceItem->GetGUID();

                    WorldPacket packet(CMSG_AUTOEQUIP_ITEM, 2);
                    packet << srcBag << srcSlot;

                    WorldPackets::Item::AutoEquipItem nicePacket(std::move(packet));
                    nicePacket.Read();
                    bot->GetSession()->HandleAutoEquipItemOpcode(nicePacket);

                    Item* const equippedItem = bot->GetItemByGuid(sourceGuid);
                    if (equippedItem &&
                        equippedItem->GetBagSlot() == INVENTORY_SLOT_BAG_0 &&
                        equippedItem->GetSlot() >= EQUIPMENT_SLOT_START &&
                        equippedItem->GetSlot() < EQUIPMENT_SLOT_END)
                    {
                        equipped = true;
                        dstSlot = equippedItem->GetSlot();
                        reason = "OK";
                    }
                    else
                        reason = "FAILED";
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (equipped ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << static_cast<uint32>(dstSlot);

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_EQUIP", payload.str());
}

void RunInventoryItemUnequipCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcSlot,
    uint32 srcItemId)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    bool unequipped = false;

    if (!ConsumeInventoryItemUnequipRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemUnequipToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (srcSlot >= EQUIPMENT_SLOT_END)
            reason = "BAD_POSITION";
        else
        {
            Item* const sourceItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, srcSlot);
            if (!sourceItem || sourceItem->GetEntry() != srcItemId)
                reason = "SOURCE_STALE";
            else
            {
                uint16 const sourcePos = sourceItem->GetPos();
                InventoryResult const unequipResult = bot->CanUnequipItem(sourcePos, true);
                if (unequipResult != EQUIP_ERR_OK)
                    reason = "UNEQUIP_DENIED";
                else
                {
                    ItemPosCountVec destination;
                    InventoryResult const storeResult = bot->CanStoreItem(NULL_BAG, NULL_SLOT, destination, sourceItem, false);
                    if (storeResult != EQUIP_ERR_OK)
                        reason = "NO_STORAGE";
                    else
                    {
                        ObjectGuid const sourceGuid = sourceItem->GetGUID();

                        WorldPacket packet(CMSG_AUTOSTORE_BAG_ITEM, 3);
                        packet << uint8(INVENTORY_SLOT_BAG_0) << srcSlot << uint8(NULL_BAG);

                        WorldPackets::Item::AutoStoreBagItem nicePacket(std::move(packet));
                        nicePacket.Read();
                        bot->GetSession()->HandleAutoStoreBagItemOpcode(nicePacket);

                        Item* const storedItem = bot->GetItemByGuid(sourceGuid);
                        if (storedItem &&
                            !bot->IsEquipmentPos(storedItem->GetPos()) &&
                            IsInventoryItemUnequipDestinationPositionAllowed(bot, storedItem->GetBagSlot(), storedItem->GetSlot()))
                        {
                            unequipped = true;
                            reason = "OK";
                        }
                        else
                            reason = "FAILED";
                    }
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (unequipped ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << srcItemId;

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_UNEQUIP", payload.str());
}

// MB_ITEM_DESTROY_V1_BEGIN
void RunInventoryItemDestroyCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    bool destroyed = false;

    if (!ConsumeInventoryItemDestroyRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemDestroyToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!IsInventoryItemMovePositionAllowed(bot, srcBag, srcSlot))
            reason = "BAD_POSITION";
        else
        {
            InventoryItemMovePositionState const sourceState =
                ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);

            if (!InventoryItemMoveStateMatchesExpected(sourceState, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                ItemTemplate const* const itemTemplate = sourceItem ? sourceItem->GetTemplate() : nullptr;
                if (!sourceItem || !itemTemplate)
                    reason = "SOURCE_STALE";
                else if (sourceItem->IsNotEmptyBag())
                    reason = "NONEMPTY_BAG";
                else if (itemTemplate->HasFlag(ITEM_FLAG_NO_USER_DESTROY))
                    reason = "NO_USER_DESTROY";
                else
                {
                    ObjectGuid const sourceGuid = sourceItem->GetGUID();

                    WorldPacket packet(CMSG_DESTROYITEM, 6);
                    packet << srcBag << srcSlot
                           << uint8(0) << uint8(0) << uint8(0) << uint8(0);

                    WorldPackets::Item::DestroyItem destroyPacket(std::move(packet));
                    destroyPacket.Read();
                    bot->GetSession()->HandleDestroyItemOpcode(destroyPacket);

                    destroyed = bot->GetItemByGuid(sourceGuid) == nullptr;
                    reason = destroyed ? "OK" : "FAILED";
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (destroyed ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << srcItemId;

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_DESTROY", payload.str());
}
// MB_ITEM_DESTROY_V1_END
// MB_ITEM_USE_V1_BEGIN
// Design inspired by the Jellypowered bridge contribution.
void RunInventoryItemUseCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    bool used = false;

    if (!ConsumeInventoryItemUseRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemUseToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!IsInventoryItemEquipSourcePositionAllowed(bot, srcBag, srcSlot))
            reason = "BAD_POSITION";
        else
        {
            InventoryItemMovePositionState const sourceState =
                ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);

            if (!InventoryItemMoveStateMatchesExpected(sourceState, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                ItemTemplate const* const itemTemplate = sourceItem ? sourceItem->GetTemplate() : nullptr;
                if (!sourceItem || !itemTemplate)
                    reason = "SOURCE_STALE";
                else if (sourceItem->IsBag())
                    reason = "UNSUPPORTED_ITEM";
                else
                {
                    InventoryResult const canUse = bot->CanUseItem(sourceItem);
                    if (canUse != EQUIP_ERR_OK)
                        reason = "CANNOT_USE";
                    else if (bot->IsNonMeleeSpellCast(false))
                        reason = "CAST_BUSY";
                    else if (itemTemplate->Class == ITEM_CLASS_GEM)
                        reason = "TARGET_REQUIRED";
                    else if (itemTemplate->StartQuest && sObjectMgr->GetQuestTemplate(itemTemplate->StartQuest))
                    {
                        uint32 const questId = itemTemplate->StartQuest;
                        QuestStatus const beforeStatus = bot->GetQuestStatus(questId);

                        WorldPacket packet(CMSG_QUESTGIVER_ACCEPT_QUEST, 8 + 4 + 4);
                        packet << sourceItem->GetGUID();
                        packet << questId;
                        packet << uint32(0);
                        bot->GetSession()->HandleQuestgiverAcceptQuestOpcode(packet);

                        QuestStatus const afterStatus = bot->GetQuestStatus(questId);
                        used = beforeStatus == QUEST_STATUS_NONE && afterStatus != QUEST_STATUS_NONE;
                        reason = used ? "OK" : "QUEST_NOT_ACCEPTED";
                    }
                    else
                    {
                        uint32 spellId = 0;
                        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                        {
                            if (itemTemplate->Spells[i].SpellId > 0 &&
                                itemTemplate->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                            {
                                spellId = itemTemplate->Spells[i].SpellId;
                                break;
                            }
                        }

                        if (!spellId)
                            reason = "NOT_USABLE";
                        else
                        {
                            SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
                            if (!spellInfo)
                                reason = "NOT_USABLE";
                            else if (spellInfo->Targets &
                                (TARGET_FLAG_ITEM | TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_TRADE_ITEM))
                                reason = "TARGET_REQUIRED";
                            else if (!botAI->CanCastSpell(spellId, bot, false, nullptr, sourceItem))
                                reason = "CAST_FAILED";
                            else
                            {
                                bot->ClearUnitState(UNIT_STATE_CHASE);
                                bot->ClearUnitState(UNIT_STATE_FOLLOW);

                                if (bot->isMoving())
                                {
                                    bot->StopMoving();
                                    reason = "MOVING";
                                }
                                else
                                {
                                    ObjectGuid const sourceGuid = sourceItem->GetGUID();
                                    uint32 const beforeCount = sourceItem->GetCount();
                                    bool const hadCooldown = bot->HasSpellCooldown(spellId);

                                    uint32 const targetMask =
                                        (spellInfo->Targets & TARGET_FLAG_UNIT) ? TARGET_FLAG_UNIT : TARGET_FLAG_NONE;

                                    WorldPacket packet(CMSG_USE_ITEM);
                                    packet << srcBag << srcSlot << uint8(1) << spellId
                                           << sourceGuid << uint32(0) << uint8(0);
                                    packet << targetMask;
                                    if (targetMask & TARGET_FLAG_UNIT)
                                        packet << bot->GetPackGUID();
                                    bot->GetSession()->HandleUseItemOpcode(packet);

                                    Item* const sourceAfter = bot->GetItemByGuid(sourceGuid);
                                    bool const consumed = !sourceAfter || sourceAfter->GetCount() < beforeCount;
                                    bool const cooldownStarted = !hadCooldown && bot->HasSpellCooldown(spellId);
                                    bool const castStarted = bot->IsNonMeleeSpellCast(false);
                                    used = consumed || cooldownStarted || castStarted;
                                    reason = used ? "OK" : "CAST_FAILED";
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (used ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << srcItemId;

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_USE", payload.str());
}
// MB_ITEM_USE_V1_END

// MB_ITEM_SELL_SINGLE_V1_BEGIN
void RunInventoryItemSellCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint8 srcBag,
    uint8 srcSlot,
    uint32 srcItemId,
    uint32 srcCount)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    uint32 soldCount = 0;

    if (!ConsumeInventoryItemSellRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterInventoryItemSellToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!IsInventoryItemEquipSourcePositionAllowed(bot, srcBag, srcSlot))
            reason = "BAD_POSITION";
        else
        {
            InventoryItemMovePositionState const sourceState =
                ReadInventoryItemMovePositionState(bot, srcBag, srcSlot);

            if (!InventoryItemMoveStateMatchesExpected(sourceState, srcItemId, srcCount))
                reason = "SOURCE_STALE";
            else
            {
                Item* const sourceItem = bot->GetItemByPos(srcBag, srcSlot);
                ItemTemplate const* const itemTemplate = sourceItem ? sourceItem->GetTemplate() : nullptr;
                if (!sourceItem || !itemTemplate)
                    reason = "SOURCE_STALE";
                else if (sourceItem->IsNotEmptyBag())
                    reason = "NONEMPTY_BAG";
                else if (itemTemplate->Class == ITEM_CLASS_QUEST)
                    reason = "QUEST_ITEM";
                else if (itemTemplate->Class == ITEM_CLASS_KEY)
                    reason = "KEY_ITEM";
                else if (sourceItem->GetEntry() == 6948)
                    reason = "HEARTHSTONE";
                else if (!itemTemplate->SellPrice)
                    reason = "NO_SELL_PRICE";
                else
                {
                    AiObjectContext* const context = botAI->GetAiObjectContext();
                    if (context && context->GetValue<ItemUsage>("item usage", sourceItem->GetEntry())->Get() == ITEM_USAGE_QUEST)
                        reason = "QUEST_ITEM";
                    else
                    {
                        Creature* const vendor = FindNearbyInteractiveVendor(bot);
                        if (!vendor)
                            reason = "VENDOR_NOT_FOUND";
                        else
                        {
                            ObjectGuid const sourceGuid = sourceItem->GetGUID();
                            uint32 const moneyBefore = bot->GetMoney();

                            WorldPacket packet(CMSG_SELL_ITEM);
                            packet << vendor->GetGUID() << sourceGuid << srcCount;

                            WorldPackets::Item::SellItem sellPacket(std::move(packet));
                            sellPacket.Read();
                            bot->GetSession()->HandleSellItemOpcode(sellPacket);

                            if (botAI->HasCheat(BotCheatMask::gold))
                                bot->SetMoney(moneyBefore);

                            Item* const sourceAfter = bot->GetItemByPos(srcBag, srcSlot);
                            if (!sourceAfter)
                                soldCount = srcCount;
                            else if (sourceAfter->GetGUID() == sourceGuid &&
                                     sourceAfter->GetEntry() == srcItemId &&
                                     sourceAfter->GetCount() < srcCount)
                                soldCount = srcCount - sourceAfter->GetCount();

                            reason = soldCount > 0 ? "OK" : "FAILED";
                        }
                    }
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (soldCount > 0 ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << static_cast<uint32>(srcBag)
        << kFieldSeparator << static_cast<uint32>(srcSlot)
        << kFieldSeparator << srcItemId
        << kFieldSeparator << soldCount;

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_SELL", payload.str());
}
// MB_ITEM_SELL_SINGLE_V1_END
// MB_VENDOR_BUYBACK_V1_BEGIN
struct VendorBuybackEntry
{
    uint32 slot = 0;
    uint32 itemId = 0;
    uint32 count = 0;
    uint32 price = 0;
    uint32 timestamp = 0;
};

void SendVendorBuybackEnd(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    char const* status,
    std::string const& reason,
    uint32 count)
{
    std::ostringstream payload;
    payload << UrlEncodeField(botName)
        << kFieldSeparator << requestToken
        << kFieldSeparator << status
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << count;

    SendAddonPacket(requester, replyType, "BUYBACK_END", payload.str());
}

void SendVendorBuybackPackets(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string reason;

    if (!ConsumeVendorBuybackRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (!FindNearbyInteractiveVendor(bot))
            reason = "VENDOR_NOT_FOUND";
    }

    if (!reason.empty())
    {
        SendVendorBuybackEnd(requester, replyType, effectiveBotName, token, "ERR", reason, 0);
        return;
    }

    std::vector<VendorBuybackEntry> entries;
    entries.reserve(BUYBACK_SLOT_END - BUYBACK_SLOT_START);

    for (uint32 slot = BUYBACK_SLOT_START; slot < BUYBACK_SLOT_END; ++slot)
    {
        Item* const item = bot->GetItemFromBuyBackSlot(slot);
        if (!item)
            continue;

        VendorBuybackEntry entry;
        entry.slot = slot;
        entry.itemId = item->GetEntry();
        entry.count = item->GetCount();
        entry.price = bot->GetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START);
        entry.timestamp = bot->GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + slot - BUYBACK_SLOT_START);
        entries.push_back(entry);
    }

    std::ostringstream beginPayload;
    beginPayload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << entries.size();
    SendAddonPacket(requester, replyType, "BUYBACK_BEGIN", beginPayload.str());

    for (VendorBuybackEntry const& entry : entries)
    {
        std::ostringstream itemPayload;
        itemPayload << UrlEncodeField(effectiveBotName)
            << kFieldSeparator << token
            << kFieldSeparator << entry.slot
            << kFieldSeparator << entry.itemId
            << kFieldSeparator << entry.count
            << kFieldSeparator << entry.price
            << kFieldSeparator << entry.timestamp;
        SendAddonPacket(requester, replyType, "BUYBACK_ITEM", itemPayload.str());
    }

    SendVendorBuybackEnd(
        requester, replyType, effectiveBotName, token, "OK", "OK", static_cast<uint32>(entries.size()));
}

void RunVendorBuybackCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint32 slot,
    uint32 expectedItemId,
    uint32 expectedCount,
    uint32 expectedPrice)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string reason;
    bool purchased = false;

    if (!ConsumeVendorBuybackRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (!RegisterVendorBuybackToken(requester, token))
            reason = "DUPLICATE";
        else if (!requester || !requester->GetSession())
            reason = "NO_REQUESTER_SESSION";
        else if (!bot->GetSession())
            reason = "NO_BOT_SESSION";
        else if (!bot->IsInWorld())
            reason = "BOT_NOT_IN_WORLD";
        else if (!bot->IsAlive())
            reason = "BOT_DEAD";
        else if (slot < BUYBACK_SLOT_START || slot >= BUYBACK_SLOT_END)
            reason = "BAD_SLOT";
        else
        {
            Item* const sourceItem = bot->GetItemFromBuyBackSlot(slot);
            uint32 const actualPrice = bot->GetUInt32Value(
                PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START);

            if (!sourceItem ||
                sourceItem->GetEntry() != expectedItemId ||
                sourceItem->GetCount() != expectedCount ||
                actualPrice != expectedPrice)
            {
                reason = "SOURCE_STALE";
            }
            else
            {
                Creature* const vendor = FindNearbyInteractiveVendor(bot);
                if (!vendor)
                    reason = "VENDOR_NOT_FOUND";
                else if (!bot->HasEnoughMoney(actualPrice))
                    reason = "NOT_ENOUGH_MONEY";
                else
                {
                    ItemPosCountVec destination;
                    InventoryResult const canStore =
                        bot->CanStoreItem(NULL_BAG, NULL_SLOT, destination, sourceItem, false);
                    if (canStore != EQUIP_ERR_OK)
                    {
                        reason = "CANNOT_STORE";
                    }
                    else
                    {
                        WorldPacket packet(CMSG_BUYBACK_ITEM);
                        packet << vendor->GetGUID() << slot;

                        WorldPackets::Item::BuybackItem buybackPacket(std::move(packet));
                        buybackPacket.Read();
                        bot->GetSession()->HandleBuybackItem(buybackPacket);

                        purchased = bot->GetItemFromBuyBackSlot(slot) == nullptr;
                        reason = purchased ? "OK" : "FAILED";
                    }
                }
            }
        }
    }

    if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (purchased ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << slot
        << kFieldSeparator << expectedItemId
        << kFieldSeparator << expectedCount
        << kFieldSeparator << expectedPrice;

    SendAddonPacket(requester, replyType, "BUYBACK_RESULT", payload.str());
}
// MB_VENDOR_BUYBACK_V1_END


void RunInventoryItemActionCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& actionValue, std::string const& itemIdValue, std::string const& countValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    std::string const action = ToUpper(Trim(actionValue));
    uint32 itemId = 0;
    uint32 requestedCount = 0;
    bool zeroParamsInvalid = false;
    if (action == "SELL_GREY" || action == "SELL_VENDOR" || action == "OPEN_ITEMS")
    {
        if (Trim(itemIdValue) != "0" || Trim(countValue) != "0")
            zeroParamsInvalid = true;
    }
    else
    {
        TryParseUint32Field(Trim(itemIdValue), 1, std::numeric_limits<uint32>::max(), itemId);
        TryParseUint32Field(Trim(countValue), 0, kMaxItemActionCount, requestedCount);
    }

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    uint32 moved = 0;
    if (!ConsumeItemActionRateLimit(requester))
    {
        reason = "RATE_LIMIT";
    }
    else if (!bot)
    {
        reason = "NO_BOT";
    }
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
        {
            reason = "FORBIDDEN";
        }
        else if (action == "BANK_DEPOSIT")
            moved = MoveMatchingBagItemsToBank(bot, itemId, requestedCount, reason);
        else if (action == "BANK_WITHDRAW")
            moved = MoveMatchingBankItemsToBags(bot, itemId, requestedCount, reason);
        else if (action == "GBANK_DEPOSIT")
            moved = MoveMatchingBagItemsToGuildBank(requester, bot, itemId, requestedCount, reason);
        else if (action == "GBANK_WITHDRAW")
            moved = MoveMatchingGuildBankItemsToBags(requester, bot, itemId, requestedCount, reason);
        else if (action == "BUY_ITEM")
            moved = BuyMatchingVendorItem(bot, itemId, requestedCount, reason);
        else if (action == "SELL_GREY")
        {
            if (zeroParamsInvalid)
                reason = "BAD_REQUEST";
            else
                moved = SellGreyBagItems(bot, reason);
        }
        else if (action == "SELL_VENDOR")
        {
            if (zeroParamsInvalid)
                reason = "BAD_REQUEST";
            else
                moved = SellVendorBagItems(bot, reason);
        }
        else if (action == "OPEN_ITEMS")
        {
            if (zeroParamsInvalid)
            {
                reason = "BAD_REQUEST";
            }
            else if (!bot->GetSession())
            {
                reason = "NO_SESSION";
            }
            else
            {
                // MB_OPEN_ITEMS_RESIDUAL_V1_BEGIN
                // OPEN_ITEMS is manual/residual only. Playerbots already runs "open items"
                // automatically from the "item push result" world-packet trigger.
                AiObjectContext* const context = botAI->GetAiObjectContext();
                if (!context)
                {
                    reason = "OPEN_FAILED";
                }
                else
                {
                    bool autoOpenPending = false;
                    Trigger* const itemPushTrigger = context->GetTrigger("item push result");
                    if (itemPushTrigger)
                    {
                        Event pendingItemPush = itemPushTrigger->Check();
                        autoOpenPending = !pendingItemPush.getPacket().empty();
                    }

                    if (autoOpenPending)
                    {
                        reason = "AUTO_OPEN_PENDING";
                    }
                    else
                    {
                        auto isResidualOpenable = [bot](Item* candidate) -> bool
                        {
                            if (!candidate || candidate->m_lootGenerated)
                                return false;

                            ItemTemplate const* const itemTemplate = candidate->GetTemplate();
                            if (!itemTemplate || bot->CanUseItem(itemTemplate) != EQUIP_ERR_OK ||
                                !itemTemplate->HasFlag(ITEM_FLAG_HAS_LOOT))
                            {
                                return false;
                            }

                            if (itemTemplate->LockID != 0 && candidate->IsLocked())
                                return false;

                            return true;
                        };

                        Item* item = nullptr;
                        for (uint8 slot = INVENTORY_SLOT_ITEM_START; !item && slot < INVENTORY_SLOT_ITEM_END; ++slot)
                        {
                            Item* const candidate = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                            if (isResidualOpenable(candidate))
                                item = candidate;
                        }

                        for (uint8 bag = INVENTORY_SLOT_BAG_START; !item && bag < INVENTORY_SLOT_BAG_END; ++bag)
                        {
                            Bag* const container = bot->GetBagByPos(bag);
                            if (!container)
                                continue;

                            for (uint32 slot = 0; !item && slot < container->GetBagSize(); ++slot)
                            {
                                Item* const candidate = bot->GetItemByPos(bag, static_cast<uint8>(slot));
                                if (isResidualOpenable(candidate))
                                    item = candidate;
                            }
                        }

                        if (!item)
                        {
                            reason = "NO_OPENABLE_ITEM";
                        }
                        else
                        {
                            uint8 const bag = item->GetBagSlot();
                            uint8 const slot = item->GetSlot();
                            ObjectGuid const itemGuid = item->GetGUID();
                            itemId = item->GetEntry();

                            WorldPacket packet(CMSG_OPEN_ITEM);
                            packet << bag << slot;
                            bot->GetSession()->HandleOpenItemOpcode(packet);

                            if (item->m_lootGenerated)
                            {
                                LootObject lootObject;
                                lootObject.guid = itemGuid;
                                context->GetValue<LootObject>("loot target")->Set(lootObject);
                                moved = 1;
                            }
                            else
                            {
                                reason = "OPEN_FAILED";
                            }
                        }
                    }
                }
                // MB_OPEN_ITEMS_RESIDUAL_V1_END
            }
        }
        else
            reason = "BAD_ACTION";
    }

    bool const ok = moved > 0;
    if (ok)
        reason = "OK";
    else if (reason.empty())
        reason = "FAILED";

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << action
        << kFieldSeparator << itemId
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << moved;

    SendAddonPacket(requester, replyType, "INVENTORY_ITEM_ACTION", payload.str());
}

void SendEnchantTradePackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string reason = "OK";

    if (!ConsumeEnchantTradeRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
        {
            reason = "FORBIDDEN";
        }
        else if (!bot->HasSkill(SKILL_ENCHANTING))
            reason = "NOT_ENCHANTER";
    }

    uint32 const skillValue = bot && bot->HasSkill(SKILL_ENCHANTING) ? bot->GetSkillValue(SKILL_ENCHANTING) : 0;
    uint32 const maxSkill = bot && bot->HasSkill(SKILL_ENCHANTING) ? bot->GetMaxSkillValue(SKILL_ENCHANTING) : 0;
    bool const ok = reason == "OK";

    std::ostringstream beginPayload;
    beginPayload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << skillValue
        << kFieldSeparator << maxSkill;
    SendAddonPacket(requester, replyType, "ENCHANT_TRADE_BEGIN", beginPayload.str());

    uint32 count = 0;
    if (ok)
    {
        for (EnchantTradeEntryData const& entry : BuildEnchantTradeEntries(bot))
        {
            std::ostringstream payload;
            payload << UrlEncodeField(effectiveBotName)
                << kFieldSeparator << token
                << kFieldSeparator << entry.spellId
                << kFieldSeparator << UrlEncodeField(entry.difficulty)
                << kFieldSeparator << entry.available
                << kFieldSeparator << entry.hasTools
                << kFieldSeparator << entry.materials.size();
            if (!IsAddonPacketWithinBudget("ENCHANT_TRADE_ITEM", payload.str()))
                continue;

            SendAddonPacket(requester, replyType, "ENCHANT_TRADE_ITEM", payload.str());
            uint32 materialIndex = 0;
            for (EnchantTradeMaterialData const& material : entry.materials)
            {
                ++materialIndex;
                std::ostringstream materialPayload;
                materialPayload << UrlEncodeField(effectiveBotName)
                    << kFieldSeparator << token
                    << kFieldSeparator << entry.spellId
                    << kFieldSeparator << materialIndex
                    << kFieldSeparator << material.itemId
                    << kFieldSeparator << material.required
                    << kFieldSeparator << material.available;
                if (IsAddonPacketWithinBudget("ENCHANT_TRADE_MATERIAL", materialPayload.str()))
                    SendAddonPacket(requester, replyType, "ENCHANT_TRADE_MATERIAL", materialPayload.str());
            }
            ++count;
        }
    }

    std::ostringstream endPayload;
    endPayload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << count;
    SendAddonPacket(requester, replyType, "ENCHANT_TRADE_END", endPayload.str());
}

std::string ValidateEnchantTradeContext(Player* requester, Player* bot, uint32 spellId, SpellInfo const*& spellInfo)
{
    if (!requester || !bot)
        return "BAD_REQUEST";

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetSecurity() ||
        !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
    {
        return "FORBIDDEN";
    }

    if (!bot->GetSession())
        return "NO_SESSION";

    std::string const identityReason = ValidateEnchantTradeSpellIdentity(bot, spellId, spellInfo);
    if (identityReason != "OK")
        return identityReason;

    if (!BotHasRecipeRequiredTools(bot, spellInfo))
        return "MISSING_TOOLS";

    uint32 materialsAvailable = 0;
    std::vector<EnchantTradeMaterialData> materials;
    BuildEnchantTradeMaterials(spellInfo, BuildBotInventoryItemCounts(bot), materials, materialsAvailable);
    if (!materialsAvailable)
        return "NO_MATERIALS";

    if (bot->IsInCombat())
        return "IN_COMBAT";
    if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL))
        return "LOST_CONTROL";
    if (bot->IsFlying() || bot->HasUnitState(UNIT_STATE_IN_FLIGHT))
        return "IN_FLIGHT";
    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return "CHANNELING";
    if (bot->HasSpellCooldown(spellId))
        return "NOT_READY";
    if (!bot->IsStandState())
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
        return "NOT_STANDING";
    }

    uint32 const castTime = !spellInfo->IsChanneled() ? spellInfo->CalcCastTime(bot) : spellInfo->GetDuration();
    if ((castTime || spellInfo->IsAutoRepeatRangedSpell()) && bot->isMoving())
        return "MOVING";

    TradeData* const botTrade = bot->GetTradeData();
    TradeData* const requesterTrade = requester ? requester->GetTradeData() : nullptr;
    if (!botTrade || !requesterTrade)
        return "NO_TRADE";
    if (botTrade->GetTrader() != requester || requesterTrade->GetTrader() != bot)
        return "WRONG_TRADER";
    if (!requesterTrade->GetItem(TRADE_SLOT_NONTRADED))
        return "NO_TRADE_ITEM";
    if (botTrade->GetSpell())
        return "ALREADY_ENCHANTED";

    return "OK";
}

void RunEnchantTradeCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& spellIdValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 spellId = 0;
    TryParseUint32Field(Trim(spellIdValue), 1, std::numeric_limits<uint32>::max(), spellId);

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;
    std::string reason = "OK";
    bool accepted = false;

    if (!ConsumeEnchantTradeRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!bot)
        reason = "NO_BOT";
    else
    {
        SpellInfo const* spellInfo = nullptr;
        reason = ValidateEnchantTradeContext(requester, bot, spellId, spellInfo);
        if (reason == "OK")
        {
            SpellCastTargets targets;
            targets.SetTradeItemTarget(bot);

            // Mirror the native TradeHandler validation path without preparing a
            // heap-allocated Spell here. The Core owns final spell preparation
            // when the normal trade is accepted.
            Spell spell(bot, spellInfo, TRIGGERED_FULL_MASK);
            spell.m_targets = targets;
            SpellCastResult const result = spell.CheckCast(true);
            if (result == SPELL_FAILED_EQUIPPED_ITEM_CLASS)
                reason = "BAD_TARGET";
            else
                reason = GetSpellCastFailureReason(result);

            if (result == SPELL_CAST_OK)
            {
                TradeData* const botTrade = bot->GetTradeData();
                if (!botTrade)
                    reason = "NO_TRADE";
                else
                {
                    botTrade->SetSpell(spellId);
                    accepted = true;
                }
            }
        }
    }

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << spellId
        << kFieldSeparator << (accepted ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << (accepted ? 1 : 0);
    SendAddonPacket(requester, replyType, "ENCHANT_TRADE_RESULT", payload.str());
}

void RunProfessionRecipeCraftCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& skillIdValue, std::string const& spellIdValue, std::string const& itemIdValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 skillId = 0;
    uint32 spellId = 0;
    uint32 expectedItemId = 0;
    TryParseUint32Field(Trim(skillIdValue), 1, std::numeric_limits<uint32>::max(), skillId);
    TryParseUint32Field(Trim(spellIdValue), 1, std::numeric_limits<uint32>::max(), spellId);
    TryParseUint32Field(Trim(itemIdValue), 0, std::numeric_limits<uint32>::max(), expectedItemId);

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    uint32 actualItemId = expectedItemId;
    std::string result = ValidateProfessionRecipeCraft(bot, skillId, spellId, expectedItemId, actualItemId);
    if (result == "OK")
    {
        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (ProfessionRecipeRequiresExactItemTarget(spellInfo))
            result = "TARGET_REQUIRED";
        else
            result = CastProfessionRecipe(bot, spellId);
    }

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << skillId
        << kFieldSeparator << spellId
        << kFieldSeparator << actualItemId
        << kFieldSeparator << (result == "OK" ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(result);

    SendAddonPacket(requester, replyType, "PROFESSION_RECIPE_CRAFT", payload.str());
}


// MB_CRAFT_RECIPE_TARGET_V1_COMMAND_BEGIN
void RunProfessionRecipeTargetCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& requestToken,
    uint32 skillId,
    uint32 spellId,
    uint32 targetBag,
    uint32 targetSlot,
    uint32 targetItemId)
{
    if (!requester || !requester->GetSession())
        return;

    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason = "OK";
    Item* targetItem = nullptr;
    PlayerbotAI* const botAI = bot ? GetBotAI(bot) : nullptr;

    if (!ConsumeCraftRecipeTargetRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!RegisterCraftRecipeTargetToken(requester, token))
        reason = "REPLAY";
    else if (!bot)
        reason = "NO_BOT";
    else if (!bot->GetSession() || !bot->IsInWorld())
        reason = "BOT_UNAVAILABLE";
    else if (!bot->IsAlive())
        reason = "BOT_DEAD";
    else if (!botAI)
        reason = "NO_AI";
    else if (!botAI->GetSecurity() ||
        !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
        reason = "FORBIDDEN";
    else if (!IsAllowedProfessionRecipeTargetPosition(targetBag, targetSlot))
        reason = "BAD_TARGET_POSITION";
    else if (!bot->IsValidPos(static_cast<uint8>(targetBag), static_cast<uint8>(targetSlot), true))
        reason = "BAD_TARGET_POSITION";
    else
    {
        targetItem = bot->GetItemByPos(static_cast<uint8>(targetBag), static_cast<uint8>(targetSlot));
        if (!targetItem)
            reason = "MISSING_TARGET_ITEM";
        else if (targetItem->GetEntry() != targetItemId)
            reason = "TARGET_STALE";
    }

    uint32 ignoredCreatedItemId = 0;
    if (reason == "OK")
        reason = ValidateProfessionRecipeCraft(bot, skillId, spellId, 0, ignoredCreatedItemId);

    SpellInfo const* const spellInfo = reason == "OK" ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (reason == "OK" && !ProfessionRecipeRequiresExactItemTarget(spellInfo))
        reason = "NOT_ITEM_TARGET_RECIPE";

    if (reason == "OK")
        reason = CastProfessionRecipeTarget(bot, spellId, targetItem);

    std::string const status = reason == "OK" ? "OK" : "ERR";
    std::ostringstream payload;
    payload << token
        << kFieldSeparator << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << status
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << skillId
        << kFieldSeparator << spellId
        << kFieldSeparator << targetBag
        << kFieldSeparator << targetSlot
        << kFieldSeparator << targetItemId;

    SendAddonPacket(requester, replyType, "CRAFT_RECIPE_TARGET_RESULT", payload.str());
}
// MB_CRAFT_RECIPE_TARGET_V1_END

void RunOutfitCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& encodedSuffix, std::string const& persistToken)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    std::string const suffix = SanitizeOutfitCommandSuffix(UrlDecodeField(encodedSuffix));
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    bool const persist = persistToken == "1";

    bool ok = false;
    if (bot && IsAllowedOutfitCommandSuffix(suffix))
        ok = ApplyBridgeNativeOutfitCommand(bot, suffix, persist);

    std::ostringstream payload;
    payload << UrlEncodeField(effectiveBotName)
        << kFieldSeparator << token
        << kFieldSeparator << (ok ? "OK" : "ERR");

    SendAddonPacket(requester, replyType, "OUTFITS_CMD", payload.str());
}

bool IsAllowedRTIIcon(std::string const& value)
{
    static std::set<std::string> const allowed = { "STAR", "CIRCLE", "DIAMOND", "TRIANGLE", "MOON", "SQUARE", "CROSS", "SKULL" };
    return allowed.find(ToUpper(Trim(value))) != allowed.end();
}

bool IsAllowedRTICommand(std::string const& command)
{
    std::istringstream in(ToUpper(Trim(command)));
    std::vector<std::string> parts;
    std::string part;

    while (in >> part)
        parts.push_back(part);

    if (parts.size() == 3 && (parts[0] == "ATTACK" || parts[0] == "PULL") && parts[1] == "RTI" && parts[2] == "TARGET")
        return true;

    if (parts.size() == 2 && parts[0] == "RTI" && IsAllowedRTIIcon(parts[1]))
        return true;

    if (parts.size() == 3 && parts[0] == "RTI" && parts[1] == "CC" && IsAllowedRTIIcon(parts[2]))
        return true;

    return false;
}

bool IsAllowedPositionCommand(std::string const& command)
{
    return command == "disperse disable" || command.rfind("disperse set ", 0) == 0;
}

bool ApplyNativeDisperseCommand(Player* bot, std::string const& command)
{
    if (!bot)
        return false;

    PlayerbotAI* const ai = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!ai || !ai->GetAiObjectContext())
        return false;

    float distance = 0.0f;

    if (command != "disperse disable")
    {
        std::string const prefix = "disperse set ";
        std::string const valueText = Trim(command.substr(prefix.size()));

        char* end = nullptr;
        double const value = std::strtod(valueText.c_str(), &end);
        if (!end || *end != '\0' || !std::isfinite(value) || value <= 0.0 || value > 100.0)
            return false;

        distance = static_cast<float>(value);
    }

    AiObjectContext* const context = ai->GetAiObjectContext();
    auto* const disperseDistance = context->GetValue<float>("disperse distance");
    if (!disperseDistance)
        return false;

    disperseDistance->Set(distance);

    return true;
}

bool IsAllowedCombatCommand(std::string const& command)
{
    std::string const normalized = ToUpper(Trim(command));

    static std::set<std::string> const allowed =
    {
        "CO +FOCUS",
        "CO -FOCUS",
        "CO +DPS ASSIST",
        "CO -DPS ASSIST",
        "CO +AOE",
        "CO -AOE",
        "CO +DPS AOE",
        "CO -DPS AOE",
        "CO +TANK ASSIST",
        "CO -TANK ASSIST",
        "CO +AVOID AOE",
        "CO -AVOID AOE",
        "CO +SAVE MANA",
        "CO -SAVE MANA",
        "CO +THREAT",
        "CO -THREAT",
        "CO +BEHIND",
        "CO -BEHIND",
        "CO +WAIT FOR ATTACK",
        "CO -WAIT FOR ATTACK"
    };

    if (allowed.find(normalized) != allowed.end())
        return true;

    static std::string const waitPrefix = "WAIT FOR ATTACK TIME ";
    if (normalized.rfind(waitPrefix, 0) != 0)
        return false;

    std::string const value = Trim(normalized.substr(waitPrefix.size()));
    if (value.empty())
        return false;

    uint32 seconds = 0;
    for (char c : value)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;

        seconds = (seconds * 10) + static_cast<uint32>(c - '0');
        if (seconds > 60)
            return false;
    }

    return true;
}

std::string NormalizeCombatCommand(std::string const& command)
{
    std::string const trimmed = Trim(command);
    std::string const normalized = ToUpper(trimmed);

    // Backward compatibility with the first addon patch.
    // Playerbots exposes the strategy as "aoe", not "dps aoe".
    if (normalized == "CO +DPS AOE")
        return "co +aoe";

    if (normalized == "CO -DPS AOE")
        return "co -aoe";

    return trimmed;
}

std::string NormalizePositionCommand(std::string const& command)
{
    std::string normalized = Trim(command);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "disperse disable")
        return normalized;

    std::string const prefix = "disperse set ";
    if (normalized.rfind(prefix, 0) != 0)
        return "";

    std::string const valueText = Trim(normalized.substr(prefix.size()));
    if (valueText.empty())
        return "";

    char* end = nullptr;
    double const value = std::strtod(valueText.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(value) || value <= 0.0 || value > 100.0)
        return "";

    std::ostringstream out;
    out << "disperse set " << value;
    return out.str();
}

std::string NormalizeLootCommand(std::string const& command)
{
    std::string normalized = Trim(command);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });

    return normalized;
}

bool IsAllowedLootCommand(std::string const& command)
{
    static std::set<std::string> const allowed =
    {
        "nc +loot",
        "nc -loot",
        "ll all",
        "ll normal",
        "ll gray",
        "ll disenchant"
    };

    return allowed.find(Trim(command)) != allowed.end();
}

bool BotMatchesRTIScope(Player* requester, Player* bot, std::string const& scope, std::string const& target)
{
    if (!requester || !bot)
        return false;

    if (scope == "ALL")
        return true;

    if (scope == "BOT")
        return bot->GetName() == target;

    if (scope == "GROUP")
    {
        uint32 groupNumber = 0;
        if (!TryParseUint32Field(target, 1, 8, groupNumber))
            return false;

        Group* const group = requester->GetGroup();
        if (!group || bot->GetGroup() != group)
            return false;

        return group->GetMemberGroup(bot->GetGUID()) == groupNumber - 1;
    }

    return false;
}

bool BotMatchesCombatScope(Player* requester, Player* bot, std::string const& scope, std::string const& target)
{
    if (!requester || !bot)
        return false;

    if (scope == "ALL")
        return true;

    if (scope == "RAID")
    {
        Group* const group = requester->GetGroup();
        return group && group->isRaidGroup() && bot->GetGroup() == group;
    }

    if (scope == "GROUP" || scope == "PARTY")
    {
        if (!target.empty())
            return BotMatchesRTIScope(requester, bot, "GROUP", target);

        Group* const group = requester->GetGroup();
        if (!group)
            return false;

        return bot->GetGroup() == group;
    }

    return BotMatchesRTIScope(requester, bot, scope, target);
}

struct StrategyMutationOperation
{
    bool enable = false;
    std::string name;
};

struct StrategyMutationRateState
{
    std::deque<std::chrono::steady_clock::time_point> requests;
};

std::map<std::string, StrategyMutationRateState> sStrategyMutationRateStates;

bool IsValidStrategyName(std::string const& name)
{
    if (name.empty() || name.size() > kMaxStrategyNameLength || name != Trim(name))
        return false;

    for (unsigned char const c : name)
    {
        if (!std::isalnum(c) && c != ' ' && c != '-' && c != '_' && c != '\'')
            return false;
    }

    return true;
}

bool TryNormalizeStrategyChanges(
    std::string const& value,
    std::string& normalized,
    std::vector<StrategyMutationOperation>& operations,
    std::string& reason)
{
    normalized.clear();
    operations.clear();
    reason.clear();

    std::string const changes = Trim(value);
    if (changes.empty() || changes.size() > kMaxCommandLength)
    {
        reason = "BAD_CHANGES";
        return false;
    }

    std::size_t start = 0;
    while (true)
    {
        std::size_t const separator = changes.find(',', start);
        std::string const rawOperation =
            separator == std::string::npos ? changes.substr(start) : changes.substr(start, separator - start);
        std::string const operation = Trim(rawOperation);

        if (operation.size() < 2 || (operation[0] != '+' && operation[0] != '-'))
        {
            reason = "BAD_OPERATION";
            return false;
        }

        std::string const name = ToLower(Trim(operation.substr(1)));
        if (!IsValidStrategyName(name))
        {
            reason = "BAD_STRATEGY";
            return false;
        }

        operations.push_back({operation[0] == '+', name});
        if (operations.size() > kMaxStrategyOperations)
        {
            reason = "TOO_MANY_OPERATIONS";
            return false;
        }

        if (!normalized.empty())
            normalized.push_back(',');
        normalized.push_back(operation[0]);
        normalized += name;

        if (separator == std::string::npos)
            break;

        start = separator + 1;
    }

    if (operations.empty())
    {
        reason = "BAD_CHANGES";
        return false;
    }

    reason = "OK";
    return true;
}

bool ConsumeStrategyMutationRateLimit(Player* requester)
{
    if (!requester)
        return false;

    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();
    StrategyMutationRateState& state = sStrategyMutationRateStates[key];

    while (!state.requests.empty() && now - state.requests.front() >= kStrategyMutationRateWindow)
        state.requests.pop_front();

    if (state.requests.size() >= kStrategyMutationRateLimit)
        return false;

    state.requests.push_back(now);

    if (sStrategyMutationRateStates.size() > 512)
    {
        for (auto it = sStrategyMutationRateStates.begin(); it != sStrategyMutationRateStates.end();)
        {
            while (!it->second.requests.empty() && now - it->second.requests.front() >= kStrategyMutationRateWindow)
                it->second.requests.pop_front();

            if (it->second.requests.empty() && it->first != key)
                it = sStrategyMutationRateStates.erase(it);
            else
                ++it;
        }
    }

    return true;
}

bool VerifyStrategyMutationResult(
    PlayerbotAI* botAI,
    BotState botState,
    std::vector<StrategyMutationOperation> const& operations)
{
    if (!botAI)
        return false;

    std::set<std::string> verifiedStrategies;
    for (auto operation = operations.rbegin(); operation != operations.rend(); ++operation)
    {
        if (!verifiedStrategies.insert(operation->name).second)
            continue;

        if (botAI->HasStrategy(operation->name, botState) != operation->enable)
            return false;
    }

    return true;
}

void CollectCarriedWarlockStoneEnchantIds(Item* item, std::set<uint32>& enchantIds)
{
    if (!item)
        return;

    ItemTemplate const* const proto = item->GetTemplate();
    if (!proto)
        return;

    std::string const itemName = ToLower(proto->Name1);
    if (itemName.find("firestone") == std::string::npos && itemName.find("spellstone") == std::string::npos)
        return;

    for (uint8 spellIndex = 0; spellIndex < MAX_ITEM_PROTO_SPELLS; ++spellIndex)
    {
        uint32 const spellId = proto->Spells[spellIndex].SpellId;
        if (!spellId)
            continue;

        SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        for (uint8 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
        {
            SpellEffectInfo const& effect = spellInfo->Effects[effectIndex];
            if (effect.Effect == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY && effect.MiscValue > 0)
                enchantIds.insert(static_cast<uint32>(effect.MiscValue));
        }
    }
}

std::set<uint32> GetCarriedWarlockStoneEnchantIds(Player* bot)
{
    std::set<uint32> enchantIds;
    if (!bot)
        return enchantIds;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        CollectCarriedWarlockStoneEnchantIds(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), enchantIds);

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* const pBag = static_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag));
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            CollectCarriedWarlockStoneEnchantIds(pBag->GetItemByPos(slot), enchantIds);
    }

    return enchantIds;
}

enum class WarlockStoneSwitchResult
{
    NotRequired,
    Applied,
    Failed
};

bool IsWarlockStoneStrategyMutation(
    Player* bot,
    BotState botState,
    std::vector<StrategyMutationOperation> const& operations)
{
    if (!bot || botState != BOT_STATE_NON_COMBAT || bot->getClass() != CLASS_WARLOCK)
        return false;

    for (StrategyMutationOperation const& operation : operations)
    {
        if (operation.name == "firestone" || operation.name == "spellstone")
            return true;
    }

    return false;
}

std::map<std::string, bool> CaptureStrategyMutationState(
    PlayerbotAI* botAI,
    BotState botState,
    std::vector<StrategyMutationOperation> const& operations)
{
    std::map<std::string, bool> states;
    if (!botAI)
        return states;

    for (StrategyMutationOperation const& operation : operations)
    {
        if (states.find(operation.name) == states.end())
            states.emplace(operation.name, botAI->HasStrategy(operation.name, botState));
    }

    return states;
}

bool RollbackNativeStrategyMutation(
    Player* requester,
    PlayerbotAI* botAI,
    std::string const& actionName,
    BotState botState,
    std::map<std::string, bool> const& priorStates)
{
    if (!requester || !botAI)
        return false;

    if (priorStates.empty())
        return true;

    std::ostringstream changes;
    std::vector<StrategyMutationOperation> rollbackOperations;
    bool first = true;

    for (auto const& priorState : priorStates)
    {
        if (!first)
            changes << ',';

        changes << (priorState.second ? '+' : '-') << priorState.first;
        rollbackOperations.push_back({priorState.second, priorState.first});
        first = false;
    }

    if (!botAI->DoSpecificAction(actionName, Event(actionName, changes.str(), requester), true))
        return false;

    return VerifyStrategyMutationResult(botAI, botState, rollbackOperations);
}

WarlockStoneSwitchResult TryForceWarlockStoneSwitch(
    Player* requester,
    Player* bot,
    PlayerbotAI* botAI,
    BotState botState,
    bool hadFirestoneStrategy,
    bool hadSpellstoneStrategy)
{
    if (!requester || !bot || !botAI || botState != BOT_STATE_NON_COMBAT || bot->getClass() != CLASS_WARLOCK)
        return WarlockStoneSwitchResult::NotRequired;

    bool const hasFirestoneStrategy = botAI->HasStrategy("firestone", BOT_STATE_NON_COMBAT);
    bool const hasSpellstoneStrategy = botAI->HasStrategy("spellstone", BOT_STATE_NON_COMBAT);

    std::string desiredStone;
    if (!hadFirestoneStrategy && hadSpellstoneStrategy && hasFirestoneStrategy && !hasSpellstoneStrategy)
        desiredStone = "firestone";
    else if (hadFirestoneStrategy && !hadSpellstoneStrategy && !hasFirestoneStrategy && hasSpellstoneStrategy)
        desiredStone = "spellstone";
    else
        return WarlockStoneSwitchResult::NotRequired;

    Item* const mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainHand)
        return WarlockStoneSwitchResult::NotRequired;

    uint32 const currentEnchantId = mainHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
    if (!currentEnchantId)
        return WarlockStoneSwitchResult::NotRequired;

    std::set<uint32> const carriedStoneEnchantIds = GetCarriedWarlockStoneEnchantIds(bot);
    if (carriedStoneEnchantIds.find(currentEnchantId) == carriedStoneEnchantIds.end())
    {
        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge warlock stone switch skipped bot={} requested={} currentEnchant={} reason=UNRECOGNIZED_TEMP_ENCHANT",
                bot->GetName(),
                desiredStone,
                currentEnchantId);
        }
        return WarlockStoneSwitchResult::NotRequired;
    }

    uint32 const currentEnchantDuration = mainHand->GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT);
    uint32 const currentEnchantCharges = mainHand->GetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT);

    // stateScope N selects the non-combat strategy bucket; it is not a runtime combat-state guarantee.
    // Re-check immediately before touching the equipped enchantment to close the race after the pre-mutation guard.
    if (bot->IsInCombat())
    {
        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge warlock stone switch skipped bot={} requested={} currentEnchant={} reason=RUNTIME_COMBAT_BEFORE_ENCHANT_CLEAR",
                bot->GetName(),
                desiredStone,
                currentEnchantId);
        }
        return WarlockStoneSwitchResult::Failed;
    }

    bot->ApplyEnchantment(mainHand, TEMP_ENCHANTMENT_SLOT, false);
    mainHand->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);

    bool const applied = botAI->DoSpecificAction(desiredStone, Event(), true);
    if (!applied)
    {
        // Restore the exact persistent temporary-enchant fields and re-apply its equipped effects/duration tracking.
        mainHand->SetEnchantment(
            TEMP_ENCHANTMENT_SLOT,
            currentEnchantId,
            currentEnchantDuration,
            currentEnchantCharges,
            bot->GetGUID());
        bot->ApplyEnchantment(mainHand, TEMP_ENCHANTMENT_SLOT, true);

        bool const restored =
            mainHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) == currentEnchantId &&
            mainHand->GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT) == currentEnchantDuration &&
            mainHand->GetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT) == currentEnchantCharges;

        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge warlock stone switch failed bot={} requested={} previousEnchant={} previousDuration={} previousCharges={} enchantRestored={}",
                bot->GetName(),
                desiredStone,
                currentEnchantId,
                currentEnchantDuration,
                currentEnchantCharges,
                restored);
        }

        return WarlockStoneSwitchResult::Failed;
    }

    if (BridgeConsoleLogsEnabled())
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge warlock stone switch bot={} requested={} previousEnchant={} applied={} resultingEnchant={}",
            bot->GetName(),
            desiredStone,
            currentEnchantId,
            applied,
            mainHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT));
    }

    return WarlockStoneSwitchResult::Applied;
}

bool ApplyNativeStrategyMutation(
    Player* requester,
    Player* bot,
    std::string const& actionName,
    BotState botState,
    std::string const& changes,
    std::vector<StrategyMutationOperation> const& operations)
{
    if (!requester || !bot)
        return false;

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI || !botAI->GetSecurity() ||
        !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
    {
        return false;
    }

    bool const isWarlockStoneMutation = IsWarlockStoneStrategyMutation(bot, botState, operations);
    if (isWarlockStoneMutation && bot->IsInCombat())
    {
        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge warlock stone strategy mutation rejected bot={} reason=RUNTIME_COMBAT_BEFORE_MUTATION",
                bot->GetName());
        }
        return false;
    }

    std::map<std::string, bool> priorStrategyStates;
    if (isWarlockStoneMutation)
        priorStrategyStates = CaptureStrategyMutationState(botAI, botState, operations);

    bool const hadFirestoneStrategy =
        botState == BOT_STATE_NON_COMBAT && botAI->HasStrategy("firestone", BOT_STATE_NON_COMBAT);
    bool const hadSpellstoneStrategy =
        botState == BOT_STATE_NON_COMBAT && botAI->HasStrategy("spellstone", BOT_STATE_NON_COMBAT);

    if (!botAI->DoSpecificAction(actionName, Event(actionName, changes, requester), true))
        return false;

    if (!VerifyStrategyMutationResult(botAI, botState, operations))
        return false;

    WarlockStoneSwitchResult const stoneSwitchResult = TryForceWarlockStoneSwitch(
        requester,
        bot,
        botAI,
        botState,
        hadFirestoneStrategy,
        hadSpellstoneStrategy);

    if (stoneSwitchResult == WarlockStoneSwitchResult::Failed)
    {
        bool const strategyRollbackSucceeded = RollbackNativeStrategyMutation(
            requester,
            botAI,
            actionName,
            botState,
            priorStrategyStates);

        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge warlock stone strategy rollback bot={} succeeded={}",
                bot->GetName(),
                strategyRollbackSucceeded);
        }

        return false;
    }

    return true;
}
void SendStrategyMutationAck(
    Player* requester,
    ChatMsg replyType,
    std::string const& scope,
    std::string const& target,
    std::string const& token,
    std::string const& stateScope,
    uint32 matched,
    uint32 succeeded,
    uint32 failed,
    std::string const& reason)
{
    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << stateScope
        << kFieldSeparator << matched
        << kFieldSeparator << succeeded
        << kFieldSeparator << failed
        << kFieldSeparator << UrlEncodeField(reason);

    if (SendStateAddonPacket(requester, replyType, "STRATEGY_ACK", payload.str()))
        return;

    std::ostringstream fallbackPayload;
    fallbackPayload << scope
        << kFieldSeparator
        << kFieldSeparator << token
        << kFieldSeparator << stateScope
        << kFieldSeparator << 0
        << kFieldSeparator << 0
        << kFieldSeparator << 0
        << kFieldSeparator << "ACK_TOO_LONG";
    SendStateAddonPacket(requester, replyType, "STRATEGY_ACK", fallbackPayload.str());
}

void RunStrategyMutationCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& scopeValue,
    std::string const& encodedTarget,
    std::string const& requestToken,
    std::string const& stateScopeValue,
    std::string const& encodedChanges)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string target;
    std::string rawChanges;
    std::string const token = Trim(requestToken);
    std::string const stateScope = ToUpper(Trim(stateScopeValue));
    uint32 matched = 0;
    uint32 succeeded = 0;
    uint32 failed = 0;
    bool botLimitExceeded = false;
    std::string reason = "OK";

    if (!TryUrlDecodeField(encodedTarget, target, kMaxBotNameLength, true) ||
        !TryUrlDecodeField(encodedChanges, rawChanges, kMaxCommandLength, false))
    {
        SendStrategyMutationAck(requester, replyType, scope, "", token, stateScope, 0, 0, 0, "BAD_ENCODING");
        return;
    }

    target = Trim(target);
    std::string normalizedChanges;
    std::vector<StrategyMutationOperation> operations;
    if (!TryNormalizeStrategyChanges(rawChanges, normalizedChanges, operations, reason))
    {
        SendStrategyMutationAck(requester, replyType, scope, target, token, stateScope, 0, 0, 0, reason);
        return;
    }

    if (!ConsumeStrategyMutationRateLimit(requester))
    {
        SendStrategyMutationAck(requester, replyType, scope, target, token, stateScope, 0, 0, 0, "RATE_LIMIT");
        return;
    }

    BotState const botState = stateScope == "C" ? BOT_STATE_COMBAT : BOT_STATE_NON_COMBAT;
    std::string const actionName = stateScope == "C" ? "co" : "nc";

    for (Player* const bot : GetBridgeVisibleBots(requester))
    {
        if (!BotMatchesCombatScope(requester, bot, scope, target))
            continue;

        if (matched >= kMaxStrategyMatchedBots)
        {
            botLimitExceeded = true;
            continue;
        }

        ++matched;
        if (ApplyNativeStrategyMutation(requester, bot, actionName, botState, normalizedChanges, operations))
            ++succeeded;
        else
            ++failed;
    }

    if (matched == 0)
        reason = "NO_MATCH";
    else if (botLimitExceeded)
        reason = "BOT_LIMIT";
    else if (failed > 0 && succeeded > 0)
        reason = "PARTIAL";
    else if (failed > 0)
        reason = "FAILED";

    SendStrategyMutationAck(
        requester,
        replyType,
        scope,
        target,
        token,
        stateScope,
        matched,
        succeeded,
        failed,
        reason);
}

void SendSelfStrategyAck(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    std::string const& stateScope,
    std::string const& status,
    std::string const& reason)
{
    std::ostringstream payload;
    payload << requestToken
        << kFieldSeparator << stateScope
        << kFieldSeparator << status
        << kFieldSeparator << UrlEncodeField(reason);

    if (SendStateAddonPacket(requester, replyType, "SELF_STRATEGY_ACK", payload.str()))
        return;

    std::ostringstream fallbackPayload;
    fallbackPayload << requestToken
        << kFieldSeparator << stateScope
        << kFieldSeparator << "ERR"
        << kFieldSeparator << "ACK_TOO_LONG";
    SendStateAddonPacket(requester, replyType, "SELF_STRATEGY_ACK", fallbackPayload.str());
}

enum class DeferredWarlockStoneStartResult
{
    NotApplicable,
    Started,
    Failed
};

struct PendingWarlockStoneSwitch
{
    std::string token;
    std::string stateScope;
    ChatMsg replyType = CHAT_MSG_WHISPER;
    std::string desiredStone;
    std::map<std::string, bool> priorStrategyStates;
    bool hadFirestoneStrategy = false;
    bool hadSpellstoneStrategy = false;
    std::size_t applyAttempts = 0;
    bool completionScheduled = false;
};

std::map<std::string, PendingWarlockStoneSwitch> sPendingWarlockStoneSwitches;

bool HasNamedWarlockStoneItem(PlayerbotAI* botAI, std::string const& stoneName)
{
    if (!botAI || !botAI->GetAiObjectContext())
        return false;

    std::vector<Item*> const items =
        botAI->GetAiObjectContext()->GetValue<std::vector<Item*>>("inventory items", stoneName)->Get();
    return !items.empty();
}

bool IsTemporaryWeaponEnchantItem(Item* item)
{
    if (!item)
        return false;

    std::set<uint32> enchantIds;
    CollectCarriedWarlockStoneEnchantIds(item, enchantIds);
    return !enchantIds.empty();
}

bool TryGetRequestedWarlockStoneSwitch(
    PlayerbotAI* botAI,
    std::vector<StrategyMutationOperation> const& operations,
    bool& hadFirestoneStrategy,
    bool& hadSpellstoneStrategy,
    std::string& desiredStone)
{
    desiredStone.clear();
    if (!botAI)
        return false;

    hadFirestoneStrategy = botAI->HasStrategy("firestone", BOT_STATE_NON_COMBAT);
    hadSpellstoneStrategy = botAI->HasStrategy("spellstone", BOT_STATE_NON_COMBAT);

    bool hasFirestoneStrategy = hadFirestoneStrategy;
    bool hasSpellstoneStrategy = hadSpellstoneStrategy;

    for (StrategyMutationOperation const& operation : operations)
    {
        if (operation.name == "firestone")
            hasFirestoneStrategy = operation.enable;
        else if (operation.name == "spellstone")
            hasSpellstoneStrategy = operation.enable;
    }

    if (!hadFirestoneStrategy && hadSpellstoneStrategy && hasFirestoneStrategy && !hasSpellstoneStrategy)
        desiredStone = "firestone";
    else if (hadFirestoneStrategy && !hadSpellstoneStrategy && !hasFirestoneStrategy && hasSpellstoneStrategy)
        desiredStone = "spellstone";

    return !desiredStone.empty();
}

bool RollbackPendingWarlockStoneStrategies(Player* player, PendingWarlockStoneSwitch const& pending)
{
    if (!player)
        return false;

    PlayerbotAI* const botAI = GetBotAI(player);
    if (!botAI)
        return false;

    return RollbackNativeStrategyMutation(
        player,
        botAI,
        "nc",
        BOT_STATE_NON_COMBAT,
        pending.priorStrategyStates);
}

void FinishPendingWarlockStoneSwitch(
    Player* player,
    std::string const& key,
    std::string const& token,
    bool success,
    std::string const& reason)
{
    auto const pendingIt = sPendingWarlockStoneSwitches.find(key);
    if (pendingIt == sPendingWarlockStoneSwitches.end() || pendingIt->second.token != token)
        return;

    PendingWarlockStoneSwitch const pending = pendingIt->second;

    if (!success)
    {
        bool const rollbackSucceeded = RollbackPendingWarlockStoneStrategies(player, pending);
        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge deferred warlock stone strategy rollback player={} requested={} reason={} succeeded={}",
                player ? player->GetName() : key,
                pending.desiredStone,
                reason,
                rollbackSucceeded);
        }
    }

    sPendingWarlockStoneSwitches.erase(pendingIt);

    if (player && player->GetSession())
    {
        SendSelfStrategyAck(
            player,
            pending.replyType,
            pending.token,
            pending.stateScope,
            success ? "OK" : "ERR",
            success ? "APPLIED" : reason);
    }
}

void SchedulePendingWarlockStoneCompletion(Player* player, std::string const& key, std::string const& token);

void CompletePendingWarlockStoneSwitch(Player* player, std::string const& key, std::string const& token)
{
    auto pendingIt = sPendingWarlockStoneSwitches.find(key);
    if (pendingIt == sPendingWarlockStoneSwitches.end() || pendingIt->second.token != token)
        return;

    PendingWarlockStoneSwitch& pending = pendingIt->second;
    ++pending.applyAttempts;

    if (!player || !player->GetSession() || !IsSelfBot(player) || player->getClass() != CLASS_WARLOCK)
    {
        FinishPendingWarlockStoneSwitch(player, key, token, false, "STONE_STATE_INVALID");
        return;
    }

    PlayerbotAI* const botAI = GetBotAI(player);
    if (!botAI || !botAI->GetSecurity() ||
        !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, player))
    {
        FinishPendingWarlockStoneSwitch(player, key, token, false, "STONE_NO_AI");
        return;
    }

    if (player->IsInCombat())
    {
        FinishPendingWarlockStoneSwitch(player, key, token, false, "STONE_COMBAT");
        return;
    }

    if (player->IsNonMeleeSpellCast(false) || !HasNamedWarlockStoneItem(botAI, pending.desiredStone))
    {
        if (pending.applyAttempts < kWarlockStoneSwitchMaxApplyAttempts)
        {
            SchedulePendingWarlockStoneCompletion(player, key, token);
            return;
        }

        FinishPendingWarlockStoneSwitch(player, key, token, false, "STONE_ITEM_NOT_READY");
        return;
    }

    WarlockStoneSwitchResult const result = TryForceWarlockStoneSwitch(
        player,
        player,
        botAI,
        BOT_STATE_NON_COMBAT,
        pending.hadFirestoneStrategy,
        pending.hadSpellstoneStrategy);

    if (result == WarlockStoneSwitchResult::Applied)
    {
        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge deferred warlock stone switch applied player={} requested={} attempts={}",
                player->GetName(),
                pending.desiredStone,
                pending.applyAttempts);
        }

        FinishPendingWarlockStoneSwitch(player, key, token, true, "APPLIED");
        return;
    }

    FinishPendingWarlockStoneSwitch(player, key, token, false, "STONE_APPLY_FAILED");
}

void SchedulePendingWarlockStoneCompletion(Player* player, std::string const& key, std::string const& token)
{
    if (!player)
        return;

    player->m_Events.AddEventAtOffset(
        [player, key, token]()
        {
            CompletePendingWarlockStoneSwitch(player, key, token);
        },
        kWarlockStoneSwitchApplyRetryDelay);
}

void TimeoutPendingWarlockStoneSwitch(Player* player, std::string const& key, std::string const& token)
{
    auto const pendingIt = sPendingWarlockStoneSwitches.find(key);
    if (pendingIt == sPendingWarlockStoneSwitches.end() || pendingIt->second.token != token)
        return;

    if (BridgeConsoleLogsEnabled())
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge deferred warlock stone switch timeout player={} requested={}",
            player ? player->GetName() : key,
            pendingIt->second.desiredStone);
    }

    FinishPendingWarlockStoneSwitch(player, key, token, false, "STONE_CREATE_TIMEOUT");
}

void NotifyPendingWarlockStoneItemCreated(Player* player, Item* item)
{
    if (!player || !item || !IsTemporaryWeaponEnchantItem(item))
        return;

    std::string const key = player->GetName();
    auto pendingIt = sPendingWarlockStoneSwitches.find(key);
    if (pendingIt == sPendingWarlockStoneSwitches.end() || pendingIt->second.completionScheduled)
        return;

    pendingIt->second.completionScheduled = true;

    if (BridgeConsoleLogsEnabled())
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge deferred warlock stone item created player={} requested={} itemEntry={}",
            player->GetName(),
            pendingIt->second.desiredStone,
            item->GetEntry());
    }

    SchedulePendingWarlockStoneCompletion(player, key, pendingIt->second.token);
}

void CancelPendingWarlockStoneSwitch(Player* player, std::string const& reason, bool sendAck)
{
    if (!player)
        return;

    std::string const key = player->GetName();
    auto const pendingIt = sPendingWarlockStoneSwitches.find(key);
    if (pendingIt == sPendingWarlockStoneSwitches.end())
        return;

    PendingWarlockStoneSwitch const pending = pendingIt->second;
    bool const rollbackSucceeded = RollbackPendingWarlockStoneStrategies(player, pending);
    sPendingWarlockStoneSwitches.erase(pendingIt);

    if (BridgeConsoleLogsEnabled())
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge deferred warlock stone switch cancelled player={} requested={} reason={} rollback={}",
            player->GetName(),
            pending.desiredStone,
            reason,
            rollbackSucceeded);
    }

    if (sendAck && player->GetSession())
    {
        SendSelfStrategyAck(
            player,
            pending.replyType,
            pending.token,
            pending.stateScope,
            "ERR",
            reason);
    }
}

DeferredWarlockStoneStartResult TryBeginDeferredSelfWarlockStoneSwitch(
    Player* requester,
    ChatMsg replyType,
    std::string const& token,
    std::string const& stateScope,
    std::string const& normalizedChanges,
    std::vector<StrategyMutationOperation> const& operations,
    std::string& failureReason)
{
    if (!requester || stateScope != "N" || requester->getClass() != CLASS_WARLOCK)
        return DeferredWarlockStoneStartResult::NotApplicable;

    PlayerbotAI* const botAI = GetBotAI(requester);
    if (!botAI || !botAI->GetSecurity() ||
        !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
    {
        failureReason = "STONE_NO_AI";
        return DeferredWarlockStoneStartResult::Failed;
    }

    std::string const key = requester->GetName();
    auto const existingPendingIt = sPendingWarlockStoneSwitches.find(key);
    if (existingPendingIt != sPendingWarlockStoneSwitches.end())
    {
        PendingWarlockStoneSwitch const& pending = existingPendingIt->second;
        for (StrategyMutationOperation const& operation : operations)
        {
            if (pending.priorStrategyStates.find(operation.name) != pending.priorStrategyStates.end())
            {
                failureReason = "STONE_SWITCH_PENDING";
                return DeferredWarlockStoneStartResult::Failed;
            }
        }
    }

    bool hadFirestoneStrategy = false;
    bool hadSpellstoneStrategy = false;
    std::string desiredStone;
    if (!TryGetRequestedWarlockStoneSwitch(
        botAI,
        operations,
        hadFirestoneStrategy,
        hadSpellstoneStrategy,
        desiredStone))
    {
        return DeferredWarlockStoneStartResult::NotApplicable;
    }

    Item* const mainHand = requester->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainHand)
        return DeferredWarlockStoneStartResult::NotApplicable;

    uint32 const currentEnchantId = mainHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
    if (!currentEnchantId)
        return DeferredWarlockStoneStartResult::NotApplicable;

    std::set<uint32> const carriedStoneEnchantIds = GetCarriedWarlockStoneEnchantIds(requester);
    if (carriedStoneEnchantIds.find(currentEnchantId) == carriedStoneEnchantIds.end())
        return DeferredWarlockStoneStartResult::NotApplicable;

    if (HasNamedWarlockStoneItem(botAI, desiredStone))
        return DeferredWarlockStoneStartResult::NotApplicable;

    if (requester->IsInCombat())
    {
        failureReason = "STONE_COMBAT";
        return DeferredWarlockStoneStartResult::Failed;
    }

    if (sPendingWarlockStoneSwitches.size() >= kWarlockStoneSwitchMaxPending)
    {
        failureReason = "STONE_PENDING_LIMIT";
        return DeferredWarlockStoneStartResult::Failed;
    }

    std::map<std::string, bool> const priorStrategyStates =
        CaptureStrategyMutationState(botAI, BOT_STATE_NON_COMBAT, operations);

    if (!botAI->DoSpecificAction("nc", Event("nc", normalizedChanges, requester), true) ||
        !VerifyStrategyMutationResult(botAI, BOT_STATE_NON_COMBAT, operations))
    {
        failureReason = "STONE_STRATEGY_FAILED";
        return DeferredWarlockStoneStartResult::Failed;
    }

    PendingWarlockStoneSwitch pending;
    pending.token = token;
    pending.stateScope = stateScope;
    pending.replyType = replyType;
    pending.desiredStone = desiredStone;
    pending.priorStrategyStates = priorStrategyStates;
    pending.hadFirestoneStrategy = hadFirestoneStrategy;
    pending.hadSpellstoneStrategy = hadSpellstoneStrategy;

    sPendingWarlockStoneSwitches.emplace(key, pending);

    requester->m_Events.AddEventAtOffset(
        [requester, key, token]()
        {
            TimeoutPendingWarlockStoneSwitch(requester, key, token);
        },
        kWarlockStoneSwitchCreateTimeout);

    std::string const createAction = "create " + desiredStone;
    if (!botAI->DoSpecificAction(createAction, Event(), true))
    {
        PendingWarlockStoneSwitch const failedPending = sPendingWarlockStoneSwitches[key];
        sPendingWarlockStoneSwitches.erase(key);
        bool const rollbackSucceeded = RollbackPendingWarlockStoneStrategies(requester, failedPending);

        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge deferred warlock stone create failed player={} requested={} rollback={}",
                requester->GetName(),
                desiredStone,
                rollbackSucceeded);
        }

        failureReason = "STONE_CREATE_FAILED";
        return DeferredWarlockStoneStartResult::Failed;
    }

    if (BridgeConsoleLogsEnabled())
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge deferred warlock stone create started player={} requested={} token={}",
            requester->GetName(),
            desiredStone,
            token);
    }

    return DeferredWarlockStoneStartResult::Started;
}


bool IsAllowedSelfCombatStrategyForClass(Player* requester, std::string const& name)
{
    if (name == "dps assist"
        || name == "dps aoe"
        || name == "tank assist"
        || name == "avoid aoe"
        || name == "save mana"
        || name == "threat"
        || name == "behind"
        || name == "focus")
    {
        return true;
    }

    if (!requester)
        return false;

    switch (requester->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            return name == "blood"
                || name == "frost"
                || name == "frost aoe"
                || name == "tank face"
                || name == "unholy"
                || name == "unholy aoe";
        case CLASS_DRUID:
            return name == "aoe"
                || name == "balance"
                || name == "bear"
                || name == "cat"
                || name == "healer dps"
                || name == "offheal"
                || name == "resto"
                || name == "tank face";
        case CLASS_HUNTER:
            return name == "aoe"
                || name == "bdps"
                || name == "bm"
                || name == "bspeed"
                || name == "mm"
                || name == "rnature"
                || name == "surv"
                || name == "trap weave";
        case CLASS_MAGE:
            return name == "aoe"
                || name == "arcane"
                || name == "fire"
                || name == "firestarter"
                || name == "frost"
                || name == "frostfire";
        case CLASS_PALADIN:
            return name == "baoe"
                || name == "barmor"
                || name == "bcast"
                || name == "bspeed"
                || name == "dps"
                || name == "heal"
                || name == "healer dps"
                || name == "offheal"
                || name == "rfire"
                || name == "rfrost"
                || name == "rshadow"
                || name == "tank"
                || name == "tank face";
        case CLASS_PRIEST:
            return name == "heal"
                || name == "healer dps"
                || name == "holy dps"
                || name == "holy heal"
                || name == "shadow"
                || name == "shadow aoe"
                || name == "shadow debuff";
        case CLASS_ROGUE:
            return name == "boost"
                || name == "dps"
                || name == "stealthed";
        case CLASS_SHAMAN:
            return name == "aoe"
                || name == "cleansing"
                || name == "cure"
                || name == "earthbind"
                || name == "ele"
                || name == "enh"
                || name == "fire resistance"
                || name == "flametongue"
                || name == "frost resistance"
                || name == "grounding"
                || name == "healer dps"
                || name == "healing stream"
                || name == "magma"
                || name == "mana spring"
                || name == "nature resistance"
                || name == "resto"
                || name == "searing"
                || name == "stoneskin"
                || name == "strength of earth"
                || name == "tremor"
                || name == "windfury"
                || name == "wrath"
                || name == "wrath of air";
        case CLASS_WARLOCK:
            return name == "curse of agony"
                || name == "curse of doom"
                || name == "curse of elements"
                || name == "curse of exhaustion"
                || name == "curse of tongues"
                || name == "curse of weakness"
                || name == "meta melee"
                || name == "tank";
        case CLASS_WARRIOR:
            return name == "tank"
                || name == "tank face";
        default:
            return false;
    }
}

bool IsAllowedSelfStrategyFoundationMutation(
    Player* requester,
    BotState botState,
    std::vector<StrategyMutationOperation> const& operations,
    std::string& reason)
{
    if (botState == BOT_STATE_NON_COMBAT)
    {
        for (StrategyMutationOperation const& operation : operations)
        {
            bool allowed = operation.name == "food"
                || operation.name == "loot"
                || operation.name == "gather"
                || (operation.name == "mount" && !operation.enable);

            if (!allowed && requester)
            {
                switch (requester->getClass())
                {
                    case CLASS_DRUID:
                        allowed = operation.name == "buff";
                        break;
                    case CLASS_HUNTER:
                        allowed = operation.name == "rnature"
                            || operation.name == "bspeed"
                            || operation.name == "bdps";
                        break;
                    case CLASS_MAGE:
                        allowed = operation.name == "bmana"
                            || operation.name == "bdps";
                        break;
                    case CLASS_PALADIN:
                        allowed = operation.name == "bsanc"
                            || operation.name == "bwisdom"
                            || operation.name == "bkings"
                            || operation.name == "bmight"
                            || operation.name == "bspeed"
                            || operation.name == "rfire"
                            || operation.name == "rfrost"
                            || operation.name == "rshadow"
                            || operation.name == "baoe"
                            || operation.name == "barmor"
                            || operation.name == "bcast";
                        break;
                    case CLASS_PRIEST:
                        allowed = operation.name == "buff"
                            || operation.name == "rshadow";
                        break;
                    case CLASS_ROGUE:
                        allowed = operation.name == "stealth";
                        break;
                    default:
                        break;
                }
            }
            if (!allowed && requester && requester->getClass() == CLASS_WARLOCK)
            {
                allowed = operation.name == "imp"
                    || operation.name == "voidwalker"
                    || operation.name == "succubus"
                    || operation.name == "felhunter"
                    || operation.name == "felguard"
                    || operation.name == "ss self"
                    || operation.name == "ss master"
                    || operation.name == "ss tank"
                    || operation.name == "ss healer"
                    || operation.name == "spellstone"
                    || operation.name == "firestone";
            }

            if (!allowed)
            {
                reason = "UNSUPPORTED_STRATEGY";
                return false;
            }
        }

        return true;
    }

    if (botState != BOT_STATE_COMBAT)
    {
        reason = "UNSUPPORTED_STATE";
        return false;
    }

    for (StrategyMutationOperation const& operation : operations)
    {
        if (!IsAllowedSelfCombatStrategyForClass(requester, operation.name))
        {
            reason = "UNSUPPORTED_STRATEGY";
            return false;
        }
    }

    return true;
}

void SendSelfActionAck(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    std::string const& action,
    bool ok,
    std::string const& reason)
{
    if (!requester)
        return;

    std::ostringstream payload;
    payload << requestToken
        << kFieldSeparator << action
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason);

    if (SendStateAddonPacket(requester, replyType, "SELF_ACTION_ACK", payload.str()))
        return;

    std::ostringstream fallbackPayload;
    fallbackPayload << requestToken
        << kFieldSeparator << action
        << kFieldSeparator << "ERR"
        << kFieldSeparator << "ACK_TOO_LONG";
    SendStateAddonPacket(requester, replyType, "SELF_ACTION_ACK", fallbackPayload.str());
}

void RunSelfActionCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    std::string const& actionValue,
    std::string const& argumentValue)
{
    std::string const token = Trim(requestToken);
    std::string const action = ToUpper(Trim(actionValue));
    std::string const argument = Trim(argumentValue);
    bool ok = false;
    std::string reason = "BAD_REQUEST";

    if (!requester || !requester->GetSession())
        reason = "NO_SESSION";
    else if (!IsSelfBot(requester))
        reason = "NOT_SELF_BOT";
    else if (!ConsumeSelfBotRequestRateLimit(requester))
        reason = "RATE_LIMIT";
    else if ((action == "AUTOGEAR" || action == "MAINTENANCE") &&
             !ConsumeSelfBotHeavyActionRateLimit(requester))
        reason = "RATE_LIMIT";
    else
    {
        PlayerbotAI* const botAI = GetBotAI(requester);
        if (!botAI)
            reason = "NO_AI";
        else if (!botAI->GetSecurity() ||
                 !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            reason = "FORBIDDEN";
        else if (action == "WAIT_ATTACK_TIME")
        {
            if (argument != "0" && argument != "3" && argument != "5" && argument != "10")
                reason = "BAD_ARGUMENT";
            else if (!botAI->GetAiObjectContext())
                reason = "NO_CONTEXT";
            else
            {
                uint8 const value = static_cast<uint8>(std::strtoul(argument.c_str(), nullptr, 10));
                botAI->GetAiObjectContext()->GetValue<uint8>("wait for attack time")->Set(value);
                ok = true;
                reason = "APPLIED";
            }
        }
        else if (action == "AUTOGEAR")
        {
            if (!argument.empty())
                reason = "BAD_ARGUMENT";
            else if (!sPlayerbotAIConfig.autoGearCommand)
                reason = "DISABLED";
            else if (!sPlayerbotAIConfig.autoGearCommandAltBots &&
                     !sPlayerbotAIConfig.IsInRandomAccountList(requester->GetSession()->GetAccountId()))
                reason = "ALT_BOT_REFUSED";
            else
            {
                uint32 const quality = static_cast<uint32>(sPlayerbotAIConfig.autoGearQualityLimit);
                uint32 const ilvl = static_cast<uint32>(sPlayerbotAIConfig.autoGearScoreLimit);
                PlayerbotFactory::AutoGear(requester, quality, ilvl, true);
                ok = true;
                reason = "APPLIED";
            }
        }
        else if (action == "MAINTENANCE")
        {
            if (!argument.empty())
                reason = "BAD_ARGUMENT";
            else if (!sPlayerbotAIConfig.maintenanceCommand)
                reason = "DISABLED";
            else
            {
                PlayerbotFactory factory(requester, requester->GetLevel());

                if (!botAI->IsAltBot())
                {
                    factory.InitAttunementQuests();
                    factory.InitBags(false);
                    factory.InitAmmo();
                    factory.InitFood();
                    factory.InitReagents();
                    factory.InitConsumables();
                    factory.InitPotions();
                    factory.InitTalentsTree(true);
                    factory.InitPet();
                    factory.InitPetTalents();
                    factory.InitSkills();
                    factory.InitClassSpells();
                    factory.InitAvailableSpells();
                    factory.InitReputation();
                    factory.InitSpecialSpells();
                    factory.InitMounts();
                    factory.InitGlyphs(false);
                    factory.InitKeyring();
                    if (requester->GetLevel() >= sPlayerbotAIConfig.minEnchantingBotLevel)
                        factory.ApplyEnchantAndGemsNew();
                }
                else
                {
                    if (sPlayerbotAIConfig.altMaintenanceAttunementQs)
                        factory.InitAttunementQuests();
                    if (sPlayerbotAIConfig.altMaintenanceBags)
                        factory.InitBags(false);
                    if (sPlayerbotAIConfig.altMaintenanceAmmo)
                        factory.InitAmmo();
                    if (sPlayerbotAIConfig.altMaintenanceFood)
                        factory.InitFood();
                    if (sPlayerbotAIConfig.altMaintenanceReagents)
                        factory.InitReagents();
                    if (sPlayerbotAIConfig.altMaintenanceConsumables)
                        factory.InitConsumables();
                    if (sPlayerbotAIConfig.altMaintenancePotions)
                        factory.InitPotions();
                    if (sPlayerbotAIConfig.altMaintenanceTalentTree)
                        factory.InitTalentsTree(true);
                    if (sPlayerbotAIConfig.altMaintenancePet)
                        factory.InitPet();
                    if (sPlayerbotAIConfig.altMaintenancePetTalents)
                        factory.InitPetTalents();
                    if (sPlayerbotAIConfig.altMaintenanceSkills)
                        factory.InitSkills();
                    if (sPlayerbotAIConfig.altMaintenanceClassSpells)
                        factory.InitClassSpells();
                    if (sPlayerbotAIConfig.altMaintenanceAvailableSpells)
                        factory.InitAvailableSpells();
                    if (sPlayerbotAIConfig.altMaintenanceReputation)
                        factory.InitReputation();
                    if (sPlayerbotAIConfig.altMaintenanceSpecialSpells)
                        factory.InitSpecialSpells();
                    if (sPlayerbotAIConfig.altMaintenanceMounts)
                        factory.InitMounts();
                    if (sPlayerbotAIConfig.altMaintenanceGlyphs)
                        factory.InitGlyphs(false);
                    if (sPlayerbotAIConfig.altMaintenanceKeyring)
                        factory.InitKeyring();
                    if (sPlayerbotAIConfig.altMaintenanceGemsEnchants &&
                        requester->GetLevel() >= sPlayerbotAIConfig.minEnchantingBotLevel)
                        factory.ApplyEnchantAndGemsNew();
                }

                requester->DurabilityRepairAll(false, 1.0f, false);
                requester->SendTalentsInfoData(false);
                ok = true;
                reason = "APPLIED";
            }
        }
        else
            reason = "UNSUPPORTED_ACTION";
    }

    SendSelfActionAck(requester, replyType, token, action, ok, reason);
}
void RunSelfStrategyMutationCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    std::string const& stateScopeValue,
    std::string const& encodedChanges)
{
    std::string const token = Trim(requestToken);
    std::string const stateScope = ToUpper(Trim(stateScopeValue));
    std::string status = "ERR";
    std::string reason = "UNKNOWN";

    if (!requester || !requester->GetSession())
        reason = "NO_SESSION";
    else if (stateScope != "C" && stateScope != "N")
        reason = "BAD_STATE";
    else
    {
        std::string rawChanges;
        if (!TryUrlDecodeField(encodedChanges, rawChanges, kMaxCommandLength, false))
            reason = "BAD_ENCODING";
        else
        {
            std::string normalizedChanges;
            std::vector<StrategyMutationOperation> operations;
            if (!TryNormalizeStrategyChanges(rawChanges, normalizedChanges, operations, reason))
            {
                // reason already set by the shared strict normalizer.
            }
            else
            {
                BotState const botState = stateScope == "C" ? BOT_STATE_COMBAT : BOT_STATE_NON_COMBAT;
                std::string const actionName = stateScope == "C" ? "co" : "nc";

                if (!IsAllowedSelfStrategyFoundationMutation(requester, botState, operations, reason))
                {
                    // SelfBot mutations remain restricted by the state- and class-aware server allowlist.
                }
                else if (!ConsumeStrategyMutationRateLimit(requester))
                    reason = "RATE_LIMIT";
                else if (!IsSelfBot(requester))
                    reason = "NOT_SELF_BOT";
                else if (!GET_PLAYERBOT_AI(requester))
                    reason = "NO_AI";
                else
                {
                    std::string deferredFailureReason;
                    DeferredWarlockStoneStartResult const deferredResult =
                        TryBeginDeferredSelfWarlockStoneSwitch(
                            requester,
                            replyType,
                            token,
                            stateScope,
                            normalizedChanges,
                            operations,
                            deferredFailureReason);

                    if (deferredResult == DeferredWarlockStoneStartResult::Started)
                        return;
                    if (deferredResult == DeferredWarlockStoneStartResult::Failed)
                        reason = deferredFailureReason.empty() ? "FAILED" : deferredFailureReason;
                    else if (ApplyNativeStrategyMutation(
                        requester, requester, actionName, botState, normalizedChanges, operations))
                    {
                        status = "OK";
                        reason = "APPLIED";
                    }
                    else
                        reason = "FAILED";
                }
            }
        }
    }

    SendSelfStrategyAck(requester, replyType, token, stateScope, status, reason);
}
bool IsAllowedFormationName(std::string const& formation)
{
    static std::set<std::string> const allowed =
    {
        "arrow",
        "queue",
        "near",
        "melee",
        "line",
        "circle",
        "chaos",
        "shield"
    };

    return allowed.find(formation) != allowed.end();
}

void SendFormationPackets(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);

    std::vector<std::pair<std::string, std::string>> entries;

    if (requester && scope == "GROUP" && target.empty() && !token.empty() && token.size() <= 64)
    {
        Group* const requesterGroup = requester->GetGroup();
        if (requesterGroup)
        {
            for (Player* const bot : GetBridgeVisibleBots(requester))
            {
                if (!bot || bot->GetGroup() != requesterGroup)
                    continue;

                std::string formation = "?";

                PlayerbotAI* const botAI = GetBotAI(bot);
                if (botAI && botAI->GetAiObjectContext())
                {
                    AiObjectContext* const context = botAI->GetAiObjectContext();
                    FormationValue* const value = (FormationValue*)context->GetValue<Formation*>("formation");
                    if (value)
                    {
                        formation = Trim(value->Save());
                        if (formation.empty())
                            formation = "?";
                    }
                }

                entries.emplace_back(bot->GetName(), formation);
            }
        }
    }

    std::sort(entries.begin(), entries.end(), [](std::pair<std::string, std::string> const& left, std::pair<std::string, std::string> const& right)
    {
        return left.first < right.first;
    });

    std::ostringstream beginPayload;
    beginPayload << token << kFieldSeparator << entries.size();
    SendAddonPacket(requester, replyType, "FORMATIONS_BEGIN", beginPayload.str());

    uint32 sent = 0;
    for (std::pair<std::string, std::string> const& entry : entries)
    {
        std::ostringstream itemPayload;
        itemPayload << token
            << kFieldSeparator << UrlEncodeField(entry.first)
            << kFieldSeparator << UrlEncodeField(entry.second);

        SendAddonPacket(requester, replyType, "FORMATIONS_ITEM", itemPayload.str());
        ++sent;
    }

    std::ostringstream endPayload;
    endPayload << token << kFieldSeparator << sent;
    SendAddonPacket(requester, replyType, "FORMATIONS_END", endPayload.str());
}

bool ApplyNativeFormation(Player* bot, std::string const& formation)
{
    if (!bot || !IsAllowedFormationName(formation))
        return false;

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
        return false;

    AiObjectContext* const context = botAI->GetAiObjectContext();
    if (!context)
        return false;

    FormationValue* const value = static_cast<FormationValue*>(context->GetValue<Formation*>("formation"));
    if (!value || !value->Load(formation))
        return false;

    return value->Save() == formation;
}

void RunFormationCommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& encodedFormation)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);
    std::string const formation = ToLower(Trim(UrlDecodeField(encodedFormation)));
    uint32 succeeded = 0;
    uint32 failed = 0;

    bool const validRequest =
        requester &&
        scope == "GROUP" &&
        target.empty() &&
        !token.empty() &&
        token.size() <= 64 &&
        formation.size() <= 16 &&
        IsAllowedFormationName(formation);

    Group* const requesterGroup = validRequest ? requester->GetGroup() : nullptr;
    if (requesterGroup)
    {
        for (Player* const bot : GetBridgeVisibleBots(requester))
        {
            if (!bot || bot->GetGroup() != requesterGroup)
                continue;

            if (ApplyNativeFormation(bot, formation))
                ++succeeded;
            else
                ++failed;
        }
    }

    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << succeeded
        << kFieldSeparator << failed
        << kFieldSeparator << UrlEncodeField(formation);

    SendAddonPacket(requester, replyType, "FORMATION_ACK", payload.str());
}

void RunRTICommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& encodedCommand)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);
    std::string const rawCommand = Trim(UrlDecodeField(encodedCommand));
    std::string const command = NormalizeCombatCommand(rawCommand);
    uint32 executed = 0;

    if (IsAllowedRTICommand(command) && (scope == "ALL" || scope == "GROUP" || scope == "BOT"))
    {
        for (Player* const bot : GetBridgeVisibleBots(requester))
        {
            if (!BotMatchesRTIScope(requester, bot, scope, target))
                continue;

            if (ExecuteSilentBotCommand(requester, bot, command))
                ++executed;
        }
    }

    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << executed
        << kFieldSeparator << UrlEncodeField(command);

    SendAddonPacket(requester, replyType, "RTI_ACK", payload.str());
}

void RunCombatCommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& encodedCommand)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);
    std::string const rawCommand = Trim(UrlDecodeField(encodedCommand));
    std::string const command = NormalizeCombatCommand(rawCommand);
    uint32 executed = 0;

    if (IsAllowedCombatCommand(command) && (scope == "ALL" || scope == "RAID" || scope == "GROUP" || scope == "PARTY" || scope == "BOT"))
    {
        for (Player* const bot : GetBridgeVisibleBots(requester))
        {
            if (!BotMatchesCombatScope(requester, bot, scope, target))
                continue;

            if (ExecuteSilentBotCommand(requester, bot, command))
                ++executed;
        }
    }

    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << executed
        << kFieldSeparator << UrlEncodeField(command);

    SendAddonPacket(requester, replyType, "COMBAT_ACK", payload.str());
}

void RunPositionCommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& encodedCommand)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);
    std::string const rawCommand = Trim(UrlDecodeField(encodedCommand));
    std::string const command = NormalizePositionCommand(rawCommand);
    uint32 executed = 0;

    if (IsAllowedPositionCommand(command) && (scope == "ALL" || scope == "RAID" || scope == "GROUP" || scope == "PARTY" || scope == "BOT"))
    {
        for (Player* const bot : GetBridgeVisibleBots(requester))
        {
            if (!BotMatchesCombatScope(requester, bot, scope, target))
                continue;

            if (ApplyNativeDisperseCommand(bot, command))
                ++executed;
        }
    }

    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << executed
        << kFieldSeparator << UrlEncodeField(command);

    SendAddonPacket(requester, replyType, "POSITION_ACK", payload.str());
}

// MB_LOOT_RULE_ITEM_V1_BEGIN
void RunLootRuleItemCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& scopeValue,
    std::string const& encodedTarget,
    std::string const& requestToken,
    std::string const& actionValue,
    uint32 itemId)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);
    std::string const action = ToUpper(Trim(actionValue));
    std::string reason;
    uint32 matched = 0;
    uint32 changed = 0;
    bool ok = false;

    if (!ConsumeLootRuleItemRateLimit(requester))
        reason = "RATE_LIMIT";
    else if (!requester || !requester->GetSession())
        reason = "NO_REQUESTER_SESSION";
    else if (!requester->IsInWorld())
        reason = "REQUESTER_NOT_IN_WORLD";
    else if (!RegisterLootRuleItemToken(requester, token))
        reason = "DUPLICATE";
    else if (action != "ADD" && action != "REMOVE")
        reason = "BAD_ACTION";
    else if (!sObjectMgr->GetItemTemplate(itemId))
        reason = "INVALID_ITEM";
    else
    {
        std::vector<PlayerbotAI*> selected;
        for (Player* const bot : GetBridgeVisibleBots(requester))
        {
            if (!BotMatchesCombatScope(requester, bot, scope, target))
                continue;

            if (selected.size() >= kLootRuleItemMaxMatchedBots)
            {
                reason = "TOO_MANY_BOTS";
                break;
            }

            PlayerbotAI* const botAI = GetBotAI(bot);
            if (!botAI || !botAI->GetSecurity() ||
                !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
            {
                reason = "FORBIDDEN";
                break;
            }
            if (!bot->GetSession())
            {
                reason = "NO_BOT_SESSION";
                break;
            }
            if (!bot->IsInWorld())
            {
                reason = "BOT_NOT_IN_WORLD";
                break;
            }
            if (!bot->IsAlive())
            {
                reason = "BOT_DEAD";
                break;
            }
            if (!botAI->GetAiObjectContext())
            {
                reason = "NO_BOT_CONTEXT";
                break;
            }

            selected.push_back(botAI);
        }

        if (reason.empty() && selected.empty())
            reason = "NO_BOTS";
        else if (reason.empty())
        {
            matched = static_cast<uint32>(selected.size());
            std::vector<PlayerbotAI*> changedBots;
            changedBots.reserve(selected.size());

            for (PlayerbotAI* const botAI : selected)
            {
                AiObjectContext* const context = botAI->GetAiObjectContext();
                std::set<uint32>& alwaysLootItems =
                    context->GetValue<std::set<uint32>&>("always loot list")->Get();

                bool const needsChange =
                    action == "ADD"
                        ? alwaysLootItems.find(itemId) == alwaysLootItems.end()
                        : alwaysLootItems.find(itemId) != alwaysLootItems.end();
                if (needsChange)
                    changedBots.push_back(botAI);
            }

            if (!ReserveLootRuleItemPersistenceBudget(changedBots.size()))
                reason = "PERSISTENCE_BUSY";
            else
            {
                changed = static_cast<uint32>(changedBots.size());
                for (PlayerbotAI* const botAI : changedBots)
                {
                    AiObjectContext* const context = botAI->GetAiObjectContext();
                    std::set<uint32>& alwaysLootItems =
                        context->GetValue<std::set<uint32>&>("always loot list")->Get();

                    if (action == "ADD")
                        alwaysLootItems.insert(itemId);
                    else
                        alwaysLootItems.erase(itemId);

                    PlayerbotRepository::instance().Save(botAI);
                }

                ok = true;
                if (action == "ADD")
                    reason = changed == matched ? "ADDED" : (changed == 0 ? "ALREADY_PRESENT" : "PARTIAL");
                else
                    reason = changed == matched ? "REMOVED" : (changed == 0 ? "ALREADY_ABSENT" : "PARTIAL");
            }
        }
    }

    if (!ok)
    {
        matched = 0;
        changed = 0;
        if (reason.empty())
            reason = "FAILED";
    }

    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << action
        << kFieldSeparator << itemId
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason)
        << kFieldSeparator << matched
        << kFieldSeparator << changed;

    SendAddonPacket(requester, replyType, "LOOT_RULE_ITEM_RESULT", payload.str());
}
// MB_LOOT_RULE_ITEM_V1_END
void RunLootCommand(Player* requester, ChatMsg replyType, std::string const& scopeValue, std::string const& encodedTarget, std::string const& requestToken, std::string const& encodedCommand)
{
    std::string const scope = ToUpper(Trim(scopeValue));
    std::string const target = Trim(UrlDecodeField(encodedTarget));
    std::string const token = Trim(requestToken);
    std::string const rawCommand = Trim(UrlDecodeField(encodedCommand));
    std::string const command = NormalizeLootCommand(rawCommand);
    uint32 executed = 0;

    if (IsAllowedLootCommand(command) && (scope == "ALL" || scope == "RAID" || scope == "GROUP" || scope == "PARTY" || scope == "BOT"))
    {
        for (Player* const bot : GetBridgeVisibleBots(requester))
        {
            if (!BotMatchesCombatScope(requester, bot, scope, target))
                continue;

            if (ExecuteSilentBotCommand(requester, bot, command))
                ++executed;
        }
    }

    std::ostringstream payload;
    payload << scope
        << kFieldSeparator << UrlEncodeField(target)
        << kFieldSeparator << token
        << kFieldSeparator << executed
        << kFieldSeparator << UrlEncodeField(command);

    SendAddonPacket(requester, replyType, "LOOT_ACK", payload.str());
}

ChatMsg NormalizeReplyChatType(uint32 type)
{
    switch (type)
    {
        case CHAT_MSG_PARTY:
        case CHAT_MSG_RAID:
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        case CHAT_MSG_WHISPER:
        case CHAT_MSG_CHANNEL:
            return static_cast<ChatMsg>(type);
        default:
            return CHAT_MSG_WHISPER;
    }
}

void SendAddonPacket(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& payload)
{
    if (!player || !player->GetSession())
        return;

    std::string wire = std::string(kAddonEnvelope) + opcode;
    if (!payload.empty())
        wire += std::string(1, kFieldSeparator) + payload;

    if (BridgeConsoleLogsEnabled())
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge TX player={} opcode={} payloadBytes={} wireBytes={} type={}",
            player->GetName(),
            SanitizeLogValue(opcode, kMaxOpcodeLength),
            payload.size(),
            wire.size(),
            static_cast<uint32>(chatType));
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, chatType, LANG_ADDON, player, nullptr, wire.c_str());
    player->SendDirectMessage(&data);
}

bool SendStateAddonPacket(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& payload)
{
    if (!player || !player->GetSession())
        return false;

    std::size_t const wireLength = GetAddonWireLength(opcode, payload);
    if (wireLength > kMaxBridgeWireLength)
    {
        LOG_INFO(
            "playerbots",
            "MultiBotBridge STATE TX rejected player={} opcode={} payloadBytes={} wireBytes={} maxWireBytes={}",
            player->GetName(),
            SanitizeLogValue(opcode, kMaxOpcodeLength),
            payload.size(),
            wireLength,
            kMaxBridgeWireLength);
        return false;
    }

    SendAddonPacket(player, chatType, opcode, payload);
    return true;
}

bool SendProtocolError(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& requestType, std::string const& token, std::string const& reason)
{
    std::string const safeOpcode = IsValidProtocolName(opcode, kMaxOpcodeLength) ? ToUpper(opcode) : "";
    std::string const safeRequestType = IsValidProtocolName(requestType, kMaxRequestTypeLength) ? ToUpper(requestType) : "";
    std::string const safeToken = IsValidRequestToken(token) ? token : "";

    std::ostringstream payload;
    payload << UrlEncodeField(safeOpcode)
        << kFieldSeparator << UrlEncodeField(safeRequestType)
        << kFieldSeparator << safeToken
        << kFieldSeparator << UrlEncodeField(reason);

    SendAddonPacket(player, chatType, "ERR", payload.str());
    return true;
}

uint32 GetPct(uint32 current, uint32 max)
{
    if (!max)
        return 0;

    return static_cast<uint32>((current * 100u) / max);
}

bool IsBotInRequesterGroup(Player* requester, Player* bot)
{
    if (!requester || !bot)
        return false;

    Group* const group = requester->GetGroup();
    return group && bot->GetGroup() == group;
}

bool IsBotMasteredByRequester(Player* requester, Player* bot)
{
    if (!requester || !bot)
        return false;

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    return botAI && botAI->GetMaster() == requester;
}

bool CanExposeRandomHolderBot(Player* requester, Player* bot)
{
    if (!requester || !bot)
        return false;

    if (!sPlayerbotsMgr.GetPlayerbotAI(bot))
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot) && !sRandomPlayerbotMgr.IsAddclassBot(bot))
        return false;

    return IsBotMasteredByRequester(requester, bot) || IsBotInRequesterGroup(requester, bot);
}

void AppendBridgeVisibleBot(Player* bot, std::vector<Player*>& bots, std::set<ObjectGuid>& seen)
{
    if (!bot || !bot->IsInWorld())
        return;

    if (!seen.insert(bot->GetGUID()).second)
        return;

    bots.push_back(bot);
}

std::vector<Player*> GetBridgeVisibleBots(Player* player)
{
    std::vector<Player*> bots;
    std::set<ObjectGuid> seen;

    if (!player)
        return bots;

    if (PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player))
        for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
            AppendBridgeVisibleBot(it->second, bots, seen);

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (CanExposeRandomHolderBot(player, bot))
            AppendBridgeVisibleBot(bot, bots, seen);
    }

    return bots;
}

Player* FindBotByName(Player* player, std::string const& botName)
{
    std::string const wantedName = Trim(botName);
    if (wantedName.empty())
        return nullptr;

    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        if (bot->GetName() == wantedName)
            return bot;
    }

    return nullptr;
}

bool ConsumeWeaponEnchantDebugRateLimit(Player* requester)
{
    if (!requester)
        return false;

    static std::map<std::string, std::chrono::steady_clock::time_point> lastRequests;
    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    std::string const key = requester->GetName();

    auto const existing = lastRequests.find(key);
    if (existing != lastRequests.end() && now - existing->second < std::chrono::milliseconds(500))
        return false;

    lastRequests[key] = now;

    if (lastRequests.size() > 512)
    {
        for (auto it = lastRequests.begin(); it != lastRequests.end();)
        {
            if (it->first != key && now - it->second >= std::chrono::seconds(60))
                it = lastRequests.erase(it);
            else
                ++it;
        }
    }

    return true;
}

void SendWeaponEnchantDebugPacket(
    Player* requester,
    ChatMsg replyType,
    std::string const& botName,
    std::string const& token)
{
    std::string status = "OK";
    uint32 mainItem = 0;
    uint32 mainEnchant = 0;
    uint32 mainDuration = 0;
    uint32 offItem = 0;
    uint32 offEnchant = 0;
    uint32 offDuration = 0;

    Player* const bot = FindBotByName(requester, botName);
    if (!ConsumeWeaponEnchantDebugRateLimit(requester))
    {
        status = "RATE_LIMIT";
    }
    else if (!bot)
    {
        status = "BOT_NOT_VISIBLE";
    }
    else
    {
        PlayerbotAI* const botAI = GetBotAI(bot);
        if (!botAI || !botAI->GetSecurity() ||
            !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, requester))
        {
            status = "FORBIDDEN";
        }
        else
        {
            Item* const mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            if (mainHand)
            {
                mainItem = mainHand->GetEntry();
                mainEnchant = mainHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
                mainDuration = mainHand->GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT);
            }

            Item* const offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
            if (offHand)
            {
                offItem = offHand->GetEntry();
                offEnchant = offHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
                offDuration = offHand->GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT);
            }
        }
    }

    std::ostringstream payload;
    payload << token
        << kFieldSeparator << UrlEncodeField(bot ? bot->GetName() : Trim(botName))
        << kFieldSeparator << status
        << kFieldSeparator << mainItem
        << kFieldSeparator << mainEnchant
        << kFieldSeparator << mainDuration
        << kFieldSeparator << offItem
        << kFieldSeparator << offEnchant
        << kFieldSeparator << offDuration;

    SendAddonPacket(requester, replyType, "WEAPON_ENCHANT", payload.str());
}

std::string JoinStrategies(std::vector<std::string> const& strategies)
{
    std::ostringstream out;

    for (size_t index = 0; index < strategies.size(); ++index)
    {
        if (index)
            out << ", ";

        out << strategies[index];
    }

    return out.str();
}

// MB_ALT_ROSTER_DISCOVERY_V1_BEGIN
using AltRosterRateClock = std::chrono::steady_clock;
std::map<uint32, std::deque<AltRosterRateClock::time_point>> gAltRosterRequestRateStates;

bool ConsumeAltRosterRequestRateLimit(Player* requester)
{
    if (!requester)
        return false;

    AltRosterRateClock::time_point const now = AltRosterRateClock::now();
    uint32 const requesterKey = requester->GetGUID().GetCounter();

    auto pruneQueue = [now](std::deque<AltRosterRateClock::time_point>& attempts)
    {
        while (!attempts.empty() && now - attempts.front() >= kAltRosterRateWindow)
            attempts.pop_front();
    };

    auto it = gAltRosterRequestRateStates.find(requesterKey);
    if (it == gAltRosterRequestRateStates.end())
    {
        for (auto existing = gAltRosterRequestRateStates.begin(); existing != gAltRosterRequestRateStates.end();)
        {
            pruneQueue(existing->second);
            if (existing->second.empty())
                existing = gAltRosterRequestRateStates.erase(existing);
            else
                ++existing;
        }

        if (gAltRosterRequestRateStates.size() >= kAltRosterMaxRequesterStates)
            return false;

        it = gAltRosterRequestRateStates.emplace(
            requesterKey, std::deque<AltRosterRateClock::time_point>()).first;
    }

    std::deque<AltRosterRateClock::time_point>& attempts = it->second;
    pruneQueue(attempts);
    if (attempts.size() >= kAltRosterRateLimit)
        return false;

    attempts.push_back(now);
    return true;
}

void SendAltRosterPackets(Player* requester, ChatMsg replyType)
{
    if (!requester || !requester->GetSession())
        return;

    uint32 const accountId = requester->GetSession()->GetAccountId();
    uint32 const requesterGuid = requester->GetGUID().GetCounter();

    // MB_ALT_ROSTER_ONLINE_STATE_V1_BEGIN
    PlayerbotMgr* const playerbotMgr = sPlayerbotsMgr.GetPlayerbotMgr(requester);
    // MB_ALT_ROSTER_ONLINE_STATE_V1_END

    std::vector<std::string> entries;
    bool truncated = false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT guid, name, class, level FROM characters "
        "WHERE account = {} AND deleteInfos_Name IS NULL ORDER BY guid",
        accountId);

    if (result)
    {
        do
        {
            Field* const fields = result->Fetch();
            uint32 const guid = fields[0].Get<uint32>();
            if (guid == requesterGuid)
                continue;

            if (entries.size() >= kMaxAltRosterEntries)
            {
                truncated = true;
                break;
            }

            std::string const name = fields[1].Get<std::string>();
            uint32 const classId = fields[2].Get<uint8>();
            uint32 const level = fields[3].Get<uint8>();
            bool const online = playerbotMgr && playerbotMgr->GetPlayerBot(guid) != nullptr;

            std::ostringstream entry;
            entry << guid
                << kFieldSeparator << UrlEncodeField(name)
                << kFieldSeparator << classId
                << kFieldSeparator << level
                << kFieldSeparator << (online ? "ONLINE" : "OFFLINE");

            std::string const payload = entry.str();
            if (!IsAddonPacketWithinBudget("ALT_ROSTER_ENTRY", payload))
            {
                truncated = true;
                continue;
            }

            entries.push_back(payload);
        }
        while (result->NextRow());
    }

    std::ostringstream framing;
    framing << entries.size() << kFieldSeparator << (truncated ? '1' : '0');

    SendAddonPacket(requester, replyType, "ALT_ROSTER_BEGIN", framing.str());
    for (std::string const& entry : entries)
        SendAddonPacket(requester, replyType, "ALT_ROSTER_ENTRY", entry);
    SendAddonPacket(requester, replyType, "ALT_ROSTER_END", framing.str());
}
// MB_ALT_ROSTER_DISCOVERY_V1_END

// MB_BOT_LIFECYCLE_V1_BEGIN
using BotLifecycleClock = std::chrono::steady_clock;

struct BotLifecyclePendingConnect
{
    BotLifecycleClock::time_point startedAt;
};

struct BotLifecycleRequesterState
{
    std::deque<BotLifecycleClock::time_point> requests;
    std::deque<std::pair<std::string, BotLifecycleClock::time_point>> recentTokens;
    std::map<uint32, BotLifecyclePendingConnect> pendingConnects;
};

std::map<uint32, BotLifecycleRequesterState> gBotLifecycleRequesterStates;

void PruneBotLifecycleRequesterState(
    BotLifecycleRequesterState& state,
    BotLifecycleClock::time_point const now)
{
    while (!state.requests.empty() && now - state.requests.front() >= kBotLifecycleRateWindow)
        state.requests.pop_front();

    while (!state.recentTokens.empty() && now - state.recentTokens.front().second >= kBotLifecycleReplayTtl)
        state.recentTokens.pop_front();

    while (state.recentTokens.size() > kBotLifecycleMaxRecentTokens)
        state.recentTokens.pop_front();

    for (auto it = state.pendingConnects.begin(); it != state.pendingConnects.end();)
    {
        if (now - it->second.startedAt >= kBotLifecyclePendingRetention)
            it = state.pendingConnects.erase(it);
        else
            ++it;
    }
}

BotLifecycleRequesterState* GetBotLifecycleRequesterState(
    Player* requester,
    bool create,
    BotLifecycleClock::time_point const now)
{
    if (!requester)
        return nullptr;

    uint32 const requesterKey = requester->GetGUID().GetCounter();
    auto it = gBotLifecycleRequesterStates.find(requesterKey);
    if (it != gBotLifecycleRequesterStates.end())
    {
        PruneBotLifecycleRequesterState(it->second, now);
        return &it->second;
    }

    if (!create)
        return nullptr;

    if (gBotLifecycleRequesterStates.size() >= kBotLifecycleMaxRequesterStates)
    {
        for (auto existing = gBotLifecycleRequesterStates.begin();
             existing != gBotLifecycleRequesterStates.end();)
        {
            PruneBotLifecycleRequesterState(existing->second, now);
            if (existing->second.requests.empty()
                && existing->second.recentTokens.empty()
                && existing->second.pendingConnects.empty())
            {
                existing = gBotLifecycleRequesterStates.erase(existing);
            }
            else
                ++existing;
        }
    }

    if (gBotLifecycleRequesterStates.size() >= kBotLifecycleMaxRequesterStates)
        return nullptr;

    it = gBotLifecycleRequesterStates.emplace(
        requesterKey, BotLifecycleRequesterState()).first;
    return &it->second;
}

std::map<uint32, std::deque<BotLifecycleClock::time_point>>
    gBotLifecycleQueryRequests;

bool ConsumeBotLifecycleQueryRateLimit(Player* requester)
{
    if (!requester)
        return false;

    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    uint32 const requesterKey = requester->GetGUID().GetCounter();

    auto it = gBotLifecycleQueryRequests.find(requesterKey);
    if (it == gBotLifecycleQueryRequests.end())
    {
        if (gBotLifecycleQueryRequests.size() >= kBotLifecycleMaxRequesterStates)
        {
            for (auto existing = gBotLifecycleQueryRequests.begin();
                 existing != gBotLifecycleQueryRequests.end();)
            {
                while (!existing->second.empty()
                    && now - existing->second.front() >= kBotLifecycleRateWindow)
                {
                    existing->second.pop_front();
                }

                if (existing->second.empty())
                    existing = gBotLifecycleQueryRequests.erase(existing);
                else
                    ++existing;
            }
        }

        if (gBotLifecycleQueryRequests.size() >= kBotLifecycleMaxRequesterStates)
            return false;

        it = gBotLifecycleQueryRequests.emplace(
            requesterKey,
            std::deque<BotLifecycleClock::time_point>()).first;
    }

    std::deque<BotLifecycleClock::time_point>& requests = it->second;
    while (!requests.empty()
        && now - requests.front() >= kBotLifecycleRateWindow)
    {
        requests.pop_front();
    }

    if (requests.size() >= kBotLifecycleQueryRateLimit)
        return false;

    requests.push_back(now);
    return true;
}

bool ConsumeBotLifecycleMutationRateLimit(Player* requester)
{
    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    BotLifecycleRequesterState* const state =
        GetBotLifecycleRequesterState(requester, true, now);
    if (!state)
        return false;

    if (state->requests.size() >= kBotLifecycleMutationRateLimit)
        return false;

    state->requests.push_back(now);
    return true;
}

bool RegisterBotLifecycleMutationToken(Player* requester, std::string const& token)
{
    if (!requester || !IsValidRequestToken(token))
        return false;

    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    BotLifecycleRequesterState* const state =
        GetBotLifecycleRequesterState(requester, true, now);
    if (!state)
        return false;

    for (auto const& recent : state->recentTokens)
        if (recent.first == token)
            return false;

    state->recentTokens.push_back({token, now});
    while (state->recentTokens.size() > kBotLifecycleMaxRecentTokens)
        state->recentTokens.pop_front();
    return true;
}

bool ResolveBotLifecycleTarget(
    Player* requester,
    uint32 lowGuid,
    ObjectGuid& targetGuid,
    CharacterCacheEntry const*& target,
    std::string& reason)
{
    target = nullptr;
    reason.clear();

    if (!requester || !requester->GetSession())
    {
        reason = "NO_SESSION";
        return false;
    }

    targetGuid = ObjectGuid::Create<HighGuid::Player>(lowGuid);
    if (targetGuid == requester->GetGUID())
    {
        reason = "SELF_TARGET";
        return false;
    }

    target = sCharacterCache->GetCharacterCacheByGuid(targetGuid);
    if (!target)
    {
        reason = "NOT_FOUND";
        return false;
    }

    return true;
}

bool IsBotLifecycleControlRelationAllowed(
    Player* requester,
    PlayerbotMgr* mgr,
    ObjectGuid const& targetGuid,
    CharacterCacheEntry const& target)
{
    if (!requester || !requester->GetSession() || !mgr)
        return false;

    uint32 const masterAccountId = requester->GetSession()->GetAccountId();
    bool const sameAccount =
        sPlayerbotAIConfig.allowAccountBots
        && target.AccountId == masterAccountId;

    Guild* const guild = requester->GetGuildId()
        ? sGuildMgr->GetGuildById(requester->GetGuildId())
        : nullptr;
    bool const sameGuild =
        sPlayerbotAIConfig.allowGuildBots
        && guild
        && guild->GetMember(targetGuid);

    bool const addClassBot =
        sRandomPlayerbotMgr.IsAddclassBot(targetGuid.GetCounter());

    bool const linkedAccount =
        sPlayerbotAIConfig.allowTrustedAccountBots
        && mgr->IsAccountLinked(target.AccountId, masterAccountId);

    // MB_BOT_LIFECYCLE_GROUP_AUTHORIZATION_B1_V1_BEGIN
    return sameAccount || sameGuild || addClassBot || linkedAccount;
    // MB_BOT_LIFECYCLE_GROUP_AUTHORIZATION_B1_V1_END
}

bool GetBotLifecyclePendingConnect(
    Player* requester,
    uint32 lowGuid,
    bool& timedOut)
{
    timedOut = false;
    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    BotLifecycleRequesterState* const state =
        GetBotLifecycleRequesterState(requester, false, now);
    if (!state)
        return false;

    auto const it = state->pendingConnects.find(lowGuid);
    if (it == state->pendingConnects.end())
        return false;

    // MB_BOT_LIFECYCLE_INFLIGHT_RETENTION_B2_V1_BEGIN
    timedOut =
        now - it->second.startedAt >= kBotLifecycleConnectTimeout;

    return true;
    // MB_BOT_LIFECYCLE_INFLIGHT_RETENTION_B2_V1_END
}

void ClearBotLifecyclePendingConnect(Player* requester, uint32 lowGuid)
{
    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    BotLifecycleRequesterState* const state =
        GetBotLifecycleRequesterState(requester, false, now);
    if (state)
        state->pendingConnects.erase(lowGuid);
}

bool StartBotLifecyclePendingConnect(Player* requester, uint32 lowGuid)
{
    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    BotLifecycleRequesterState* const state =
        GetBotLifecycleRequesterState(requester, true, now);
    if (!state)
        return false;

    // MB_BOT_LIFECYCLE_INFLIGHT_RETENTION_B2_V1_BEGIN
    // GetBotLifecycleRequesterState() already prunes entries only after the
    // longer kBotLifecyclePendingRetention window. Timed-out reporting must
    // not release an asynchronous AddPlayerBot operation still in flight.
    if (state->pendingConnects.size() >= kBotLifecycleMaxPendingConnects)
    // MB_BOT_LIFECYCLE_INFLIGHT_RETENTION_B2_V1_END
        return false;

    state->pendingConnects[lowGuid] = {now};
    return true;
}

std::size_t CountBotLifecyclePendingConnects(Player* requester)
{
    BotLifecycleClock::time_point const now = BotLifecycleClock::now();
    BotLifecycleRequesterState* const state =
        GetBotLifecycleRequesterState(requester, false, now);
    if (!state)
        return 0;

    // MB_BOT_LIFECYCLE_INFLIGHT_RETENTION_B2_V1_BEGIN
    // Timed-out-but-retained connects still reserve bot capacity until
    // completion is observed or kBotLifecyclePendingRetention expires.
    return state->pendingConnects.size();
    // MB_BOT_LIFECYCLE_INFLIGHT_RETENTION_B2_V1_END
}

void SendBotLifecycleResultPacket(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    uint32 lowGuid,
    std::string const& name,
    std::string const& action,
    std::string const& status,
    std::string const& reason)
{
    if (!requester)
        return;

    std::ostringstream out;
    out << requestToken
        << kFieldSeparator << lowGuid
        << kFieldSeparator << UrlEncodeField(name)
        << kFieldSeparator << action
        << kFieldSeparator << status
        << kFieldSeparator << reason;

    SendAddonPacket(requester, replyType, "BOT_LIFECYCLE", out.str());
}

void SendBotLifecycleStatePacket(
    Player* requester,
    ChatMsg replyType,
    uint32 lowGuid,
    std::string const& requestToken)
{
    ObjectGuid targetGuid;
    CharacterCacheEntry const* target = nullptr;
    std::string resolveReason;
    if (!ResolveBotLifecycleTarget(
            requester, lowGuid, targetGuid, target, resolveReason))
    {
        std::ostringstream out;
        out << requestToken
            << kFieldSeparator << lowGuid
            << kFieldSeparator
            << kFieldSeparator << "OFFLINE"
            << kFieldSeparator << resolveReason;
        SendAddonPacket(
            requester, replyType, "BOT_LIFECYCLE_STATE", out.str());
        return;
    }

    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(requester);
    if (!mgr)
    {
        std::ostringstream out;
        out << requestToken
            << kFieldSeparator << lowGuid
            << kFieldSeparator << UrlEncodeField(target->Name)
            << kFieldSeparator << "OFFLINE"
            << kFieldSeparator << "NO_MANAGER";
        SendAddonPacket(
            requester, replyType, "BOT_LIFECYCLE_STATE", out.str());
        return;
    }

    if (mgr->GetPlayerBot(targetGuid))
    {
        ClearBotLifecyclePendingConnect(requester, lowGuid);

        std::ostringstream out;
        out << requestToken
            << kFieldSeparator << lowGuid
            << kFieldSeparator << UrlEncodeField(target->Name)
            << kFieldSeparator << "ONLINE"
            << kFieldSeparator << "OK";
        SendAddonPacket(
            requester, replyType, "BOT_LIFECYCLE_STATE", out.str());
        return;
    }

    if (!IsBotLifecycleControlRelationAllowed(
            requester, mgr, targetGuid, *target))
    {
        std::ostringstream out;
        out << requestToken
            << kFieldSeparator << lowGuid
            << kFieldSeparator
            << kFieldSeparator << "OFFLINE"
            << kFieldSeparator << "FORBIDDEN";
        SendAddonPacket(
            requester, replyType, "BOT_LIFECYCLE_STATE", out.str());
        return;
    }

    bool timedOut = false;
    if (GetBotLifecyclePendingConnect(requester, lowGuid, timedOut))
    {
        std::ostringstream out;
        out << requestToken
            << kFieldSeparator << lowGuid
            << kFieldSeparator << UrlEncodeField(target->Name)
            << kFieldSeparator << "CONNECTING"
            << kFieldSeparator << "PENDING";
        SendAddonPacket(
            requester, replyType, "BOT_LIFECYCLE_STATE", out.str());
        return;
    }

    std::string reason = timedOut ? "TIMEOUT" : "OK";
    if (ObjectAccessor::FindConnectedPlayer(targetGuid))
        reason = "IN_USE";

    std::ostringstream out;
    out << requestToken
        << kFieldSeparator << lowGuid
        << kFieldSeparator << UrlEncodeField(target->Name)
        << kFieldSeparator << "OFFLINE"
        << kFieldSeparator << reason;
    SendAddonPacket(
        requester, replyType, "BOT_LIFECYCLE_STATE", out.str());
}

void RunBotLifecycleConnect(
    Player* requester,
    ChatMsg replyType,
    uint32 lowGuid,
    std::string const& requestToken)
{
    ObjectGuid targetGuid;
    CharacterCacheEntry const* target = nullptr;
    std::string reason;
    if (!ResolveBotLifecycleTarget(
            requester, lowGuid, targetGuid, target, reason))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, "",
            "CONNECT", "ERR", reason);
        return;
    }

    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(requester);
    if (!mgr)
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "ERR", "NO_MANAGER");
        return;
    }

    if (mgr->GetPlayerBot(targetGuid))
    {
        ClearBotLifecyclePendingConnect(requester, lowGuid);
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "OK", "ALREADY_ONLINE");
        return;
    }

    bool timedOut = false;
    if (GetBotLifecyclePendingConnect(requester, lowGuid, timedOut))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "PENDING", "ALREADY_CONNECTING");
        return;
    }

    if (!IsBotLifecycleControlRelationAllowed(
            requester, mgr, targetGuid, *target))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "ERR", "FORBIDDEN");
        return;
    }

    if (ObjectAccessor::FindConnectedPlayer(targetGuid))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "ERR", "IN_USE");
        return;
    }

    std::size_t const pendingCount =
        CountBotLifecyclePendingConnects(requester);
    uint32 const maxBots =
        uint32(PlayerbotAIConfig::instance().maxAddedBots);
    if (mgr->GetPlayerbotsCount() + pendingCount >= maxBots)
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "ERR", "MAX_BOTS");
        return;
    }

    if (!StartBotLifecyclePendingConnect(requester, lowGuid))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "ERR", "TOO_MANY_PENDING");
        return;
    }

    mgr->AddPlayerBot(
        targetGuid,
        requester->GetSession()->GetAccountId());

    if (mgr->GetPlayerBot(targetGuid))
    {
        ClearBotLifecyclePendingConnect(requester, lowGuid);
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "CONNECT", "OK", "ONLINE");
        return;
    }

    SendBotLifecycleResultPacket(
        requester, replyType, requestToken, lowGuid, target->Name,
        "CONNECT", "PENDING", "STARTED");
}

void RunBotLifecycleDisconnect(
    Player* requester,
    ChatMsg replyType,
    uint32 lowGuid,
    std::string const& requestToken)
{
    ObjectGuid targetGuid;
    CharacterCacheEntry const* target = nullptr;
    std::string reason;
    if (!ResolveBotLifecycleTarget(
            requester, lowGuid, targetGuid, target, reason))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, "",
            "DISCONNECT", "ERR", reason);
        return;
    }

    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(requester);
    if (!mgr)
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "DISCONNECT", "ERR", "NO_MANAGER");
        return;
    }

    Player* const bot = mgr->GetPlayerBot(targetGuid);
    if (!bot)
    {
        bool timedOut = false;
        if (GetBotLifecyclePendingConnect(
                requester, lowGuid, timedOut))
        {
            SendBotLifecycleResultPacket(
                requester, replyType, requestToken, lowGuid, target->Name,
                "DISCONNECT", "ERR", "CONNECTING");
            return;
        }

        ClearBotLifecyclePendingConnect(requester, lowGuid);
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "DISCONNECT", "OK", "ALREADY_OFFLINE");
        return;
    }

    mgr->LogoutPlayerBot(targetGuid);
    if (mgr->GetPlayerBot(targetGuid))
    {
        SendBotLifecycleResultPacket(
            requester, replyType, requestToken, lowGuid, target->Name,
            "DISCONNECT", "ERR", "STATE_MISMATCH");
        return;
    }

    ClearBotLifecyclePendingConnect(requester, lowGuid);
    SendBotLifecycleResultPacket(
        requester, replyType, requestToken, lowGuid, target->Name,
        "DISCONNECT", "OK", "OFFLINE");
}

// MB_BOT_TARGET_RESOLVE_V1_BEGIN
void SendBotTargetResolvePacket(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    std::string const& status,
    std::string const& reason,
    std::string const& canonicalName,
    uint32 lowGuid,
    std::string const& lifecycleState)
{
    std::ostringstream out;
    out << requestToken
        << kFieldSeparator << status
        << kFieldSeparator << reason
        << kFieldSeparator << UrlEncodeField(canonicalName)
        << kFieldSeparator << lowGuid
        << kFieldSeparator << lifecycleState;

    SendAddonPacket(requester, replyType, "BOT_TARGET_RESOLVE", out.str());
}

bool ResolveBotLifecycleTargetByName(
    Player* requester,
    std::string const& requestedName,
    ObjectGuid& targetGuid,
    CharacterCacheEntry const*& target)
{
    target = nullptr;

    if (!requester || !requester->GetSession())
        return false;

    std::string normalizedName = requestedName;
    if (normalizedName != Trim(normalizedName))
        return false;

    if (!normalizePlayerName(normalizedName))
        return false;

    targetGuid = sCharacterCache->GetCharacterGuidByName(normalizedName);
    if (targetGuid.IsEmpty() || targetGuid == requester->GetGUID())
        return false;

    target = sCharacterCache->GetCharacterCacheByGuid(targetGuid);
    return target != nullptr;
}

void RunBotTargetResolveRequest(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestedName,
    std::string const& requestToken)
{
    ObjectGuid targetGuid;
    CharacterCacheEntry const* target = nullptr;

    if (!ResolveBotLifecycleTargetByName(requester, requestedName, targetGuid, target))
    {
        SendBotTargetResolvePacket(
            requester, replyType, requestToken,
            "ERR", "NOT_ALLOWED", "", 0, "UNKNOWN");
        return;
    }

    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(requester);
    if (!mgr || !IsBotLifecycleControlRelationAllowed(
            requester, mgr, targetGuid, *target))
    {
        SendBotTargetResolvePacket(
            requester, replyType, requestToken,
            "ERR", "NOT_ALLOWED", "", 0, "UNKNOWN");
        return;
    }

    uint32 const lowGuid = targetGuid.GetCounter();

    if (mgr->GetPlayerBot(targetGuid))
    {
        ClearBotLifecyclePendingConnect(requester, lowGuid);
        SendBotTargetResolvePacket(
            requester, replyType, requestToken,
            "OK", "OK", target->Name, lowGuid, "ONLINE");
        return;
    }

    bool timedOut = false;
    if (GetBotLifecyclePendingConnect(requester, lowGuid, timedOut))
    {
        SendBotTargetResolvePacket(
            requester, replyType, requestToken,
            "OK", "PENDING", target->Name, lowGuid, "CONNECTING");
        return;
    }

    std::string reason = timedOut ? "TIMEOUT" : "OK";
    if (ObjectAccessor::FindConnectedPlayer(targetGuid))
        reason = "IN_USE";

    SendBotTargetResolvePacket(
        requester, replyType, requestToken,
        "OK", reason, target->Name, lowGuid, "OFFLINE");
}
// MB_BOT_TARGET_RESOLVE_V1_END
// MB_BOT_LIFECYCLE_V1_END

std::string BuildRosterPayload(Player* player)
{
    std::ostringstream out;
    bool first = true;

    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        if (!first)
            out << ';';
        first = false;

        out << bot->GetName() << ',' << static_cast<uint32>(bot->getClass()) << ',' << static_cast<uint32>(bot->GetLevel())
            << ',' << static_cast<uint32>(bot->GetMapId()) << ',' << (bot->IsAlive() ? '1' : '0') << ','
            << GetPct(bot->GetHealth(), bot->GetMaxHealth()) << ',' << GetPct(bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA));
    }

    return out.str();
}

void SendDetailPackets(Player* player, ChatMsg replyType)
{
    bool sent = false;

    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        std::string const payload = BuildBotDetailPayload(bot);
        if (payload.empty())
            continue;

        SendAddonPacket(player, replyType, "DETAIL", payload);

        std::string const professionPayload = BuildBotProfessionPayload(bot);
        if (!professionPayload.empty())
            SendAddonPacket(player, replyType, "PROFESSION", professionPayload);

        sent = true;
    }

    if (!sent)
        SendAddonPacket(player, replyType, "DETAILS", "");
}

std::string BuildDetailPayload(Player* player, std::string const& botName)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
        return "";

    return BuildBotDetailPayload(bot);
}

std::string BuildProfessionPayload(Player* player, std::string const& botName)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
        return "";

    return BuildBotProfessionPayload(bot);
}

void SendProfessionPackets(Player* player, ChatMsg replyType)
{
    bool sent = false;
    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        std::string const payload = BuildBotProfessionPayload(bot);
        if (payload.empty())
            continue;

        SendAddonPacket(player, replyType, "PROFESSION", payload);
        sent = true;
    }

    if (!sent)
        SendAddonPacket(player, replyType, "PROFESSIONS", "");
}

std::string BuildPvpStatsPayload(Player* player, std::string const& botName)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
        return "";

    return BuildPvpStatsPayload(bot);
}

void SendPvpStatsPackets(Player* player, ChatMsg replyType)
{
    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        std::string const payload = BuildPvpStatsPayload(bot);
        if (!payload.empty())
            SendAddonPacket(player, replyType, "PVP_STATS", payload);
    }
}

std::string BuildStatePayload(Player* player, std::string const& botName)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
        return Trim(botName) + std::string(1, kFieldSeparator) + kFieldSeparator;

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    if (!botAI)
        return bot->GetName() + std::string(1, kFieldSeparator) + kFieldSeparator;

    std::ostringstream out;
    out << bot->GetName() << kFieldSeparator << JoinStrategies(botAI->GetStrategies(BOT_STATE_COMBAT)) << kFieldSeparator
        << JoinStrategies(botAI->GetStrategies(BOT_STATE_NON_COMBAT));
    return out.str();
}

void SendStateAbort(Player* player, ChatMsg replyType, std::string const& token, std::string const& botName, std::string const& reason)
{
    std::ostringstream payload;
    payload << token << kFieldSeparator << UrlEncodeField(botName) << kFieldSeparator << UrlEncodeField(reason);
    if (SendStateAddonPacket(player, replyType, "STATE_ABORT", payload.str()))
        return;

    std::ostringstream fallbackPayload;
    fallbackPayload << token << kFieldSeparator << kFieldSeparator << UrlEncodeField(reason);
    if (SendStateAddonPacket(player, replyType, "STATE_ABORT", fallbackPayload.str()))
        return;

    std::ostringstream minimalPayload;
    minimalPayload << token << kFieldSeparator << kFieldSeparator << "ABORT_TOO_LONG";
    SendStateAddonPacket(player, replyType, "STATE_ABORT", minimalPayload.str());
}

bool AppendStateFramePacket(
    std::vector<std::pair<std::string, std::string>>& packets,
    std::string const& opcode,
    std::string const& payload,
    std::string& reason)
{
    if (!IsAddonPacketWithinBudget(opcode, payload))
    {
        reason = "PACKET_TOO_LONG";
        return false;
    }

    packets.emplace_back(opcode, payload);
    return true;
}

bool AppendStateFramesForBot(
    std::vector<std::pair<std::string, std::string>>& packets,
    std::string const& token,
    Player* bot,
    std::string& reason)
{
    if (!bot)
    {
        reason = "NO_BOT";
        return false;
    }

    PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
    std::vector<std::string> combatStrategies;
    std::vector<std::string> nonCombatStrategies;
    if (botAI)
    {
        combatStrategies = botAI->GetStrategies(BOT_STATE_COMBAT);
        nonCombatStrategies = botAI->GetStrategies(BOT_STATE_NON_COMBAT);
    }

    if (combatStrategies.size() > kMaxStateStrategiesPerScope || nonCombatStrategies.size() > kMaxStateStrategiesPerScope)
    {
        reason = "TOO_MANY_STRATEGIES";
        return false;
    }

    std::string const encodedBotName = UrlEncodeField(bot->GetName());
    std::ostringstream beginPayload;
    beginPayload << token << kFieldSeparator << encodedBotName << kFieldSeparator << combatStrategies.size() << kFieldSeparator
        << nonCombatStrategies.size();
    if (!AppendStateFramePacket(packets, "STATE_BEGIN", beginPayload.str(), reason))
        return false;

    for (std::size_t index = 0; index < combatStrategies.size(); ++index)
    {
        std::ostringstream itemPayload;
        itemPayload << token << kFieldSeparator << encodedBotName << kFieldSeparator << 'C' << kFieldSeparator << (index + 1)
            << kFieldSeparator << UrlEncodeField(combatStrategies[index]);
        if (!AppendStateFramePacket(packets, "STATE_ITEM", itemPayload.str(), reason))
            return false;
    }

    for (std::size_t index = 0; index < nonCombatStrategies.size(); ++index)
    {
        std::ostringstream itemPayload;
        itemPayload << token << kFieldSeparator << encodedBotName << kFieldSeparator << 'N' << kFieldSeparator << (index + 1)
            << kFieldSeparator << UrlEncodeField(nonCombatStrategies[index]);
        if (!AppendStateFramePacket(packets, "STATE_ITEM", itemPayload.str(), reason))
            return false;
    }

    std::ostringstream endPayload;
    endPayload << token << kFieldSeparator << encodedBotName << kFieldSeparator << combatStrategies.size() << kFieldSeparator
        << nonCombatStrategies.size();
    return AppendStateFramePacket(packets, "STATE_END", endPayload.str(), reason);
}

bool SendPreparedStatePackets(
    Player* player,
    ChatMsg replyType,
    std::vector<std::pair<std::string, std::string>> const& packets)
{
    for (auto const& packet : packets)
        if (!SendStateAddonPacket(player, replyType, packet.first, packet.second))
            return false;

    return true;
}

void SendFramedStatePacket(Player* player, ChatMsg replyType, std::string const& botName, std::string const& token)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
    {
        SendStateAbort(player, replyType, token, botName, "NO_BOT");
        return;
    }

    std::vector<std::pair<std::string, std::string>> packets;
    std::string reason;
    if (!AppendStateFramesForBot(packets, token, bot, reason))
    {
        SendStateAbort(player, replyType, token, bot->GetName(), reason);
        return;
    }

    if (!SendPreparedStatePackets(player, replyType, packets))
        SendStateAbort(player, replyType, token, bot->GetName(), "SEND_FAILED");
}

void SendSelfStrategyStatePacket(Player* player, ChatMsg replyType, std::string const& token)
{
    if (!player || !player->GetSession())
    {
        if (player)
            SendStateAbort(player, replyType, token, player->GetName(), "NO_SESSION");
        return;
    }

    if (!IsSelfBot(player))
    {
        SendStateAbort(player, replyType, token, player->GetName(), "NOT_SELF_BOT");
        return;
    }

    if (!GET_PLAYERBOT_AI(player))
    {
        SendStateAbort(player, replyType, token, player->GetName(), "NO_AI");
        return;
    }

    std::vector<std::pair<std::string, std::string>> packets;
    std::string reason;
    if (!AppendStateFramesForBot(packets, token, player, reason))
    {
        SendStateAbort(player, replyType, token, player->GetName(), reason);
        return;
    }

    if (!SendPreparedStatePackets(player, replyType, packets))
        SendStateAbort(player, replyType, token, player->GetName(), "SEND_FAILED");
}
void SendFramedStatePackets(Player* player, ChatMsg replyType, std::string const& token)
{
    std::vector<Player*> const bots = GetBridgeVisibleBots(player);
    if (bots.size() > kMaxStateBots)
    {
        SendStateAbort(player, replyType, token, "", "TOO_MANY_BOTS");
        return;
    }

    std::vector<std::pair<std::string, std::string>> packets;
    std::string reason;
    std::ostringstream beginPayload;
    beginPayload << token << kFieldSeparator << bots.size();
    if (!AppendStateFramePacket(packets, "STATES_BEGIN", beginPayload.str(), reason))
    {
        SendStateAbort(player, replyType, token, "", reason);
        return;
    }

    for (Player* const bot : bots)
    {
        if (!AppendStateFramesForBot(packets, token, bot, reason))
        {
            SendStateAbort(player, replyType, token, bot ? bot->GetName() : "", reason);
            return;
        }
    }

    std::ostringstream endPayload;
    endPayload << token << kFieldSeparator << bots.size();
    if (!AppendStateFramePacket(packets, "STATES_END", endPayload.str(), reason))
    {
        SendStateAbort(player, replyType, token, "", reason);
        return;
    }

    if (!SendPreparedStatePackets(player, replyType, packets))
        SendStateAbort(player, replyType, token, "", "SEND_FAILED");
}

void SendStatePackets(Player* player, ChatMsg replyType)
{
    bool sent = false;
    bool stateTooLong = false;
    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
        std::string combatStrategies;
        std::string nonCombatStrategies;

        if (botAI)
        {
            combatStrategies = JoinStrategies(botAI->GetStrategies(BOT_STATE_COMBAT));
            nonCombatStrategies = JoinStrategies(botAI->GetStrategies(BOT_STATE_NON_COMBAT));
        }

        std::ostringstream out;
        out << bot->GetName() << kFieldSeparator << combatStrategies << kFieldSeparator << nonCombatStrategies;
        if (!SendStateAddonPacket(player, replyType, "STATE", out.str()))
            stateTooLong = true;
        sent = true;
    }

    if (!sent)
        SendAddonPacket(player, replyType, "STATES", "");
    else if (stateTooLong)
        SendProtocolError(player, replyType, "GET", "STATES", "", "STATE_TOO_LONG");
}

std::string BuildStatsPayload(Player* player, std::string const& botName)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
        return "";

    return BuildStatsPayload(bot);
}

void SendStatsPackets(Player* player, ChatMsg replyType)
{
    for (Player* const bot : GetBridgeVisibleBots(player))
    {
        std::string const payload = BuildStatsPayload(bot);
        if (!payload.empty())
            SendAddonPacket(player, replyType, "STATS", payload);
    }
}

// MB_ISSUE33_SELF_BOT_V1_BEGIN
using SelfBotRateClock = std::chrono::steady_clock;
std::map<uint32, std::deque<SelfBotRateClock::time_point>> gSelfBotRequestRateStates;

bool ConsumeSelfBotRequestRateLimit(Player* requester)
{
    if (!requester)
        return false;

    SelfBotRateClock::time_point const now = SelfBotRateClock::now();
    uint32 const requesterKey = requester->GetGUID().GetCounter();

    auto pruneQueue = [now](std::deque<SelfBotRateClock::time_point>& attempts)
    {
        while (!attempts.empty() && now - attempts.front() >= kSelfBotRateWindow)
            attempts.pop_front();
    };

    auto it = gSelfBotRequestRateStates.find(requesterKey);
    if (it == gSelfBotRequestRateStates.end())
    {
        if (gSelfBotRequestRateStates.size() >= kSelfBotMaxRequesterStates)
        {
            for (auto stateIt = gSelfBotRequestRateStates.begin();
                 stateIt != gSelfBotRequestRateStates.end();)
            {
                pruneQueue(stateIt->second);
                if (stateIt->second.empty())
                    stateIt = gSelfBotRequestRateStates.erase(stateIt);
                else
                    ++stateIt;
            }
        }

        if (gSelfBotRequestRateStates.size() >= kSelfBotMaxRequesterStates)
            return false;

        it = gSelfBotRequestRateStates.emplace(
            requesterKey, std::deque<SelfBotRateClock::time_point>()).first;
    }

    std::deque<SelfBotRateClock::time_point>& attempts = it->second;
    pruneQueue(attempts);
    if (attempts.size() >= kSelfBotRateLimit)
        return false;

    attempts.push_back(now);
    return true;
}

std::map<uint32, SelfBotRateClock::time_point> gSelfBotHeavyActionRateStates;

bool ConsumeSelfBotHeavyActionRateLimit(Player* requester)
{
    if (!requester)
        return false;

    SelfBotRateClock::time_point const now = SelfBotRateClock::now();
    uint32 const requesterKey = requester->GetGUID().GetCounter();

    auto existingIt = gSelfBotHeavyActionRateStates.find(requesterKey);
    if (existingIt != gSelfBotHeavyActionRateStates.end())
    {
        if (now - existingIt->second < kSelfBotHeavyActionRateWindow)
            return false;

        gSelfBotHeavyActionRateStates.erase(existingIt);
    }

    if (gSelfBotHeavyActionRateStates.size() >= kSelfBotHeavyActionMaxRequesterStates)
    {
        for (auto stateIt = gSelfBotHeavyActionRateStates.begin();
             stateIt != gSelfBotHeavyActionRateStates.end();)
        {
            if (now - stateIt->second >= kSelfBotHeavyActionRateWindow)
                stateIt = gSelfBotHeavyActionRateStates.erase(stateIt);
            else
                ++stateIt;
        }
    }

    if (gSelfBotHeavyActionRateStates.size() >= kSelfBotHeavyActionMaxRequesterStates)
        return false;

    gSelfBotHeavyActionRateStates[requesterKey] = now;
    return true;
}

void SendSelfBotPacket(
    Player* requester,
    ChatMsg replyType,
    std::string const& opcode,
    std::string const& requestToken,
    std::string const& status,
    std::string const& reason)
{
    if (!requester)
        return;

    std::ostringstream out;
    out << requestToken
        << kFieldSeparator << status
        << kFieldSeparator << (IsSelfBot(requester) ? 1 : 0)
        << kFieldSeparator << UrlEncodeField(reason);
    SendAddonPacket(requester, replyType, opcode, out.str());
}

void RunSelfBotCommand(
    Player* requester,
    ChatMsg replyType,
    std::string const& requestToken,
    std::string const& desiredState)
{
    std::string status = "ERR";
    std::string reason = "UNKNOWN";

    if (!requester || !requester->GetSession())
        reason = "NO_SESSION";
    else
    {
        bool const desiredActive = desiredState == "ENABLE";
        bool const currentActive = IsSelfBot(requester);

        if (currentActive == desiredActive)
        {
            status = "OK";
            reason = desiredActive ? "ALREADY_ENABLED" : "ALREADY_DISABLED";
        }
        else if (desiredActive && GET_PLAYERBOT_AI(requester) && !currentActive)
            reason = "AI_STATE_CONFLICT";
        else if (desiredActive && sPlayerbotAIConfig.selfBotLevel == 0)
            reason = "DISABLED";
        else if (desiredActive
                 && sPlayerbotAIConfig.selfBotLevel == 1
                 && !requester->CanBeGameMaster())
            reason = "FORBIDDEN";
        else
        {
            PlayerbotMgr* const mgr = GET_PLAYERBOT_MGR(requester);
            if (!mgr)
                reason = "NO_MANAGER";
            else
            {
                mgr->HandlePlayerbotCommand("self", requester);
                if (IsSelfBot(requester) == desiredActive)
                {
                    status = "OK";
                    reason = "APPLIED";
                }
                else
                    reason = "STATE_MISMATCH";
            }
        }
    }

    SendSelfBotPacket(
        requester,
        replyType,
        "SELF_BOT_RESULT",
        requestToken,
        status,
        reason);
}
// MB_ISSUE33_SELF_BOT_V1_END

bool HandleBridgeOpcode(Player* player, ChatMsg replyType, std::string const& opcode, std::string const& payload)
{
    std::string const trimmedOpcode = Trim(opcode);
    std::string const normalized = ToUpper(trimmedOpcode);

    if (opcode != trimmedOpcode || !IsValidProtocolName(trimmedOpcode, kMaxOpcodeLength))
        return SendProtocolError(player, replyType, "", "", "", "BAD_OPCODE");

    if (normalized == "HELLO")
    {
        if (payload != kProtocolVersion)
            return SendProtocolError(player, replyType, normalized, "", "", "BAD_VERSION");

        SendAddonPacket(player, replyType, "HELLO_ACK", std::string(kProtocolVersion) + kFieldSeparator + kBridgeName);
        if (!SendCapabilitiesPackets(player, replyType))
            return SendProtocolError(player, replyType, normalized, "", "", "CAPS_BUILD_FAILED");
        return true;
    }

    if (normalized == "PING")
    {
        if (!IsValidRequestToken(payload))
            return SendProtocolError(player, replyType, normalized, "", "", "BAD_TOKEN");

        SendAddonPacket(player, replyType, "PONG", payload);
        return true;
    }

    if (normalized != "GET" && normalized != "RUN")
        return SendProtocolError(player, replyType, normalized, "", "", "UNKNOWN_OPCODE");

    std::vector<std::string> const fields = SplitFields(payload);
    if (fields.empty())
        return SendProtocolError(player, replyType, normalized, "", "", "EMPTY_REQUEST");

    std::string const rawRequestType = fields[0];
    std::string const requestType = ToUpper(Trim(rawRequestType));
    if (rawRequestType != Trim(rawRequestType) || !IsValidProtocolName(rawRequestType, kMaxRequestTypeLength))
        return SendProtocolError(player, replyType, normalized, "", "", "BAD_REQUEST_TYPE");

    if (normalized == "GET")
    {
        if (requestType == "ROSTER")
        {
            if (fields.size() != 1)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            SendAddonPacket(player, replyType, "ROSTER", BuildRosterPayload(player));
            return true;
        }

        if (requestType == "ALT_ROSTER")
        {
            if (fields.size() != 1)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            if (!ConsumeAltRosterRequestRateLimit(player))
                return SendProtocolError(player, replyType, normalized, requestType, "", "RATE_LIMIT");

            SendAltRosterPackets(player, replyType);
            return true;
        }

        if (requestType == "BOT_TARGET_RESOLVE")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            std::string targetName;
            if (!TryUrlDecodeField(fields[1], targetName, kMaxBotNameLength, false))
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_NAME");

            if (targetName != Trim(targetName))
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(
                    player, replyType, normalized, requestType, "", "BAD_TOKEN");

            if (!ConsumeBotLifecycleQueryRateLimit(player))
            {
                SendBotTargetResolvePacket(
                    player, replyType, fields[2],
                    "ERR", "RATE_LIMIT", "", 0, "UNKNOWN");
                return true;
            }

            RunBotTargetResolveRequest(
                player, replyType, targetName, fields[2]);
            return true;
        }
        if (requestType == "BOT_LIFECYCLE_STATE")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            uint32 lowGuid = 0;
            if (!TryParseUint32Field(
                    fields[1], 1, std::numeric_limits<uint32>::max(), lowGuid))
            {
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_GUID");
            }

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(
                    player, replyType, normalized, requestType, "", "BAD_TOKEN");

            if (!ConsumeBotLifecycleQueryRateLimit(player))
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "RATE_LIMIT");

            SendBotLifecycleStatePacket(
                player, replyType, lowGuid, fields[2]);
            return true;
        }

        if (requestType == "SELF_BOT")
        {
            std::string const token = GetSafeErrorToken(fields, 1);
            if (fields.size() != 2)
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidRequestToken(fields[1]))
                return SendProtocolError(
                    player, replyType, normalized, requestType, "", "BAD_TOKEN");

            if (!ConsumeSelfBotRequestRateLimit(player))
            {
                SendSelfBotPacket(
                    player, replyType, "SELF_BOT_STATE", fields[1], "ERR", "RATE_LIMIT");
                return true;
            }

            SendSelfBotPacket(
                player, replyType, "SELF_BOT_STATE", fields[1], "OK", "STATE");
            return true;
        }

        if (requestType == "SELF_STRATEGY_STATE")
        {
            std::string const token = GetSafeErrorToken(fields, 1);
            if (fields.size() != 2)
                return SendProtocolError(
                    player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidRequestToken(fields[1]))
                return SendProtocolError(
                    player, replyType, normalized, requestType, "", "BAD_TOKEN");

            if (!ConsumeSelfBotRequestRateLimit(player))
            {
                SendStateAbort(player, replyType, fields[1], player ? player->GetName() : "", "RATE_LIMIT");
                return true;
            }

            SendSelfStrategyStatePacket(player, replyType, fields[1]);
            return true;
        }

        if (requestType == "DETAIL")
        {
            if (fields.size() != 2)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_BOT_NAME");

            SendAddonPacket(player, replyType, "DETAIL", BuildDetailPayload(player, fields[1]));

            std::string const professionPayload = BuildProfessionPayload(player, fields[1]);
            if (!professionPayload.empty())
                SendAddonPacket(player, replyType, "PROFESSION", professionPayload);

            return true;
        }

        if (requestType == "DETAILS")
        {
            if (fields.size() != 1)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            SendDetailPackets(player, replyType);
            return true;
        }

        if (requestType == "PROFESSION")
        {
            if (fields.size() != 2)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_BOT_NAME");

            SendAddonPacket(player, replyType, "PROFESSION", BuildProfessionPayload(player, fields[1]));
            return true;
        }

        if (requestType == "PROFESSIONS")
        {
            if (fields.size() != 1)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            SendProfessionPackets(player, replyType);
            return true;
        }

        if (requestType == "STATE")
        {
            if (fields.size() == 2)
            {
                if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                    return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_BOT_NAME");

                std::string const legacyPayload = BuildStatePayload(player, fields[1]);
                if (!SendStateAddonPacket(player, replyType, "STATE", legacyPayload))
                    return SendProtocolError(player, replyType, normalized, requestType, "", "STATE_TOO_LONG");
                return true;
            }

            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            std::string botName;
            if (!TryUrlDecodeField(fields[1], botName, kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");
            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendFramedStatePacket(player, replyType, botName, fields[2]);
            return true;
        }

        if (requestType == "STATES")
        {
            if (fields.size() == 1)
            {
                SendStatePackets(player, replyType);
                return true;
            }

            std::string const token = GetSafeErrorToken(fields, 1);
            if (fields.size() != 2)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");
            if (!IsValidRequestToken(fields[1]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendFramedStatePackets(player, replyType, fields[1]);
            return true;
        }

        if (requestType == "FORMATIONS")
        {
            std::string const token = GetSafeErrorToken(fields, 3);
            if (fields.size() != 4)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            std::string target;
            if (ToUpper(fields[1]) != "GROUP" || !TryUrlDecodeField(fields[2], target, kMaxBotNameLength, true) || !target.empty())
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_SCOPE");

            if (!IsValidRequestToken(fields[3]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendFormationPackets(player, replyType, fields[1], fields[2], fields[3]);
            return true;
        }

        if (requestType == "TALENT_SPEC_LIST")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendTalentSpecListPackets(player, replyType, fields[1], fields[2]);
            return true;
        }

        if (requestType == "QUESTS")
        {
            std::string const token = GetSafeErrorToken(fields, 3);
            if (fields.size() != 4)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            std::string const mode = ToUpper(fields[1]);
            if (mode != "INCOMPLETED" && mode != "COMPLETED" && mode != "ALL")
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_MODE");

            if (!IsValidCanonicalRawField(fields[2], kMaxBotNameLength, true))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[3]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendQuestPackets(player, replyType, fields[1], fields[2], fields[3]);
            return true;
        }

        if (requestType == "QUEST_PROGRESS")
        {
            std::string const token = GetSafeErrorToken(fields, 2);

            if (fields.size() != 3 && fields.size() != 4)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            bool watch = false;

            if (fields.size() == 4)
            {
                if (fields[3] != "WATCH")
                    return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_MODE");

                watch = true;
            }

            if (!ConsumeQuestProgressRateLimit(player))
                return SendProtocolError(player, replyType, normalized, requestType, token, "RATE_LIMIT");

            SendQuestProgressPackets(player, replyType, fields[1], fields[2], watch);
            return true;
        }

        if (requestType == "GAMEOBJECTS")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, true))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendGameObjectPackets(player, replyType, fields[1], fields[2]);
            return true;
        }

        if (requestType == "GLYPHS")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendGlyphPackets(player, replyType, fields[1], fields[2]);
            return true;
        }

        if (requestType == "PVP_STATS" || requestType == "STATS")
        {
            if (fields.size() != 1 && fields.size() != 2)
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_FIELD_COUNT");

            if (fields.size() == 2 && !IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_BOT_NAME");

            std::string const botName = fields.size() == 2 ? fields[1] : "";
            if (requestType == "PVP_STATS")
            {
                if (botName.empty())
                    SendPvpStatsPackets(player, replyType);
                else
                    SendAddonPacket(player, replyType, "PVP_STATS", BuildPvpStatsPayload(player, botName));
            }
            else
            {
                if (botName.empty())
                    SendStatsPackets(player, replyType);
                else
                    SendAddonPacket(player, replyType, "STATS", BuildStatsPayload(player, botName));
            }

            return true;
        }

        if (requestType == "WEAPON_ENCHANT")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            std::string botName;
            if (!TryUrlDecodeField(fields[1], botName, kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendWeaponEnchantDebugPacket(player, replyType, botName, fields[2]);
            return true;
        }

        if (requestType == "INVENTORY" || requestType == "INVENTORY_EXACT" || requestType == "BUYBACK" || requestType == "BANK" || requestType == "GBANK" ||
            requestType == "SPELLBOOK" || requestType == "BOT_SKILLS" || requestType == "BOT_REPUTATIONS" ||
            requestType == "BOT_EMBLEMS" || requestType == "OUTFITS" || requestType == "TRAINER")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            if (requestType == "INVENTORY")
                SendInventorySnapshot(player, replyType, fields[1], fields[2]);
            else if (requestType == "INVENTORY_EXACT")
            {
                if (!ConsumeInventoryExactRateLimit(player))
                {
                    std::string const prefixPayload = fields[1] + std::string(1, kFieldSeparator) + fields[2];
                    SendAddonPacket(player, replyType, "INV_EXACT_BEGIN", prefixPayload);
                    SendAddonPacket(
                        player,
                        replyType,
                        "INV_EXACT_ERROR",
                        prefixPayload + std::string(1, kFieldSeparator) + "RATE_LIMIT");
                    SendAddonPacket(player, replyType, "INV_EXACT_END", prefixPayload);
                    return true;
                }
                SendInventoryExactSnapshot(player, replyType, fields[1], fields[2]);
            }
            else if (requestType == "BUYBACK")
                SendVendorBuybackPackets(player, replyType, fields[1], fields[2]);
            else if (requestType == "BANK" || requestType == "GBANK")
            {
                if (!ConsumeBankSnapshotRateLimit(player))
                {
                    std::string const prefixPayload = UrlEncodeField(fields[1]) + std::string(1, kFieldSeparator) + fields[2];
                    if (requestType == "BANK")
                    {
                        SendAddonPacket(player, replyType, "BANK_BEGIN", prefixPayload);
                        SendAddonPacket(player, replyType, "BANK_ERROR", prefixPayload + std::string(1, kFieldSeparator) + "RATE_LIMIT");
                        SendAddonPacket(player, replyType, "BANK_END", prefixPayload);
                    }
                    else
                    {
                        SendAddonPacket(player, replyType, "GBANK_BEGIN", prefixPayload);
                        SendAddonPacket(player, replyType, "GBANK_ERROR", prefixPayload + std::string(1, kFieldSeparator) + "RATE_LIMIT");
                        SendAddonPacket(player, replyType, "GBANK_END", prefixPayload);
                    }
                    return true;
                }

                if (requestType == "BANK")
                    SendBankPackets(player, replyType, fields[1], fields[2]);
                else
                    SendGuildBankPackets(player, replyType, fields[1], fields[2]);
            }
            else if (requestType == "SPELLBOOK")
                SendSpellbookSnapshot(player, replyType, fields[1], fields[2]);
            else if (requestType == "BOT_SKILLS")
                SendBotSkillPackets(player, replyType, fields[1], fields[2]);
            else if (requestType == "BOT_REPUTATIONS")
                SendBotReputationPackets(player, replyType, fields[1], fields[2]);
            else if (requestType == "BOT_EMBLEMS")
                SendBotEmblemPackets(player, replyType, fields[1], fields[2]);
            else if (requestType == "OUTFITS")
                SendOutfitPackets(player, replyType, fields[1], fields[2]);
            else
                SendTrainerPackets(player, replyType, fields[1], fields[2]);

            return true;
        }

        if (requestType == "ENCHANT_TRADE")
        {
            std::string const token = GetSafeErrorToken(fields, 2);
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            if (!IsValidRequestToken(fields[2]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendEnchantTradePackets(player, replyType, fields[1], fields[2]);
            return true;
        }

        if (requestType == "PROFESSION_RECIPES")
        {
            std::string const token = GetSafeErrorToken(fields, 3);
            if (fields.size() != 4)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

            uint32 skillId = 0;
            if (!TryParseUint32Field(fields[2], 1, std::numeric_limits<uint32>::max(), skillId))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");

            if (!IsValidRequestToken(fields[3]))
                return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

            SendProfessionRecipePackets(player, replyType, fields[1], fields[2], fields[3]);
            return true;
        }

        return SendProtocolError(player, replyType, normalized, requestType, "", "UNKNOWN_GET");
    }

    if (requestType == "BOT_CONNECT" || requestType == "BOT_DISCONNECT")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 3)
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        uint32 lowGuid = 0;
        if (!TryParseUint32Field(
                fields[1], 1, std::numeric_limits<uint32>::max(), lowGuid))
        {
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_GUID");
        }

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(
                player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::string const action =
            requestType == "BOT_CONNECT" ? "CONNECT" : "DISCONNECT";

        if (!ConsumeBotLifecycleMutationRateLimit(player))
        {
            SendBotLifecycleResultPacket(
                player, replyType, fields[2], lowGuid, "",
                action, "ERR", "RATE_LIMIT");
            return true;
        }

        if (!RegisterBotLifecycleMutationToken(player, fields[2]))
        {
            SendBotLifecycleResultPacket(
                player, replyType, fields[2], lowGuid, "",
                action, "ERR", "REPLAY");
            return true;
        }

        if (requestType == "BOT_CONNECT")
            RunBotLifecycleConnect(
                player, replyType, lowGuid, fields[2]);
        else
            RunBotLifecycleDisconnect(
                player, replyType, lowGuid, fields[2]);

        return true;
    }

    if (requestType == "SELF_BOT")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 3)
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(
                player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::string const desiredState = ToUpper(Trim(fields[2]));
        if (fields[2] != desiredState
            || (desiredState != "ENABLE" && desiredState != "DISABLE"))
        {
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_STATE");
        }

        if (!ConsumeSelfBotRequestRateLimit(player))
        {
            SendSelfBotPacket(
                player, replyType, "SELF_BOT_RESULT", fields[1], "ERR", "RATE_LIMIT");
            return true;
        }

        RunSelfBotCommand(player, replyType, fields[1], desiredState);
        return true;
    }

    if (requestType == "SELF_ACTION")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 4)
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_TOKEN");

        if (!IsValidProtocolName(fields[2], kMaxRequestTypeLength))
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_ACTION");

        if (!IsValidRawField(fields[3], 16, true))
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_ARGUMENT");

        RunSelfActionCommand(player, replyType, fields[1], fields[2], fields[3]);
        return true;
    }
    if (requestType == "SELF_STRATEGY")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 4)
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(
                player, replyType, normalized, requestType, "", "BAD_TOKEN");

        if (fields[2] != "C" && fields[2] != "N")
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_STATE");

        if (!IsValidEncodedField(fields[3], kMaxCommandLength, false))
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_ENCODING");

        RunSelfStrategyMutationCommand(player, replyType, fields[1], fields[2], fields[3]);
        return true;
    }

    if (requestType == "OUTFIT")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 5)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        if (!IsValidEncodedField(fields[3], kMaxCommandLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");

        if (fields[4] != "0" && fields[4] != "1")
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_PERSIST");

        RunOutfitCommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);
        return true;
    }

    if (requestType == "TRAINER_LEARN")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 5)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 trainerEntry = 0;
        if (!TryParseUint32Field(fields[3], 1, std::numeric_limits<uint32>::max(), trainerEntry))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");

        uint32 spellId = 0;
        if (ToUpper(fields[4]) != "ALL" && !TryParseUint32Field(fields[4], 1, std::numeric_limits<uint32>::max(), spellId))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");

        RunTrainerLearnCommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);
        return true;
    }

    if (requestType == "ENCHANT_TRADE")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 4)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 spellId = 0;
        if (!TryParseUint32Field(fields[3], 1, std::numeric_limits<uint32>::max(), spellId))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");

        RunEnchantTradeCommand(player, replyType, fields[1], fields[2], fields[3]);
        return true;
    }

    if (requestType == "CRAFT_RECIPE")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 6)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 skillId = 0;
        uint32 spellId = 0;
        uint32 itemId = 0;
        if (!TryParseUint32Field(fields[3], 1, std::numeric_limits<uint32>::max(), skillId) ||
            !TryParseUint32Field(fields[4], 1, std::numeric_limits<uint32>::max(), spellId) ||
            !TryParseUint32Field(fields[5], 0, std::numeric_limits<uint32>::max(), itemId))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunProfessionRecipeCraftCommand(player, replyType, fields[1], fields[2], fields[3], fields[4], fields[5]);
        return true;
    }


    if (requestType == "CRAFT_RECIPE_TARGET")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 8)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::string botName;
        if (!TryUrlDecodeField(fields[2], botName, kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT");

        uint32 skillId = 0;
        uint32 spellId = 0;
        uint32 targetBag = 0;
        uint32 targetSlot = 0;
        uint32 targetItemId = 0;
        if (!TryParseUint32Field(fields[3], 1, std::numeric_limits<uint32>::max(), skillId) ||
            !TryParseUint32Field(fields[4], 1, std::numeric_limits<uint32>::max(), spellId) ||
            !TryParseUint32Field(fields[5], 0, 255, targetBag) ||
            !TryParseUint32Field(fields[6], 0, 255, targetSlot) ||
            !TryParseUint32Field(fields[7], 1, std::numeric_limits<uint32>::max(), targetItemId))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunProfessionRecipeTargetCommand(
            player, replyType, botName, fields[1],
            skillId, spellId, targetBag, targetSlot, targetItemId
        );
        return true;
    }

    if (requestType == "TALENT_APPLY")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 4)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");
        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_TOKEN");

        std::string botName;
        if (!TryUrlDecodeField(fields[2], botName, kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT");

        std::string const build = Trim(fields[3]);
        if (build.empty() || build.size() > kMaxTalentApplyBuildLength)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BUILD");

        RunTalentApplyCommand(player, replyType, botName, fields[1], build);
        return true;
    }
    if (requestType == "TALENT_SPEC_APPLY")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 5)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");
        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_TOKEN");

        std::string botName;
        if (!TryUrlDecodeField(fields[2], botName, kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT");

        uint32 slot = 0;
        if (!TryParseUint32Field(fields[3], 1, 2, slot))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_SLOT");

        uint32 specIndex = 0;
        if (!TryParseUint32Field(fields[4], 0, 30, specIndex))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_SPEC");

        RunTalentSpecApplyCommand(player, replyType, botName, fields[1], slot, specIndex);
        return true;
    }

    if (requestType == "QUEST_ABANDON")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() != 3)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 questId = 0;
        if (!TryParseUint32Field(fields[2], 1, std::numeric_limits<uint32>::max(), questId))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");

        RunQuestAbandonCommand(player, replyType, fields[1], questId);
        return true;
    }
    if (requestType == "GROUP_ROLL")
    {
        std::string const token = GetSafeErrorToken(fields, 1);
        if (fields.size() < 3 || fields.size() > 4)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidRequestToken(fields[1]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::string const mode = ToUpper(Trim(fields[2]));
        if (fields[2] != mode || (mode != "NORMAL" && mode != "ITEM"))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_MODE");

        if (mode == "NORMAL")
        {
            if (fields.size() != 3)
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

            RunGroupRollCommand(player, replyType, fields[1], fields[2], "");
            return true;
        }

        if (fields.size() != 4)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidEncodedField(fields[3], kMaxGroupRollItemLinkLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");

        std::string itemLink;
        if (!TryUrlDecodeField(fields[3], itemLink, kMaxGroupRollItemLinkLength, false) ||
            itemLink.find("|Hitem:") == std::string::npos)
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ITEM");
        }

        RunGroupRollCommand(player, replyType, fields[1], fields[2], fields[3]);
        return true;
    }

    if (requestType == "ITEM_EQUIP")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 7)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        if (!TryParseUint32Field(fields[3], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[4], 0, 255, srcSlot) ||
            !TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(fields[6], 1, kMaxInventoryItemEquipCount, srcCount))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemEquipCommand(
            player, replyType, fields[1], fields[2],
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot), srcItemId, srcCount);
        return true;
    }

    if (requestType == "ITEM_UNEQUIP")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 5)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        if (!TryParseUint32Field(fields[3], EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END - 1, srcSlot) ||
            !TryParseUint32Field(fields[4], 1, std::numeric_limits<uint32>::max(), srcItemId))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemUnequipCommand(
            player, replyType, fields[1], fields[2], static_cast<uint8>(srcSlot), srcItemId);
        return true;
    }

    if (requestType == "ITEM_DESTROY")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 7)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        if (!TryParseUint32Field(fields[3], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[4], 0, 255, srcSlot) ||
            !TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(fields[6], 1, kMaxInventoryItemDestroyCount, srcCount))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemDestroyCommand(
            player, replyType, fields[1], fields[2],
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot), srcItemId, srcCount);
        return true;
    }
    if (requestType == "ITEM_USE")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 7)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        if (!TryParseUint32Field(fields[3], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[4], 0, 255, srcSlot) ||
            !TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(fields[6], 1, kMaxInventoryItemUseCount, srcCount))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemUseCommand(
            player, replyType, fields[1], fields[2],
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot), srcItemId, srcCount);
        return true;
    }

    if (requestType == "BUYBACK_ITEM")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 7)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 slot = 0;
        uint32 itemId = 0;
        uint32 count = 0;
        uint32 price = 0;
        if (!TryParseUint32Field(fields[3], BUYBACK_SLOT_START, BUYBACK_SLOT_END - 1, slot) ||
            !TryParseUint32Field(fields[4], 1, std::numeric_limits<uint32>::max(), itemId) ||
            !TryParseUint32Field(fields[5], 1, kMaxVendorBuybackCount, count) ||
            !TryParseUint32Field(fields[6], 0, std::numeric_limits<uint32>::max(), price))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunVendorBuybackCommand(player, replyType, fields[1], fields[2], slot, itemId, count, price);
        return true;
    }

    if (requestType == "ITEM_SELL")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 7)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        if (!TryParseUint32Field(fields[3], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[4], 0, 255, srcSlot) ||
            !TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(fields[6], 1, kMaxInventoryItemSellCount, srcCount))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemSellCommand(
            player, replyType, fields[1], fields[2],
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot), srcItemId, srcCount);
        return true;
    }

    if (requestType == "ITEM_MOVE")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 11)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        uint32 dstBag = 0;
        uint32 dstSlot = 0;
        uint32 dstItemId = 0;
        uint32 dstCount = 0;
        if (!TryParseUint32Field(fields[3], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[4], 0, 255, srcSlot) ||
            !TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(fields[6], 1, kMaxInventoryItemMoveCount, srcCount) ||
            !TryParseUint32Field(fields[7], 0, 255, dstBag) ||
            !TryParseUint32Field(fields[8], 0, 255, dstSlot) ||
            !TryParseUint32Field(fields[9], 0, std::numeric_limits<uint32>::max(), dstItemId) ||
            !TryParseUint32Field(fields[10], 0, kMaxInventoryItemMoveCount, dstCount))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        if ((dstItemId == 0) != (dstCount == 0))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_DESTINATION_STATE");

        RunInventoryItemMoveCommand(
            player, replyType, fields[1], fields[2],
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot), srcItemId, srcCount,
            static_cast<uint8>(dstBag), static_cast<uint8>(dstSlot), dstItemId, dstCount);
        return true;
    }

    if (requestType == "LOOT_RULE_ITEM")
    {
        std::string const token = GetSafeErrorToken(fields, 3);
        if (fields.size() != 6)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        std::string const scope = ToUpper(Trim(fields[1]));
        if (fields[1] != scope ||
            (scope != "ALL" && scope != "RAID" && scope != "GROUP" && scope != "PARTY" && scope != "BOT"))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_SCOPE");
        }

        if (fields[2].size() > kMaxBotNameLength ||
            !IsValidEncodedField(fields[2], kMaxBotNameLength, true))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");
        }

        std::string target;
        if (!TryUrlDecodeField(fields[2], target, kMaxBotNameLength, true))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");
        if (target != Trim(target))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_TARGET");
        if ((scope == "BOT" && target.empty()) ||
            ((scope == "ALL" || scope == "RAID") && !target.empty()))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_TARGET");
        }
        if ((scope == "GROUP" || scope == "PARTY") && !target.empty())
        {
            uint32 groupNumber = 0;
            if (!TryParseUint32Field(target, 1, 8, groupNumber))
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_TARGET");
        }

        if (!IsValidRequestToken(fields[3]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::string const action = ToUpper(Trim(fields[4]));
        if (fields[4] != action || (action != "ADD" && action != "REMOVE"))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ACTION");

        uint32 itemId = 0;
        if (!TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), itemId))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");

        RunLootRuleItemCommand(player, replyType, fields[1], fields[2], fields[3], fields[4], itemId);
        return true;
    }
    if (requestType == "ITEM_DEPOSIT_EXACT")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 8)
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(
                player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::string const action = ToUpper(Trim(fields[3]));
        if (fields[3] != action ||
            (action != "BANK_DEPOSIT" && action != "GBANK_DEPOSIT"))
        {
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_ACTION");
        }

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        if (!TryParseUint32Field(fields[4], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[5], 0, 255, srcSlot) ||
            !TryParseUint32Field(
                fields[6], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(
                fields[7], 1, kMaxInventoryItemDepositExactCount, srcCount))
        {
            return SendProtocolError(
                player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemDepositExactCommand(
            player, replyType, fields[1], fields[2], action,
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot),
            srcItemId, srcCount);
        return true;
    }

    if (requestType == "ITEM_TRADE")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 7)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        uint32 srcBag = 0;
        uint32 srcSlot = 0;
        uint32 srcItemId = 0;
        uint32 srcCount = 0;
        if (!TryParseUint32Field(fields[3], 0, 255, srcBag) ||
            !TryParseUint32Field(fields[4], 0, 255, srcSlot) ||
            !TryParseUint32Field(fields[5], 1, std::numeric_limits<uint32>::max(), srcItemId) ||
            !TryParseUint32Field(fields[6], 1, kMaxInventoryItemTradeCount, srcCount))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemTradeCommand(
            player, replyType, fields[1], fields[2],
            static_cast<uint8>(srcBag), static_cast<uint8>(srcSlot), srcItemId, srcCount);
        return true;
    }

    if (requestType == "ITEM_ACTION")
    {
        std::string const token = GetSafeErrorToken(fields, 2);
        if (fields.size() != 6)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], kMaxBotNameLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_BOT_NAME");

        if (!IsValidRequestToken(fields[2]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        if (!IsValidProtocolName(fields[3], kMaxRequestTypeLength))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ACTION");

        uint32 itemId = 0;
        uint32 count = 0;
        std::string const itemAction = ToUpper(fields[3]);
        if (itemAction == "SELL_GREY" || itemAction == "SELL_VENDOR" || itemAction == "OPEN_ITEMS")
        {
            if (fields[4] != "0" || fields[5] != "0")
                return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }
        else if (!TryParseUint32Field(fields[4], 1, std::numeric_limits<uint32>::max(), itemId) ||
                 !TryParseUint32Field(fields[5], 0, kMaxItemActionCount, count))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_NUMBER");
        }

        RunInventoryItemActionCommand(player, replyType, fields[1], fields[2], fields[3], fields[4], fields[5]);
        return true;
    }

    if (requestType == "STRATEGY")
    {
        std::string const token = GetSafeErrorToken(fields, 3);
        if (fields.size() != 6)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        std::string const scope = ToUpper(fields[1]);
        if ((scope != "ALL" && scope != "RAID" && scope != "GROUP" && scope != "PARTY" && scope != "BOT") ||
            fields[1] != scope)
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_SCOPE");
        }

        if (!IsValidEncodedField(fields[2], kMaxBotNameLength, true))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");

        std::string decodedTarget;
        if (!TryUrlDecodeField(fields[2], decodedTarget, kMaxBotNameLength, true) ||
            (scope == "BOT" && Trim(decodedTarget).empty()) ||
            (scope != "BOT" && !Trim(decodedTarget).empty()))
        {
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_TARGET");
        }

        if (!IsValidRequestToken(fields[3]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        if (fields[4] != "C" && fields[4] != "N")
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_STATE");

        if (!IsValidEncodedField(fields[5], kMaxCommandLength, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");

        RunStrategyMutationCommand(player, replyType, fields[1], fields[2], fields[3], fields[4], fields[5]);
        return true;
    }

    if (requestType == "FORMATION" || requestType == "COMBAT" || requestType == "POSITION" ||
        requestType == "LOOT" || requestType == "RTI")
    {
        std::string const token = GetSafeErrorToken(fields, 3);
        if (fields.size() != 5)
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_FIELD_COUNT");

        if (!IsValidCanonicalRawField(fields[1], 8, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_SCOPE");

        if (!IsValidEncodedField(fields[2], kMaxBotNameLength, true))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");

        if (!IsValidRequestToken(fields[3]))
            return SendProtocolError(player, replyType, normalized, requestType, "", "BAD_TOKEN");

        std::size_t const commandLimit = requestType == "FORMATION" ? 16 : kMaxCommandLength;
        if (!IsValidEncodedField(fields[4], commandLimit, false))
            return SendProtocolError(player, replyType, normalized, requestType, token, "BAD_ENCODING");

        if (requestType == "FORMATION")
            RunFormationCommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);
        else if (requestType == "COMBAT")
            RunCombatCommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);
        else if (requestType == "POSITION")
            RunPositionCommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);
        else if (requestType == "LOOT")
            RunLootCommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);
        else
            RunRTICommand(player, replyType, fields[1], fields[2], fields[3], fields[4]);

        return true;
    }

    return SendProtocolError(player, replyType, normalized, requestType, "", "UNKNOWN_RUN");
}

class MultiBotBridgePlayerScript final : public PlayerScript
{
public:
    MultiBotBridgePlayerScript() : PlayerScript("MultiBotBridgePlayerScript") {}

    bool TryHandle(Player* player, uint32 type, uint32 lang, std::string& msg)
    {
        if (!player)
            return false;

        std::string payload;
        std::string reason;
        BridgePayloadStatus const status = TryExtractBridgePayload(lang, msg, payload, reason);
        if (status == BridgePayloadStatus::NotBridge)
            return false;

        ChatMsg const replyType = NormalizeReplyChatType(type);
        if (status == BridgePayloadStatus::Invalid)
        {
            if (BridgeConsoleLogsEnabled())
            {
                LOG_WARN(
                    "playerbots",
                    "MultiBotBridge rejected player={} reason={} wireBytes={} type={}",
                    player->GetName(),
                    SanitizeLogValue(reason, 32),
                    msg.size(),
                    type);
            }

            SendProtocolError(player, replyType, "", "", "", reason);
            return true;
        }

        std::pair<std::string, std::string> const packet = SplitOnce(payload, kFieldSeparator);

        if (BridgeConsoleLogsEnabled())
        {
            LOG_INFO(
                "playerbots",
                "MultiBotBridge RX player={} opcode={} payloadBytes={} wireBytes={} type={}",
                player->GetName(),
                SanitizeLogValue(packet.first, kMaxOpcodeLength),
                packet.second.size(),
                msg.size(),
                type);
        }

        return HandleBridgeOpcode(player, replyType, packet.first, packet.second);
    }

    void OnPlayerCreateItem(Player* player, Item* item, uint32 /*count*/) override
    {
        NotifyPendingWarlockStoneItemCreated(player, item);
    }

    void OnPlayerUpdate(Player* player, uint32 p_time) override
    {
        UpdateQuestProgressWatch(player, p_time);
    }

    void OnPlayerBeforeLogout(Player* player) override
    {
        CancelPendingWarlockStoneSwitch(player, "STONE_LOGOUT", false);
        RemoveQuestProgressWatchesForPlayer(player);
    }

    void OnPlayerMapChanged(Player* player) override
    {
        CancelPendingWarlockStoneSwitch(player, "STONE_MAP_CHANGED", true);
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        return !TryHandle(player, type, lang, msg);
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* /*group*/) override
    {
        return !TryHandle(player, type, lang, msg);
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Guild* /*guild*/) override
    {
        return !TryHandle(player, type, lang, msg);
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* /*channel*/) override
    {
        return !TryHandle(player, type, lang, msg);
    }
};
} // namespace

void AddSC_multibot_bridge()
{
    if (BridgeConsoleLogsEnabled())
        LOG_INFO("server.loading", "mod-multibot-bridge loaded");
    new MultiBotBridgePlayerScript();
}
