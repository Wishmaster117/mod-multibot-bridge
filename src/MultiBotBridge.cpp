#include "Chat.h"
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
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ReputationMgr.h"
#include "AiObjectContext.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Talentspec.h"
#include "Trainer.h"
#include "Unit.h"
#include "World.h"
#include "WorldPacket.h"
#include "QuestPackets.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
char const* const kAddonPrefix = "MBOT";
char const* const kBridgeName = "mod-multibot-bridge";
char const* const kProtocolVersion = "1";
char const kFieldSeparator = '~';

bool BridgeConsoleLogsEnabled()
{
    return sConfigMgr->GetOption<bool>("MultiBotBridge.EnableConsoleLogs", true);
}

Player* FindBotByName(Player* player, std::string const& botName);
PlayerbotAI* GetBotAI(Player* bot);
std::vector<Player*> GetBridgeVisibleBots(Player* player);
void SendAddonPacket(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& payload = "");
void SendOutfitPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void SendTrainerPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void SendBagEntryPackets(Player* requester, ChatMsg replyType, Player* bot, std::string const& requestToken);
void RunOutfitCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& encodedSuffix, std::string const& persistToken);
void RunTrainerLearnCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& trainerEntryValue, std::string const& spellIdValue);
void RunProfessionRecipeCraftCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& skillIdValue, std::string const& spellIdValue, std::string const& itemIdValue);
void RunInventoryItemActionCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& actionValue, std::string const& itemIdValue, std::string const& countValue);
void RunQuestAbandonCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& questIdValue);
void RunQuestShareCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& questIdValue, std::string const& encodedTargetName);
void RunItemEquipCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& itemIdValue, std::string const& slotHintValue, std::string const& bagValue, std::string const& slotValue);
void RunItemTradeCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& itemIdValue, std::string const& encodedTargetName, std::string const& countValue, std::string const& bagValue, std::string const& slotValue);
void RunSpellCastCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& spellIdValue, std::string const& encodedTargetName);
void RunTalentApplyCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& encodedBuildString, std::string const& dryRunFlag);
void RunProfessionRecipeCraftTargetCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& skillIdValue, std::string const& spellIdValue, std::string const& targetItemIdValue, std::string const& targetBagValue, std::string const& targetSlotValue, std::string const& targetModeValue);
void SendInventoryBulkPackets(Player* requester, ChatMsg replyType, std::string const& requestToken);
void SendBotSkillsBulkPackets(Player* requester, ChatMsg replyType, std::string const& requestToken);
void SendBotReputationPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
void SendBotEmblemPackets(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken);
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

std::pair<std::string, std::string> SplitOnce(std::string const& value, char separator)
{
    size_t const pos = value.find(separator);
    if (pos == std::string::npos)
        return {value, ""};

    return {value.substr(0, pos), value.substr(pos + 1)};
}

bool TryExtractBridgePayload(uint32 lang, std::string const& msg, std::string& payload)
{
    if (lang != LANG_ADDON)
        return false;

    payload = Trim(msg);
    if (payload.empty())
        return false;

    if (payload.rfind(kAddonPrefix, 0) == 0)
    {
        payload.erase(0, std::char_traits<char>::length(kAddonPrefix));
        while (!payload.empty() && (payload.front() == '	' || payload.front() == ' '))
            payload.erase(payload.begin());
    }

    return !payload.empty();
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
    std::string out;
    out.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size() && std::isxdigit(static_cast<unsigned char>(value[i + 1])) && std::isxdigit(static_cast<unsigned char>(value[i + 2])))
        {
            std::string const hex = value.substr(i + 1, 2);
            out.push_back(static_cast<char>(std::strtoul(hex.c_str(), nullptr, 16)));
            i += 2;
            continue;
        }

        out.push_back(value[i]);
    }

    return out;
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
        if (!link.empty())
            entry.build = BuildTalentLinkPointSummary(link);

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

    SendBagEntryPackets(requester, replyType, bot, requestToken);

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        ItemTemplate const* const proto = item->GetTemplate();
        if (!proto)
            continue;

        std::string line = ChatHelper::FormatItem(proto, item->GetCount());
        if (item->IsSoulBound())
            line += " (soulbound)";

        std::ostringstream equipPayload;
        equipPayload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << requestToken
            << kFieldSeparator << static_cast<uint32>(slot)
            << kFieldSeparator << proto->ItemId
            << kFieldSeparator << item->GetCount()
            << kFieldSeparator << UrlEncodeField(line)
            << kFieldSeparator << (item->IsSoulBound() ? 1 : 0);
        SendAddonPacket(requester, replyType, "INV_EQUIP_LOC", equipPayload.str());
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

            std::string locationLine = ChatHelper::FormatItem(proto, item->GetCount());
            if (item->IsSoulBound())
                locationLine += " (soulbound)";

            std::ostringstream locationPayload;
            locationPayload << UrlEncodeField(bot->GetName())
                << kFieldSeparator << requestToken
                << kFieldSeparator << static_cast<uint32>(item->GetBagSlot())
                << kFieldSeparator << static_cast<uint32>(item->GetSlot())
                << kFieldSeparator << itemId
                << kFieldSeparator << item->GetCount()
                << kFieldSeparator << UrlEncodeField(locationLine)
                << kFieldSeparator << (item->IsSoulBound() ? 1 : 0);
            SendAddonPacket(requester, replyType, "INV_ITEM_LOC", locationPayload.str());

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
        Unit* const unit = botAI->GetUnit(guid);
        if (!unit || unit->IsHostileTo(bot) || !unit->HasNpcFlag(static_cast<NPCFlags>(UNIT_NPC_FLAG_VENDOR)))
            continue;

        Creature* const creature = unit->ToCreature();
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

int32 GetGuildBankTabWithdrawRemaining(Guild* guild, Player* bot, uint8 tabId)
{
    if (!guild || !bot)
        return 0;

    Guild::Member const* const member = guild->GetMember(bot->GetGUID());
    if (!member)
        return 0;

    if (member->IsRank(GR_GUILDMASTER) || guild->GetLeaderGUID() == bot->GetGUID())
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

int32 GetGuildBankWithdrawRemaining(Guild* guild, Player* bot)
{
    int32 bestRemaining = 0;
    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
    {
        int32 const remaining = GetGuildBankTabWithdrawRemaining(guild, bot, tabId);
        if (remaining == std::numeric_limits<int32>::max())
            return remaining;

        if (remaining > bestRemaining)
            bestRemaining = remaining;
    }

    return bestRemaining;
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

    Guild* const guild = sGuildMgr->GetGuildById(bot->GetGuildId());
    if (!guild)
    {
        sendErrorAndEnd("BOT_NOT_IN_GUILD");
        return;
    }

    int32 const withdrawRemaining = GetGuildBankWithdrawRemaining(guild, bot);
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
    uint32 const expectedTrainerEntry = static_cast<uint32>(std::strtoul(Trim(trainerEntryValue).c_str(), nullptr, 10));
    std::string const requestedSpell = ToUpper(Trim(spellIdValue));
    bool const learnAll = requestedSpell == "ALL";
    uint32 const requestedSpellId = learnAll ? 0 : static_cast<uint32>(std::strtoul(requestedSpell.c_str(), nullptr, 10));

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
    uint32 const skillId = static_cast<uint32>(std::strtoul(Trim(skillIdValue).c_str(), nullptr, 10));
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

        uint32 const itemEntry = static_cast<uint32>(std::strtoul(item.c_str(), nullptr, 10));
        if (itemEntry)
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

bool IsUpdateOutfitCommandSuffix(std::string const& suffix)
{
    OutfitCommandParts const parts = ParseOutfitCommandSuffix(suffix);
    return parts.action == "UPDATE";
}

bool IsDirectBridgeOutfitCommandSuffix(std::string const& suffix)
{
    OutfitCommandParts const parts = ParseOutfitCommandSuffix(suffix);
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

bool ApplyBridgeNativeOutfitCommand(Player* bot, std::string const& suffix)
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

        std::ostringstream out;
        if (parts.action == "REPLACE")
            out << "Replacing current equip with outfit " << parts.name;
        else
            out << "Equipping outfit " << parts.name;

        botAI->TellMaster(out.str());
        return true;
    }

    if (parts.action == "UPDATE")
    {
        std::vector<uint32> const entries = CollectCurrentEquippedOutfitEntries(bot);
        if (entries.empty())
            return false;

        return SaveOutfitEntries(botAI, parts.name, entries);
    }

    if (parts.action == "RESET")
        return SaveOutfitEntries(botAI, parts.name, std::vector<uint32>());

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

void SendRunResult(Player* requester, ChatMsg replyType, char const* opcode, std::string const& botName, std::string const& token, bool ok, std::string const& reason, std::vector<std::string> const& extraFields = std::vector<std::string>())
{
    std::ostringstream payload;
    payload << UrlEncodeField(botName)
        << kFieldSeparator << token
        << kFieldSeparator << (ok ? "OK" : "ERR")
        << kFieldSeparator << UrlEncodeField(reason);

    for (std::string const& field : extraFields)
        payload << kFieldSeparator << UrlEncodeField(field);

    SendAddonPacket(requester, replyType, opcode, payload.str());
}

bool ParseUint32Field(std::string const& value, uint32& out)
{
    std::string const trimmed = Trim(value);
    if (trimmed.empty())
        return false;

    char* end = nullptr;
    unsigned long const parsed = std::strtoul(trimmed.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed > std::numeric_limits<uint32>::max())
        return false;

    out = static_cast<uint32>(parsed);
    return true;
}

bool ParseUint8Field(std::string const& value, uint8& out)
{
    uint32 parsed = 0;
    if (!ParseUint32Field(value, parsed) || parsed > std::numeric_limits<uint8>::max())
        return false;

    out = static_cast<uint8>(parsed);
    return true;
}

bool SameName(std::string const& left, std::string const& right)
{
    return ToUpper(Trim(left)) == ToUpper(Trim(right));
}

bool FindQuestLogSlot(Player* bot, uint32 questId, uint8& outSlot)
{
    if (!bot || !questId)
        return false;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        if (bot->GetQuestSlotQuestId(slot) == questId)
        {
            outSlot = slot;
            return true;
        }
    }

    return false;
}

Player* FindAllowedPlayerTarget(Player* requester, std::string const& targetName)
{
    std::string normalizedName = Trim(UrlDecodeField(targetName));
    if (normalizedName.empty())
        return nullptr;

    if (requester && SameName(requester->GetName(), normalizedName))
        return requester;

    if (Player* const bot = FindBotByName(requester, normalizedName))
        return bot;

    if (!normalizePlayerName(normalizedName))
        return nullptr;

    Player* const target = ObjectAccessor::FindPlayerByName(normalizedName, false);
    if (!target)
        return nullptr;

    if (requester && requester->GetGroup() && target->GetGroup() == requester->GetGroup())
        return target;

    return nullptr;
}

Unit* ResolveSpellTarget(Player* requester, Player* bot, PlayerbotAI* botAI, std::string const& encodedTargetName)
{
    std::string const targetName = Trim(UrlDecodeField(encodedTargetName));
    if (!bot)
        return nullptr;

    if (targetName.empty())
    {
        if (Unit* const selected = bot->GetSelectedUnit())
            return selected;

        return bot;
    }

    if (Player* const targetPlayer = FindAllowedPlayerTarget(requester, targetName))
        return targetPlayer;

    if (!botAI || !botAI->GetAiObjectContext())
        return nullptr;

    static char const* const valueNames[] = { "possible targets", "all targets", "nearest npcs" };
    for (char const* const valueName : valueNames)
    {
        GuidVector const units = *botAI->GetAiObjectContext()->GetValue<GuidVector>(valueName);
        for (ObjectGuid const guid : units)
        {
            Unit* const unit = botAI->GetUnit(guid);
            if (unit && SameName(unit->GetName(), targetName))
                return unit;
        }
    }

    return nullptr;
}

std::string GetBridgeSpellFailureReason(SpellCastResult result)
{
    switch (result)
    {
        case SPELL_CAST_OK:
        case SPELL_FAILED_SUCCESS:
            return "OK";
        case SPELL_FAILED_NOT_KNOWN:
            return "MISSING_SPELL";
        case SPELL_FAILED_BAD_TARGETS:
        case SPELL_FAILED_BAD_IMPLICIT_TARGETS:
        case SPELL_FAILED_NO_VALID_TARGETS:
            return "INVALID_TARGET";
        case SPELL_FAILED_OUT_OF_RANGE:
        case SPELL_FAILED_TOO_CLOSE:
            return "OUT_OF_RANGE";
        case SPELL_FAILED_NO_POWER:
            return "NO_MANA";
        case SPELL_FAILED_NOT_READY:
        case SPELL_FAILED_ITEM_NOT_READY:
            return "COOLDOWN";
        case SPELL_FAILED_REAGENTS:
            return "REAGENTS";
        case SPELL_FAILED_NEED_MORE_ITEMS:
            return "NO_MATERIALS";
        case SPELL_FAILED_MOVING:
            return "MOVING";
        case SPELL_FAILED_NOT_IN_CONTROL:
        case SPELL_FAILED_STUNNED:
        case SPELL_FAILED_CONFUSED:
        case SPELL_FAILED_FLEEING:
            return "LOST_CONTROL";
        case SPELL_FAILED_TRY_AGAIN:
        case SPELL_FAILED_SPELL_IN_PROGRESS:
            return "TRY_AGAIN";
        case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
            return "REQUIRES_SPELL_FOCUS";
        case SPELL_FAILED_EQUIPPED_ITEM:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND:
        case SPELL_FAILED_TOTEM_CATEGORY:
        case SPELL_FAILED_TOTEMS:
            return "MISSING_TOOLS";
        default:
            return "CAST_FAILED";
    }
}

struct BridgeSpellCheckData
{
    bool ok = false;
    std::string reason = "CAST_FAILED";
    SpellCastResult result = SPELL_FAILED_ERROR;
};

void BuildBridgeSpellTargets(Player* bot, SpellInfo const* spellInfo, Unit* target, Item* itemTarget, SpellCastTargets& targets)
{
    if (!target)
        target = bot;

    if (spellInfo->Effects[0].Effect != SPELL_EFFECT_OPEN_LOCK &&
        (spellInfo->Targets & TARGET_FLAG_ITEM || spellInfo->Targets & TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        // EN: Trade enchant/enhancement spells target the other player's not-traded slot.
        // FR: Les enchantements/améliorations en échange ciblent l'emplacement non échangé de l'autre joueur.
        if (itemTarget && bot->GetTrader())
            targets.SetTradeItemTarget(bot);
        else
            targets.SetItemTarget(itemTarget);
    }
    else if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
        targets.SetDst(*target);
    else if (spellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
        targets.SetDst(*bot);
    else
        targets.SetUnitTarget(target);
}

SpellCastResult CastBridgeSpellDirect(Player* bot, SpellInfo const* spellInfo, Unit* target, Item* itemTarget = nullptr)
{
    if (!bot || !spellInfo)
        return SPELL_FAILED_ERROR;

    if (!target)
        target = bot;

    ObjectGuid const oldSelection = bot->GetSelectedUnit() ? bot->GetSelectedUnit()->GetGUID() : ObjectGuid();
    bot->SetSelection(target->GetGUID());

    Spell* const spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
    SpellCastTargets targets;
    BuildBridgeSpellTargets(bot, spellInfo, target, itemTarget, targets);
    SpellCastResult const result = spell->prepare(&targets);

    bot->SetSelection(oldSelection);

    return result;
}

BridgeSpellCheckData CheckBridgeSpellCast(Player* bot, PlayerbotAI* botAI, uint32 spellId, Unit* target, Item* itemTarget = nullptr)
{
    BridgeSpellCheckData data;

    if (!bot || !botAI || !spellId)
    {
        data.reason = "BAD_REQUEST";
        return data;
    }

    if (!target)
        target = bot;

    if (!target->IsInWorld())
    {
        data.reason = "INVALID_TARGET";
        return data;
    }

    if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL))
    {
        data.reason = "LOST_CONTROL";
        return data;
    }

    SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        data.reason = "UNKNOWN_SPELL";
        return data;
    }

    Pet* const pet = bot->GetPet();
    if (!bot->HasSpell(spellId) && !(pet && pet->HasSpell(spellId)))
    {
        data.result = SPELL_FAILED_NOT_KNOWN;
        data.reason = "MISSING_SPELL";
        return data;
    }

    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
    {
        data.reason = "TRY_AGAIN";
        return data;
    }

    if (bot->HasSpellCooldown(spellId))
    {
        data.result = SPELL_FAILED_NOT_READY;
        data.reason = "COOLDOWN";
        return data;
    }

    if (bot->IsFlying() || bot->HasUnitState(UNIT_STATE_IN_FLIGHT))
    {
        data.reason = "CAST_FAILED";
        return data;
    }

    if (!bot->IsStandState())
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
        data.result = SPELL_FAILED_NOT_STANDING;
        data.reason = "TRY_AGAIN";
        return data;
    }

    uint32 const castTime = !spellInfo->IsChanneled() ? spellInfo->CalcCastTime(bot) : spellInfo->GetDuration();
    if ((castTime || spellInfo->IsAutoRepeatRangedSpell()) && bot->isMoving())
    {
        data.result = SPELL_FAILED_MOVING;
        data.reason = "MOVING";
        return data;
    }

    ObjectGuid const oldSelection = bot->GetSelectedUnit() ? bot->GetSelectedUnit()->GetGUID() : ObjectGuid();
    bot->SetSelection(target->GetGUID());

    Spell* const spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
    SpellCastTargets targets;
    BuildBridgeSpellTargets(bot, spellInfo, target, itemTarget, targets);

    // EN: CheckCast reads Spell::m_targets; validate the same target set that CastSpell will prepare.
    // FR: CheckCast lit Spell::m_targets; valider la même cible que CastSpell préparera.
    spell->m_targets = targets;
    data.result = spell->CheckCast(true);
    delete spell;

    bot->SetSelection(oldSelection);

    data.reason = GetBridgeSpellFailureReason(data.result);
    data.ok = data.reason == "OK";
    return data;
}

Item* FindInventoryItemByEntry(Player* bot, uint32 itemEntry)
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

Item* FindItemByOptionalPosition(Player* bot, uint32 itemEntry, uint8 bag, uint8 slot, bool hasPosition)
{
    if (!bot || !itemEntry)
        return nullptr;

    if (hasPosition)
    {
        Item* const item = bot->GetItemByPos(bag, slot);
        return item && item->GetEntry() == itemEntry ? item : nullptr;
    }

    return FindInventoryItemByEntry(bot, itemEntry);
}

std::string MapEquipError(InventoryResult result)
{
    switch (result)
    {
        case EQUIP_ERR_OK:
            return "OK";
        case EQUIP_ERR_ITEM_NOT_FOUND:
            return "MISSING_ITEM";
        case EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM:
        case EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM2:
            return "WRONG_CLASS";
        case EQUIP_ERR_CANT_EQUIP_LEVEL_I:
        case EQUIP_ERR_PURCHASE_LEVEL_TOO_LOW:
            return "WRONG_LEVEL";
        case EQUIP_ERR_INVENTORY_FULL:
        case EQUIP_ERR_BAG_FULL:
            return "SLOT_BLOCKED";
        default:
            return "EQUIP_FAILED";
    }
}

uint8 ResolveEquipSlot(Player* bot, PlayerbotAI* botAI, ItemTemplate const* proto, std::string const& slotHintValue, std::string& reason)
{
    if (!bot || !botAI || !proto)
    {
        reason = "BAD_REQUEST";
        return NULL_SLOT;
    }

    std::string const slotHint = ToUpper(Trim(slotHintValue.empty() ? "AUTO" : slotHintValue));
    if (slotHint == "AUTO")
    {
        if (proto->Class == ITEM_CLASS_CONTAINER)
        {
            for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
                if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot))
                    return bagSlot;

            return INVENTORY_SLOT_BAG_START;
        }

        if (proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_THROWN || proto->InventoryType == INVTYPE_RANGEDRIGHT)
            return EQUIPMENT_SLOT_RANGED;

        return botAI->FindEquipSlot(proto, NULL_SLOT, true);
    }

    if (slotHint == "BAG")
    {
        if (proto->Class != ITEM_CLASS_CONTAINER)
        {
            reason = "NOT_EQUIPPABLE";
            return NULL_SLOT;
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot))
                return bagSlot;

        return INVENTORY_SLOT_BAG_START;
    }

    if (slotHint == "MAIN_HAND")
    {
        switch (proto->InventoryType)
        {
            case INVTYPE_WEAPON:
            case INVTYPE_2HWEAPON:
            case INVTYPE_WEAPONMAINHAND:
                return EQUIPMENT_SLOT_MAINHAND;
            default:
                reason = "NOT_EQUIPPABLE";
                return NULL_SLOT;
        }
    }

    if (slotHint == "OFF_HAND")
    {
        switch (proto->InventoryType)
        {
            case INVTYPE_SHIELD:
            case INVTYPE_WEAPONOFFHAND:
            case INVTYPE_HOLDABLE:
                return EQUIPMENT_SLOT_OFFHAND;
            case INVTYPE_WEAPON:
                if (bot->CanDualWield())
                    return EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_2HWEAPON:
                if (bot->CanDualWield() && bot->CanTitanGrip() && proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM && proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF && proto->SubClass != ITEM_SUBCLASS_WEAPON_FISHING_POLE)
                    return EQUIPMENT_SLOT_OFFHAND;
                break;
            default:
                break;
        }

        reason = "NOT_EQUIPPABLE";
        return NULL_SLOT;
    }

    if (slotHint == "RANGED")
    {
        if (proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_RANGEDRIGHT || proto->InventoryType == INVTYPE_THROWN || proto->InventoryType == INVTYPE_RELIC)
            return EQUIPMENT_SLOT_RANGED;

        reason = "NOT_EQUIPPABLE";
        return NULL_SLOT;
    }

    reason = "BAD_SLOT_HINT";
    return NULL_SLOT;
}

bool EquipItemToSlot(Player* bot, Item* item, uint8 dstSlot, std::string& reason)
{
    if (!bot || !bot->GetSession() || !item || dstSlot == NULL_SLOT)
    {
        reason = "BAD_REQUEST";
        return false;
    }

    uint16 dest = 0;
    InventoryResult const canEquip = bot->CanEquipItem(dstSlot, dest, item, true);
    if (canEquip != EQUIP_ERR_OK)
    {
        reason = MapEquipError(canEquip);
        return false;
    }

    if (item->GetBagSlot() == INVENTORY_SLOT_BAG_0 && item->GetSlot() == dstSlot)
    {
        reason = "OK";
        return true;
    }

    WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
    ObjectGuid itemGuid = item->GetGUID();
    packet << itemGuid << dstSlot;

    WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
    nicePacket.Read();
    bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);
    reason = "OK";
    return true;
}

bool IsStackableItem(Item* item)
{
    if (!item || !item->GetTemplate())
        return false;

    return item->GetTemplate()->GetMaxStackSize() > 1;
}

std::vector<Item*> FindInventoryStacksByEntry(Player* bot, uint32 itemEntry)
{
    std::vector<Item*> items;
    if (!bot || !itemEntry)
        return items;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* const item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == itemEntry)
                items.push_back(item);

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag const* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            if (Item* const item = bot->GetItemByPos(bag, slot))
                if (item->GetEntry() == itemEntry)
                    items.push_back(item);
    }

    return items;
}

Item* FindBestTradeSourceStack(Player* bot, uint32 itemId, uint32 count, bool oneStackMode, uint8 preferredBag, uint8 preferredSlot, bool hasPreferredPosition, std::string& reason)
{
    if (!bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return nullptr;
    }

    if (hasPreferredPosition)
    {
        Item* const item = bot->GetItemByPos(preferredBag, preferredSlot);
        if (!item || item->GetEntry() != itemId)
        {
            reason = "MISSING_ITEM";
            return nullptr;
        }

        if (oneStackMode || item->GetCount() >= count)
            return item;

        reason = "COUNT_UNAVAILABLE";
        return nullptr;
    }

    std::vector<Item*> items = FindInventoryStacksByEntry(bot, itemId);
    if (items.empty())
    {
        reason = "MISSING_ITEM";
        return nullptr;
    }

    if (oneStackMode)
    {
        std::sort(items.begin(), items.end(), [](Item* left, Item* right)
        {
            return left->GetCount() < right->GetCount();
        });
        return items.front();
    }

    for (Item* const item : items)
        if (item->GetCount() == count)
            return item;

    Item* best = nullptr;
    for (Item* const item : items)
    {
        if (item->GetCount() > count && (!best || item->GetCount() < best->GetCount()))
            best = item;
    }

    if (best)
        return best;

    reason = "COUNT_UNAVAILABLE";
    return nullptr;
}

bool SelectExactTradeStacksRecursive(std::vector<Item*> const& items, std::size_t index, uint32 remaining, std::vector<Item*>& selected)
{
    if (!remaining)
        return true;

    if (index >= items.size())
        return false;

    for (std::size_t i = index; i < items.size(); ++i)
    {
        Item* const item = items[i];
        if (!item || item->GetCount() > remaining)
            continue;

        selected.push_back(item);
        if (SelectExactTradeStacksRecursive(items, i + 1, remaining - item->GetCount(), selected))
            return true;
        selected.pop_back();
    }

    return false;
}

std::vector<Item*> SelectExactTradeStacks(Player* bot, uint32 itemId, uint32 count)
{
    std::vector<Item*> selected;
    std::vector<Item*> items = FindInventoryStacksByEntry(bot, itemId);
    std::sort(items.begin(), items.end(), [](Item* left, Item* right)
    {
        return left->GetCount() > right->GetCount();
    });

    SelectExactTradeStacksRecursive(items, 0, count, selected);
    return selected;
}

Item* SplitStackForExactTrade(Player* bot, Item* source, uint32 count, std::string& reason)
{
    if (!bot || !source || !count || source->GetCount() <= count)
        return source;

    ItemPosCountVec dest;
    InventoryResult const canStore = bot->CanStoreItem(NULL_BAG, NULL_SLOT, dest, source->GetEntry(), count, nullptr, false);
    if (canStore != EQUIP_ERR_OK || dest.empty())
    {
        reason = "COUNT_UNAVAILABLE";
        return nullptr;
    }

    uint16 const dst = dest.front().pos;
    uint16 const src = (static_cast<uint16>(source->GetBagSlot()) << 8) | source->GetSlot();
    bot->SplitItem(src, dst, count);

    Item* const splitItem = bot->GetItemByPos(dst >> 8, dst & 255);
    if (!splitItem || splitItem->GetEntry() != source->GetEntry() || splitItem->GetCount() != count)
    {
        reason = "COUNT_UNAVAILABLE";
        return nullptr;
    }

    return splitItem;
}

uint8 GetFreeTradeSlotCount(Player* bot)
{
    if (!bot || !bot->GetTradeData())
        return 0;

    uint8 freeSlots = 0;
    TradeData* const trade = bot->GetTradeData();
    for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
        if (!trade->GetItem(TradeSlots(i)))
            ++freeSlots;

    return freeSlots;
}

bool AddItemToTradeSlot(Player* bot, Item* item, std::string& reason)
{
    if (!bot || !bot->GetSession() || !bot->GetTradeData() || !item)
    {
        reason = "TRADE_FAILED";
        return false;
    }

    if (!item->CanBeTraded(false, true))
    {
        reason = "ITEM_NOT_TRADABLE";
        return false;
    }

    int8 tradeSlot = -1;
    TradeData* const trade = bot->GetTradeData();
    for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (!trade->GetItem(TradeSlots(i)))
        {
            tradeSlot = i;
            break;
        }
    }

    if (tradeSlot == -1)
    {
        reason = "TRADE_SLOT_FULL";
        return false;
    }

    WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
    packet << static_cast<uint8>(tradeSlot);
    packet << static_cast<uint8>(item->GetBagSlot());
    packet << static_cast<uint8>(item->GetSlot());
    bot->GetSession()->HandleSetTradeItemOpcode(packet);

    reason = "OK";
    return true;
}

bool IsResetTalentBuild(std::string const& build)
{
    std::string const trimmed = Trim(build);
    if (trimmed.empty())
        return true;

    for (char const c : trimmed)
        if (c != '0' && c != '-' && c != ' ' && c != '\t')
            return false;

    return true;
}

struct BagEntryData
{
    uint8 bagIndex = 0;
    uint32 bagItemId = 0;
    std::string bagLink;
    uint32 numSlots = 0;
    std::string bagType;
};

std::string GetBagTypeString(ItemTemplate const* proto)
{
    if (!proto)
        return "BACKPACK";

    if (proto->BagFamily & BAG_FAMILY_MASK_ARROWS)
        return "QUIVER";
    if (proto->BagFamily & BAG_FAMILY_MASK_BULLETS)
        return "AMMO_POUCH";
    if (proto->BagFamily & BAG_FAMILY_MASK_SOUL_SHARDS)
        return "SOUL_SHARD";
    if (proto->BagFamily & BAG_FAMILY_MASK_HERBS)
        return "HERB";
    if (proto->BagFamily & BAG_FAMILY_MASK_ENCHANTING_SUPP)
        return "ENCHANTING";
    if (proto->BagFamily & BAG_FAMILY_MASK_MINING_SUPP)
        return "MINING";
    if (proto->BagFamily & BAG_FAMILY_MASK_ENGINEERING_SUPP)
        return "ENGINEERING";

    if (proto->Class == ITEM_CLASS_CONTAINER && proto->SubClass == ITEM_SUBCLASS_CONTAINER)
        return "NORMAL";

    return "UNKNOWN";
}

std::vector<BagEntryData> BuildBagEntries(Player* bot)
{
    std::vector<BagEntryData> entries;

    BagEntryData backpack;
    backpack.bagIndex = 0;
    backpack.numSlots = 16;
    backpack.bagType = "BACKPACK";
    entries.push_back(backpack);

    if (!bot)
        return entries;

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* const pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        ItemTemplate const* const proto = pBag->GetTemplate();
        BagEntryData entry;
        entry.bagIndex = static_cast<uint8>((bag - INVENTORY_SLOT_BAG_START) + 1);
        entry.bagItemId = proto ? proto->ItemId : 0;
        entry.bagLink = proto ? ChatHelper::FormatItem(proto, 1) : "";
        entry.numSlots = pBag->GetBagSize();
        entry.bagType = GetBagTypeString(proto);
        entries.push_back(entry);
    }

    return entries;
}

void SendBagEntryPackets(Player* requester, ChatMsg replyType, Player* bot, std::string const& requestToken)
{
    if (!requester || !bot)
        return;

    for (BagEntryData const& entry : BuildBagEntries(bot))
    {
        std::ostringstream payload;
        payload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << requestToken
            << kFieldSeparator << static_cast<uint32>(entry.bagIndex)
            << kFieldSeparator << entry.bagItemId
            << kFieldSeparator << UrlEncodeField(entry.bagLink)
            << kFieldSeparator << entry.numSlots
            << kFieldSeparator << UrlEncodeField(entry.bagType);
        SendAddonPacket(requester, replyType, "INV_BAG", payload.str());
    }
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

        uint32 const remainingRequest = requestedCount > 0 ? requestedCount - moved : 0;
        uint32 const toMove = remainingRequest > 0 && stackCount > remainingRequest ? remainingRequest : stackCount;

        if (toMove == stackCount)
        {
            bot->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
            bot->BankItem(dest, item, true);
        }
        else
        {
            Item* const splitItem = SplitStackForExactTrade(bot, item, toMove, reason);
            if (!splitItem)
                return moved;
            bot->BankItem(dest, splitItem, true);
        }
        moved += toMove;

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

        uint32 const remainingRequest = requestedCount > 0 ? requestedCount - moved : 0;
        uint32 const toMove = remainingRequest > 0 && stackCount > remainingRequest ? remainingRequest : stackCount;

        if (toMove == stackCount)
        {
            bot->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
            bot->StoreItem(dest, item, true);
        }
        else
        {
            Item* const splitItem = SplitStackForExactTrade(bot, item, toMove, reason);
            if (!splitItem)
                return moved;
            bot->StoreItem(dest, splitItem, true);
        }
        moved += toMove;

        if (requestedCount > 0 && moved >= requestedCount)
            break;
    }

    if (!moved)
        reason = "ITEM_NOT_FOUND";

    return moved;
}

uint32 MoveMatchingBagItemsToGuildBank(Player* requester, Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    if (!bot->GetGuildId())
    {
        reason = "BOT_NOT_IN_GUILD";
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

    if (!guild->MemberHasTabRights(bot->GetGUID(), 0, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
    {
        reason = "NO_GUILD_BANK_RIGHTS";
        return 0;
    }

    uint32 moved = 0;
    while (Item* const item = FindBagItemByEntry(bot, itemId))
    {
        uint32 const stackCount = item->GetCount();
        uint32 const playerSlot = item->GetSlot();
        uint32 const playerBag = item->GetBagSlot();

        uint32 const remainingRequest = requestedCount > 0 ? requestedCount - moved : 0;
        uint32 const toMove = remainingRequest > 0 && stackCount > remainingRequest ? remainingRequest : stackCount;

        guild->SwapItemsWithInventory(bot, false, 0, 255, playerBag, playerSlot, toMove);

        if (toMove < stackCount)
        {
            Item* const newItem = bot->GetItemByPos(playerBag, playerSlot);
            if (!newItem || newItem->GetEntry() != itemId)
            {
                reason = "GUILD_BANK_FULL";
                return moved;
            }
        }

        moved += toMove;

        if (requestedCount > 0 && moved >= requestedCount)
            break;
    }

    if (!moved)
        reason = "ITEM_NOT_FOUND";

    return moved;
}

uint32 MoveMatchingGuildBankItemsToBags(Player* bot, uint32 itemId, uint32 requestedCount, std::string& reason)
{
    if (!bot || !itemId)
    {
        reason = "BAD_REQUEST";
        return 0;
    }

    if (!bot->GetGuildId())
    {
        reason = "BOT_NOT_IN_GUILD";
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

    if (GetGuildBankWithdrawRemaining(guild, bot) == 0)
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

        if (GetGuildBankTabWithdrawRemaining(guild, bot, tabId) == 0)
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
            moved += after - before;

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
        if (requestedCount > 0 && bought >= requestedCount)
            break;

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

void RunInventoryItemActionCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& actionValue, std::string const& itemIdValue, std::string const& countValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    std::string const action = ToUpper(Trim(actionValue));
    uint32 const itemId = static_cast<uint32>(std::strtoul(Trim(itemIdValue).c_str(), nullptr, 10));
    uint32 const requestedCount = static_cast<uint32>(std::strtoul(Trim(countValue).c_str(), nullptr, 10));

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    std::string reason;
    uint32 moved = 0;
    if (!bot)
        reason = "NO_BOT";
    else if (action == "BANK_DEPOSIT")
        moved = MoveMatchingBagItemsToBank(bot, itemId, requestedCount, reason);
    else if (action == "BANK_WITHDRAW")
        moved = MoveMatchingBankItemsToBags(bot, itemId, requestedCount, reason);
    else if (action == "GBANK_DEPOSIT")
        moved = MoveMatchingBagItemsToGuildBank(requester, bot, itemId, requestedCount, reason);
    else if (action == "GBANK_WITHDRAW")
        moved = MoveMatchingGuildBankItemsToBags(bot, itemId, requestedCount, reason);
    else if (action == "BUY_ITEM")
        moved = BuyMatchingVendorItem(bot, itemId, requestedCount, reason);
    else
        reason = "BAD_ACTION";

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

void RunProfessionRecipeCraftCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& skillIdValue, std::string const& spellIdValue, std::string const& itemIdValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 const skillId = static_cast<uint32>(std::strtoul(Trim(skillIdValue).c_str(), nullptr, 10));
    uint32 const spellId = static_cast<uint32>(std::strtoul(Trim(spellIdValue).c_str(), nullptr, 10));
    uint32 const expectedItemId = static_cast<uint32>(std::strtoul(Trim(itemIdValue).c_str(), nullptr, 10));

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    uint32 actualItemId = expectedItemId;
    std::string result = ValidateProfessionRecipeCraft(bot, skillId, spellId, expectedItemId, actualItemId);
    if (result == "OK")
        result = CastProfessionRecipe(bot, spellId);

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

void RunQuestAbandonCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& questIdValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 questId = 0;

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "QUEST_ABANDON", effectiveBotName, token, false, "NO_BOT");
        return;
    }

    if (!ParseUint32Field(questIdValue, questId) || !questId || !sObjectMgr->GetQuestTemplate(questId))
    {
        SendRunResult(requester, replyType, "QUEST_ABANDON", effectiveBotName, token, false, "INVALID_QUEST_ID");
        return;
    }

    uint8 questSlot = 0;
    if (!FindQuestLogSlot(bot, questId, questSlot))
    {
        SendRunResult(requester, replyType, "QUEST_ABANDON", effectiveBotName, token, false, "MISSING_QUEST");
        return;
    }

    bot->SetQuestSlot(questSlot, 0);
    bot->TakeQuestSourceItem(questId, false);
    bot->SetQuestStatus(questId, QUEST_STATUS_NONE);
    bot->RemoveRewardedQuest(questId);
    bot->RemoveActiveQuest(questId, false);

    SendRunResult(requester, replyType, "QUEST_ABANDON", effectiveBotName, token, true, "OK");
}

void RunQuestShareCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& questIdValue, std::string const& encodedTargetName)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 questId = 0;

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "NO_BOT");
        return;
    }

    if (!ParseUint32Field(questIdValue, questId) || !questId)
    {
        SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "INVALID_QUEST_ID");
        return;
    }

    Quest const* const quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
    {
        SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "INVALID_QUEST_ID");
        return;
    }

    uint8 questSlot = 0;
    if (!FindQuestLogSlot(bot, questId, questSlot))
    {
        SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "MISSING_QUEST");
        return;
    }

    if (!bot->CanShareQuest(questId))
    {
        SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "NOT_SHAREABLE");
        return;
    }

    if (!bot->GetGroup())
    {
        SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "INVALID_TARGET");
        return;
    }

    std::string const targetName = Trim(UrlDecodeField(encodedTargetName));
    if (!targetName.empty())
    {
        Player* const target = FindAllowedPlayerTarget(requester, encodedTargetName);
        if (!target || target->GetGroup() != bot->GetGroup() || target == bot)
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "INVALID_TARGET");
            return;
        }

        if (!target->IsInMap(bot))
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "OUT_OF_RANGE");
            return;
        }

        if (!target->SatisfyQuestStatus(quest, false))
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "TARGET_HAS_QUEST");
            return;
        }

        if (target->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "TARGET_COMPLETE");
            return;
        }

        if (!target->CanTakeQuest(quest, false))
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "NOT_SHAREABLE");
            return;
        }

        if (!target->SatisfyQuestLog(false))
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "QUEST_LOG_FULL");
            return;
        }

        if (target->GetDivider())
        {
            SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, false, "TARGET_BUSY");
            return;
        }
    }

    // EN: Native AzerothCore quest share is group-wide; empty target skips single-recipient pre-validation.
    // FR: Le partage de quête AzerothCore est global au groupe; une cible vide saute la pré-validation mono-destinataire.
    WorldPacket packet(CMSG_PUSHQUESTTOPARTY);
    packet << questId;
    WorldPackets::Quest::PushQuestToParty pushQuest(std::move(packet));
    pushQuest.Read();
    bot->GetSession()->HandlePushQuestToParty(pushQuest);

    // EN: Bot recipients do not click the client popup; feed their AI the same packet used by playerbots auto-share.
    // FR: Les bots destinataires ne cliquent pas la fenêtre client; on injecte au bot AI le même paquet que l'auto-partage playerbots.
    uint32 botAcceptCount = 0;
    if (Group* const group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* const member = itr->GetSource();
            if (!member || member == bot || !member->IsInWorld() || !member->IsInMap(bot))
                continue;

            if (!targetName.empty() && member->GetName() != targetName)
                continue;

            PlayerbotAI* const memberAI = GET_PLAYERBOT_AI(member);
            if (!memberAI)
                continue;

            if (!member->SatisfyQuestStatus(quest, false) || member->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE ||
                !member->CanTakeQuest(quest, false) || !member->SatisfyQuestLog(false))
                continue;

            // EN: Native share sets Divider on eligible recipients; playerbot accept action needs it.
            // FR: Le partage natif définit Divider sur les destinataires éligibles; l'action d'acceptation playerbot en a besoin.
            if (member->GetDivider().IsEmpty())
                continue;

            WorldPacket acceptPacket(CMSG_PUSHQUESTTOPARTY, 20);
            acceptPacket << questId;
            memberAI->HandleMasterIncomingPacket(acceptPacket);
            ++botAcceptCount;
        }
    }

    SendRunResult(requester, replyType, "QUEST_SHARE", effectiveBotName, token, true, "OK", { "BOT_ACCEPTS:" + std::to_string(botAcceptCount) });
}

void RunItemEquipCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& itemIdValue, std::string const& slotHintValue, std::string const& bagValue, std::string const& slotValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 itemId = 0;
    uint8 bag = 0;
    uint8 slot = 0;
    bool const hasPosition = ParseUint8Field(bagValue, bag) && ParseUint8Field(slotValue, slot);

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, false, "NO_BOT");
        return;
    }

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
    {
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, false, "NO_AI");
        return;
    }

    if (!ParseUint32Field(itemIdValue, itemId) || !itemId)
    {
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, false, "BAD_REQUEST");
        return;
    }

    Item* const item = FindItemByOptionalPosition(bot, itemId, bag, slot, hasPosition);
    if (!item)
    {
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, false, "MISSING_ITEM");
        return;
    }

    ItemTemplate const* const proto = item->GetTemplate();
    if (!proto || (proto->InventoryType == INVTYPE_NON_EQUIP && proto->Class != ITEM_CLASS_CONTAINER))
    {
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, false, "NOT_EQUIPPABLE");
        return;
    }

    if (proto->InventoryType == INVTYPE_AMMO)
    {
        bot->SetAmmo(proto->ItemId);
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, true, "OK");
        return;
    }

    std::string reason;
    uint8 const dstSlot = ResolveEquipSlot(bot, botAI, proto, slotHintValue, reason);
    if (dstSlot == NULL_SLOT)
    {
        if (reason.empty())
            reason = "NOT_EQUIPPABLE";
        SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, false, reason);
        return;
    }

    bool const ok = EquipItemToSlot(bot, item, dstSlot, reason);
    SendRunResult(requester, replyType, "ITEM_EQUIP", effectiveBotName, token, ok, ok ? "OK" : reason);
}

void RunSpellCastCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& spellIdValue, std::string const& encodedTargetName)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 spellId = 0;

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "CAST_SPELL", effectiveBotName, token, false, "NO_BOT");
        return;
    }

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
    {
        SendRunResult(requester, replyType, "CAST_SPELL", effectiveBotName, token, false, "NO_AI");
        return;
    }

    if (!ParseUint32Field(spellIdValue, spellId) || !spellId)
    {
        SendRunResult(requester, replyType, "CAST_SPELL", effectiveBotName, token, false, "BAD_REQUEST");
        return;
    }

    Unit* const target = ResolveSpellTarget(requester, bot, botAI, encodedTargetName);
    if (!target)
    {
        SendRunResult(requester, replyType, "CAST_SPELL", effectiveBotName, token, false, "INVALID_TARGET");
        return;
    }

    BridgeSpellCheckData const check = CheckBridgeSpellCast(bot, botAI, spellId, target);
    if (!check.ok)
    {
        SendRunResult(requester, replyType, "CAST_SPELL", effectiveBotName, token, false, check.reason, { std::to_string(static_cast<uint32>(check.result)) });
        return;
    }

    SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    SpellCastResult const result = CastBridgeSpellDirect(bot, spellInfo, target);
    bool const ok = result == SPELL_CAST_OK;
    SendRunResult(requester, replyType, "CAST_SPELL", effectiveBotName, token, ok, ok ? "OK" : GetBridgeSpellFailureReason(result), ok ? std::vector<std::string>() : std::vector<std::string>{ std::to_string(static_cast<uint32>(result)) });
}

void RunTalentApplyCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& encodedBuildString, std::string const& dryRunFlag)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    std::string const buildString = Trim(UrlDecodeField(encodedBuildString));
    bool const dryRun = Trim(dryRunFlag) == "1" || ToUpper(Trim(dryRunFlag)) == "TRUE";

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, false, "NO_BOT", { "" });
        return;
    }

    if (IsResetTalentBuild(buildString))
    {
        if (dryRun)
        {
            SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, true, "OK", { "RESET" });
            return;
        }

        bool const ok = bot->resetTalents(true);
        SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, ok, ok ? "OK" : "RESET_FAILED", { "RESET" });
        return;
    }

    std::ostringstream validation;
    TalentSpec base(bot->getClassMask());
    if (!base.CheckTalentLink(buildString, &validation))
    {
        SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, false, "INVALID_BUILD", { validation.str() });
        return;
    }

    std::vector<std::vector<uint32>> const parsedSpecLink = PlayerbotAIConfig::ParseTempTalentsOrder(bot->getClass(), buildString);
    if (parsedSpecLink.empty())
    {
        SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, false, "INVALID_BUILD", { "Invalid link " + buildString });
        return;
    }

    if (dryRun)
    {
        SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, true, "OK", { "DRY_RUN:" + buildString });
        return;
    }

    // EN: Match legacy "talents apply <link>": parser + PlayerbotFactory handles order/dependencies.
    // FR: Même logique que l'ancien "talents apply <link>": le parseur + PlayerbotFactory gèrent l'ordre/dépendances.
    PlayerbotFactory::InitTalentsByParsedSpecLink(bot, parsedSpecLink, true);
    if (PlayerbotAI* const botAI = GetBotAI(bot))
        botAI->ResetStrategies();

    SendRunResult(requester, replyType, "TALENT_APPLY", effectiveBotName, token, true, "OK", { "APPLIED:" + buildString });
}

void RunProfessionRecipeCraftTargetCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& skillIdValue, std::string const& spellIdValue, std::string const& targetItemIdValue, std::string const& targetBagValue, std::string const& targetSlotValue, std::string const& targetModeValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 skillId = 0;
    uint32 spellId = 0;
    uint32 targetItemId = 0;
    uint8 targetBag = 0;
    uint8 targetSlot = 0;

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, "NO_BOT");
        return;
    }

    PlayerbotAI* const botAI = GetBotAI(bot);
    if (!botAI)
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, "NO_AI");
        return;
    }

    if (!ParseUint32Field(skillIdValue, skillId) || !ParseUint32Field(spellIdValue, spellId) || !ParseUint32Field(targetItemIdValue, targetItemId) || !ParseUint8Field(targetBagValue, targetBag) || !ParseUint8Field(targetSlotValue, targetSlot))
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, "BAD_REQUEST");
        return;
    }

    bool const useTradeTarget = SameName(UrlDecodeField(targetModeValue), "TRADE");
    Item* targetItem = nullptr;
    if (useTradeTarget)
    {
        TradeData* const trade = bot->GetTradeData();
        if (!trade || !trade->GetTraderData())
        {
            SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, "NO_TRADE");
            return;
        }

        // EN: The caster enchants/enhances the trader's not-traded slot, matching the trade window workflow.
        // FR: Le lanceur enchante/améliore l'emplacement non échangé du partenaire, comme dans la fenêtre d'échange.
        targetItem = trade->GetTraderData()->GetItem(TRADE_SLOT_NONTRADED);
    }
    else if (SameName(UrlDecodeField(targetModeValue), "EQUIP"))
        targetItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, targetSlot);
    else
        targetItem = bot->GetItemByPos(targetBag, targetSlot);

    if (!targetItem)
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, useTradeTarget ? "MISSING_TRADE_TARGET" : "MISSING_TARGET_ITEM");
        return;
    }

    if (targetItemId && targetItem->GetEntry() != targetItemId)
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, "TARGET_ITEM_MISMATCH");
        return;
    }

    uint32 actualItemId = 0;
    std::string validation = ValidateProfessionRecipeCraft(bot, skillId, spellId, 0, actualItemId);
    if (validation == "BAD_RECIPE")
        validation = "UNKNOWN_RECIPE";
    if (validation != "OK")
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, validation);
        return;
    }

    SpellInfo const* const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !(spellInfo->Targets & TARGET_FLAG_ITEM || spellInfo->Targets & TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, "INVALID_TARGET_ITEM");
        return;
    }

    BridgeSpellCheckData const check = CheckBridgeSpellCast(bot, botAI, spellId, bot, targetItem);
    if (!check.ok)
    {
        SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, false, check.reason, { std::to_string(static_cast<uint32>(check.result)) });
        return;
    }

    SpellCastResult const result = CastBridgeSpellDirect(bot, spellInfo, bot, targetItem);
    bool const ok = result == SPELL_CAST_OK;
    SendRunResult(requester, replyType, "CRAFT_RECIPE_TARGET", effectiveBotName, token, ok, ok ? "OK" : GetBridgeSpellFailureReason(result), ok ? std::vector<std::string>() : std::vector<std::string>{ std::to_string(static_cast<uint32>(result)) });
}

void RunItemTradeCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& itemIdValue, std::string const& encodedTargetName, std::string const& countValue, std::string const& bagValue, std::string const& slotValue)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    uint32 itemId = 0;
    uint32 count = 0;
    uint8 bag = 0;
    uint8 slot = 0;
    bool const hasPosition = ParseUint8Field(bagValue, bag) && ParseUint8Field(slotValue, slot);
    bool const hasExplicitCount = ParseUint32Field(countValue, count) && count > 0;

    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    if (!bot)
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "NO_BOT", { "0" });
        return;
    }

    if (!ParseUint32Field(itemIdValue, itemId) || !itemId)
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "BAD_REQUEST", { "0" });
        return;
    }

    Player* const target = FindAllowedPlayerTarget(requester, encodedTargetName);
    if (!target || target == bot)
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "INVALID_TARGET", { "0" });
        return;
    }

    if (!target->IsWithinDistInMap(bot, TRADE_DISTANCE, false))
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "OUT_OF_RANGE", { "0" });
        return;
    }

    std::string reason;
    Item* source = nullptr;
    bool oneStackMode = false;

    if (hasPosition)
    {
        source = bot->GetItemByPos(bag, slot);
        if (!source || source->GetEntry() != itemId)
        {
            SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "MISSING_ITEM", { "0" });
            return;
        }
        oneStackMode = !hasExplicitCount && IsStackableItem(source);
    }
    else
    {
        ItemTemplate const* const proto = sObjectMgr->GetItemTemplate(itemId);
        oneStackMode = !hasExplicitCount && proto && proto->GetMaxStackSize() > 1;
    }

    uint32 requestedCount = hasExplicitCount ? count : 1;
    source = FindBestTradeSourceStack(bot, itemId, requestedCount, oneStackMode, bag, slot, hasPosition, reason);

    std::vector<Item*> selectedStacks;
    bool needsSplit = false;
    if (source)
    {
        if (oneStackMode)
            requestedCount = source->GetCount();
        else if (source->GetCount() > requestedCount)
            needsSplit = true;
        selectedStacks.push_back(source);
    }
    else if (hasExplicitCount && !hasPosition)
        selectedStacks = SelectExactTradeStacks(bot, itemId, requestedCount);

    if (selectedStacks.empty())
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, reason.empty() ? "COUNT_UNAVAILABLE" : reason, { "0" });
        return;
    }

    if (!bot->GetTrader())
    {
        if (target->GetTrader() && target->GetTrader() != bot)
        {
            SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "TRADE_BUSY", { "0" });
            return;
        }

        WorldPacket packet(CMSG_INITIATE_TRADE);
        packet << target->GetGUID();
        bot->GetSession()->HandleInitiateTradeOpcode(packet);
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, true, "TRADE_STARTED", { "0" });
        return;
    }

    if (bot->GetTrader() != target)
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "TRADE_BUSY", { "0" });
        return;
    }

    if (GetFreeTradeSlotCount(bot) < selectedStacks.size())
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "TRADE_SLOT_FULL", { "0" });
        return;
    }

    if (needsSplit)
    {
        Item* const splitItem = SplitStackForExactTrade(bot, selectedStacks.front(), requestedCount, reason);
        if (!splitItem)
        {
            SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, reason.empty() ? "COUNT_UNAVAILABLE" : reason, { "0" });
            return;
        }
        selectedStacks.clear();
        selectedStacks.push_back(splitItem);
    }

    for (Item* const tradeItem : selectedStacks)
    {
        if (!tradeItem || !tradeItem->CanBeTraded(false, true))
        {
            SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "ITEM_NOT_TRADABLE", { "0" });
            return;
        }
    }

    uint32 moved = 0;
    for (Item* const tradeItem : selectedStacks)
    {
        if (!tradeItem)
            continue;

        if (!oneStackMode && hasExplicitCount && moved + tradeItem->GetCount() > requestedCount)
        {
            SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "COUNT_UNAVAILABLE", { "0" });
            return;
        }

        if (!AddItemToTradeSlot(bot, tradeItem, reason))
        {
            SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, reason, { std::to_string(moved) });
            return;
        }

        moved += tradeItem->GetCount();
    }

    if (!oneStackMode && hasExplicitCount && moved != requestedCount)
    {
        SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, false, "COUNT_UNAVAILABLE", { std::to_string(moved) });
        return;
    }

    SendRunResult(requester, replyType, "ITEM_TRADE", effectiveBotName, token, moved > 0, moved > 0 ? "OK" : "TRADE_FAILED", { std::to_string(moved) });
}

void RunOutfitCommand(Player* requester, ChatMsg replyType, std::string const& botName, std::string const& requestToken, std::string const& encodedSuffix, std::string const& persistToken)
{
    std::string const trimmedBotName = Trim(botName);
    std::string const token = Trim(requestToken);
    std::string const suffix = SanitizeOutfitCommandSuffix(UrlDecodeField(encodedSuffix));
    Player* const bot = FindBotByName(requester, trimmedBotName);
    std::string const effectiveBotName = bot ? bot->GetName() : trimmedBotName;

    bool ok = false;
    if (bot && IsAllowedOutfitCommandSuffix(suffix))
    {
        if (IsDirectBridgeOutfitCommandSuffix(suffix))
            ok = ApplyBridgeNativeOutfitCommand(bot, suffix);
        else
            ok = ExecuteSilentBotCommand(requester, bot, "outfit " + suffix);

        if (ok && IsUpdateOutfitCommandSuffix(suffix) && Trim(persistToken) == "1")
            ExecuteSilentBotCommand(requester, bot, "nc +chat");
    }

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
        if (!end || *end != '\0' || value <= 0.0 || value > 100.0)
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
    if (!end || *end != '\0' || value <= 0.0 || value > 100.0)
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
        "ll quest",
        "ll skill"
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
        uint32 groupNumber = static_cast<uint32>(std::strtoul(target.c_str(), nullptr, 10));
        if (groupNumber < 1 || groupNumber > 8)
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

    if (scope == "ALL" || scope == "RAID")
        return true;

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

    std::string wire = std::string(kAddonPrefix) + "\t" + opcode;
    if (!payload.empty())
        wire += std::string(1, kFieldSeparator) + payload;

    if (BridgeConsoleLogsEnabled())
        LOG_INFO("playerbots", "MultiBotBridge TX [{}] type={}", wire, static_cast<uint32>(chatType));

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, chatType, LANG_ADDON, player, nullptr, wire.c_str());
    player->SendDirectMessage(&data);
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
    if (!bot)
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

void SendStatePackets(Player* player, ChatMsg replyType)
{
    bool sent = false;
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
        SendAddonPacket(player, replyType, "STATE", out.str());
        sent = true;
    }

    if (!sent)
        SendAddonPacket(player, replyType, "STATES", "");
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

void SendInventoryBulkPackets(Player* requester, ChatMsg replyType, std::string const& requestToken)
{
    std::string const token = Trim(requestToken);
    SendAddonPacket(requester, replyType, "INV_BULK_BEGIN", token);

    for (Player* const bot : GetBridgeVisibleBots(requester))
    {
        // Send inventory bag entries
        SendBagEntryPackets(requester, replyType, bot, token);

        // Send inventory item locations
        PlayerbotAI* const botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
        if (botAI)
        {
            std::vector<Item*> const items = botAI->GetInventoryItems();
            for (Item* const item : items)
            {
                if (!item)
                    continue;

                ItemTemplate const* const proto = item->GetTemplate();
                if (!proto)
                    continue;

                std::string locationLine = ChatHelper::FormatItem(proto, item->GetCount());
                if (item->IsSoulBound())
                    locationLine += " (soulbound)";

                std::ostringstream locationPayload;
                locationPayload << UrlEncodeField(bot->GetName())
                    << kFieldSeparator << token
                    << kFieldSeparator << static_cast<uint32>(item->GetBagSlot())
                    << kFieldSeparator << static_cast<uint32>(item->GetSlot())
                    << kFieldSeparator << proto->ItemId
                    << kFieldSeparator << item->GetCount()
                    << kFieldSeparator << UrlEncodeField(locationLine)
                    << kFieldSeparator << (item->IsSoulBound() ? 1 : 0);
                SendAddonPacket(requester, replyType, "INV_ITEM_LOC", locationPayload.str());
            }
        }

        // Send inventory summary
        InventorySummaryData const summary = BuildInventorySummary(bot);
        std::ostringstream payload;
        payload << UrlEncodeField(bot->GetName())
            << kFieldSeparator << token
            << kFieldSeparator << summary.gold
            << kFieldSeparator << summary.silver
            << kFieldSeparator << summary.copper
            << kFieldSeparator << summary.bagUsed
            << kFieldSeparator << summary.bagTotal;
        SendAddonPacket(requester, replyType, "INV_BULK_ITEM", payload.str());
    }

    SendAddonPacket(requester, replyType, "INV_BULK_END", token);
}

void SendBotSkillsBulkPackets(Player* requester, ChatMsg replyType, std::string const& requestToken)
{
    std::string const token = Trim(requestToken);
    SendAddonPacket(requester, replyType, "BOT_SKILLS_BULK_BEGIN", token);

    for (Player* const bot : GetBridgeVisibleBots(requester))
    {
        for (BotSkillEntryData const& entry : BuildBotSkillEntries(bot))
        {
            std::ostringstream payload;
            payload << UrlEncodeField(bot->GetName())
                << kFieldSeparator << token
                << kFieldSeparator << entry.skillId
                << kFieldSeparator << UrlEncodeField(entry.category)
                << kFieldSeparator << UrlEncodeField(entry.key)
                << kFieldSeparator << UrlEncodeField(entry.name)
                << kFieldSeparator << entry.value
                << kFieldSeparator << entry.maxValue;
            SendAddonPacket(requester, replyType, "BOT_SKILLS_BULK_ITEM", payload.str());
        }
    }

    SendAddonPacket(requester, replyType, "BOT_SKILLS_BULK_END", token);
}

bool HandleBridgeOpcode(Player* player, ChatMsg replyType, std::string const& opcode, std::string const& payload)
{
    std::string const normalized = ToUpper(Trim(opcode));

    if (normalized == "HELLO")
    {
        SendAddonPacket(player, replyType, "HELLO_ACK", std::string(kProtocolVersion) + kFieldSeparator + kBridgeName);
        return true;
    }

    if (normalized == "PING")
    {
        SendAddonPacket(player, replyType, "PONG", payload);
        return true;
    }

    if (normalized == "GET")
    {
        std::pair<std::string, std::string> const request = SplitOnce(payload, kFieldSeparator);
        std::string const requestType = ToUpper(Trim(request.first));

        if (requestType == "ROSTER")
        {
            SendAddonPacket(player, replyType, "ROSTER", BuildRosterPayload(player));
            return true;
        }

        if (requestType == "DETAIL")
        {
            SendAddonPacket(player, replyType, "DETAIL", BuildDetailPayload(player, request.second));

            std::string const professionPayload = BuildProfessionPayload(player, request.second);
            if (!professionPayload.empty())
                SendAddonPacket(player, replyType, "PROFESSION", professionPayload);

            return true;
        }

        if (requestType == "DETAILS")
        {
            SendDetailPackets(player, replyType);
            return true;
        }

        if (requestType == "PROFESSION")
        {
            SendAddonPacket(player, replyType, "PROFESSION", BuildProfessionPayload(player, request.second));
            return true;
        }

        if (requestType == "PROFESSIONS")
        {
            SendProfessionPackets(player, replyType);
            return true;
        }

        if (requestType == "STATE")
        {
            SendAddonPacket(player, replyType, "STATE", BuildStatePayload(player, request.second));
            return true;
        }

        if (requestType == "STATES")
        {
            SendStatePackets(player, replyType);
            return true;
        }

        if (requestType == "TALENT_SPEC_LIST")
        {
            std::pair<std::string, std::string> const specRequest = SplitOnce(request.second, kFieldSeparator);
            SendTalentSpecListPackets(player, replyType, specRequest.first, specRequest.second);
            return true;
        }

        if (requestType == "QUESTS")
        {
            std::pair<std::string, std::string> const modeRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const botRequest = SplitOnce(modeRequest.second, kFieldSeparator);
            SendQuestPackets(player, replyType, modeRequest.first, botRequest.first, botRequest.second);
            return true;
        }

        if (requestType == "GAMEOBJECTS")
        {
            std::pair<std::string, std::string> const gameObjectRequest = SplitOnce(request.second, kFieldSeparator);
            SendGameObjectPackets(player, replyType, gameObjectRequest.first, gameObjectRequest.second);
            return true;
        }

        if (requestType == "GLYPHS")
        {
            std::pair<std::string, std::string> const glyphRequest = SplitOnce(request.second, kFieldSeparator);
            SendGlyphPackets(player, replyType, glyphRequest.first, glyphRequest.second);
            return true;
        }

        if (requestType == "PVP_STATS")
        {
            std::string const botName = Trim(request.second);
            if (botName.empty())
                SendPvpStatsPackets(player, replyType);
            else
                SendAddonPacket(player, replyType, "PVP_STATS", BuildPvpStatsPayload(player, botName));

            return true;
        }

        if (requestType == "STATS")
        {
            std::string const botName = Trim(request.second);
            if (botName.empty())
                SendStatsPackets(player, replyType);
            else
                SendAddonPacket(player, replyType, "STATS", BuildStatsPayload(player, botName));

            return true;
        }

        if (requestType == "INVENTORY")
        {
            std::pair<std::string, std::string> const inventoryRequest = SplitOnce(request.second, kFieldSeparator);
            SendInventorySnapshot(player, replyType, inventoryRequest.first, Trim(inventoryRequest.second));
            return true;
        }

        if (requestType == "INVENTORY_BULK")
        {
            SendInventoryBulkPackets(player, replyType, Trim(request.second));
            return true;
        }

        if (requestType == "BANK")
        {
            std::pair<std::string, std::string> const bankRequest = SplitOnce(request.second, kFieldSeparator);
            SendBankPackets(player, replyType, bankRequest.first, Trim(bankRequest.second));
            return true;
        }

        if (requestType == "GBANK")
        {
            std::pair<std::string, std::string> const bankRequest = SplitOnce(request.second, kFieldSeparator);
            SendGuildBankPackets(player, replyType, bankRequest.first, Trim(bankRequest.second));
            return true;
        }

        if (requestType == "SPELLBOOK")
        {
            std::pair<std::string, std::string> const spellbookRequest = SplitOnce(request.second, kFieldSeparator);
            SendSpellbookSnapshot(player, replyType, spellbookRequest.first, Trim(spellbookRequest.second));
            return true;
        }

        if (requestType == "BOT_SKILLS")
        {
            std::pair<std::string, std::string> const skillRequest = SplitOnce(request.second, kFieldSeparator);
            SendBotSkillPackets(player, replyType, skillRequest.first, Trim(skillRequest.second));
            return true;
        }

        if (requestType == "BOT_SKILLS_BULK")
        {
            SendBotSkillsBulkPackets(player, replyType, Trim(request.second));
            return true;
        }

        if (requestType == "BOT_REPUTATIONS")
        {
            std::pair<std::string, std::string> const reputationRequest = SplitOnce(request.second, kFieldSeparator);
            SendBotReputationPackets(player, replyType, reputationRequest.first, Trim(reputationRequest.second));
            return true;
        }

        if (requestType == "BOT_EMBLEMS")
        {
            std::pair<std::string, std::string> const emblemRequest = SplitOnce(request.second, kFieldSeparator);
            SendBotEmblemPackets(player, replyType, emblemRequest.first, Trim(emblemRequest.second));
            return true;
        }

        if (requestType == "PROFESSION_RECIPES")
        {
            std::pair<std::string, std::string> const recipeBotRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const recipeSkillRequest = SplitOnce(recipeBotRequest.second, kFieldSeparator);
            SendProfessionRecipePackets(player, replyType, recipeBotRequest.first, recipeSkillRequest.first, Trim(recipeSkillRequest.second));
            return true;
        }

        if (requestType == "OUTFITS")
        {
            std::pair<std::string, std::string> const outfitRequest = SplitOnce(request.second, kFieldSeparator);
            SendOutfitPackets(player, replyType, outfitRequest.first, Trim(outfitRequest.second));
            return true;
        }

        if (requestType == "TRAINER")
        {
            std::pair<std::string, std::string> const trainerRequest = SplitOnce(request.second, kFieldSeparator);
            SendTrainerPackets(player, replyType, trainerRequest.first, Trim(trainerRequest.second));
            return true;
        }

        return false;
    }

    if (normalized == "RUN")
    {
        std::pair<std::string, std::string> const request = SplitOnce(payload, kFieldSeparator);
        std::string const requestType = ToUpper(Trim(request.first));

        if (requestType == "OUTFIT")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const commandRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            RunOutfitCommand(player, replyType, botRequest.first, tokenRequest.first, commandRequest.first, commandRequest.second);
            return true;
        }

        if (requestType == "TRAINER_LEARN")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const trainerRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            RunTrainerLearnCommand(player, replyType, botRequest.first, tokenRequest.first, trainerRequest.first, trainerRequest.second);
            return true;
        }

        if (requestType == "CRAFT_RECIPE")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const skillRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const spellRequest = SplitOnce(skillRequest.second, kFieldSeparator);
            RunProfessionRecipeCraftCommand(player, replyType, botRequest.first, tokenRequest.first, skillRequest.first, spellRequest.first, spellRequest.second);
            return true;
        }

        if (requestType == "ITEM_ACTION")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const actionRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const itemRequest = SplitOnce(actionRequest.second, kFieldSeparator);
            RunInventoryItemActionCommand(player, replyType, botRequest.first, tokenRequest.first, actionRequest.first, itemRequest.first, itemRequest.second);
            return true;
        }

        if (requestType == "QUEST_ABANDON")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            RunQuestAbandonCommand(player, replyType, botRequest.first, tokenRequest.first, tokenRequest.second);
            return true;
        }

        if (requestType == "QUEST_SHARE")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const questRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            RunQuestShareCommand(player, replyType, botRequest.first, tokenRequest.first, questRequest.first, questRequest.second);
            return true;
        }

        if (requestType == "ITEM_EQUIP")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const itemRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const slotHintRequest = SplitOnce(itemRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const bagRequest = SplitOnce(slotHintRequest.second, kFieldSeparator);
            RunItemEquipCommand(player, replyType, botRequest.first, tokenRequest.first, itemRequest.first, slotHintRequest.first, bagRequest.first, bagRequest.second);
            return true;
        }

        if (requestType == "ITEM_TRADE")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const itemRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const targetRequest = SplitOnce(itemRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const countRequest = SplitOnce(targetRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const bagRequest = SplitOnce(countRequest.second, kFieldSeparator);
            RunItemTradeCommand(player, replyType, botRequest.first, tokenRequest.first, itemRequest.first, targetRequest.first, countRequest.first, bagRequest.first, bagRequest.second);
            return true;
        }

        if (requestType == "CAST_SPELL")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const spellRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            RunSpellCastCommand(player, replyType, botRequest.first, tokenRequest.first, spellRequest.first, spellRequest.second);
            return true;
        }

        if (requestType == "TALENT_APPLY")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const buildRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            RunTalentApplyCommand(player, replyType, botRequest.first, tokenRequest.first, buildRequest.first, buildRequest.second);
            return true;
        }

        if (requestType == "CRAFT_RECIPE_TARGET")
        {
            std::pair<std::string, std::string> const botRequest = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenRequest = SplitOnce(botRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const skillRequest = SplitOnce(tokenRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const spellRequest = SplitOnce(skillRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const itemRequest = SplitOnce(spellRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const bagRequest = SplitOnce(itemRequest.second, kFieldSeparator);
            std::pair<std::string, std::string> const slotRequest = SplitOnce(bagRequest.second, kFieldSeparator);
            RunProfessionRecipeCraftTargetCommand(player, replyType, botRequest.first, tokenRequest.first, skillRequest.first, spellRequest.first, itemRequest.first, bagRequest.first, slotRequest.first, slotRequest.second);
            return true;
        }

        if (requestType == "COMBAT")
        {
            std::pair<std::string, std::string> const scopeSplit = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const targetSplit = SplitOnce(scopeSplit.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenSplit = SplitOnce(targetSplit.second, kFieldSeparator);

            RunCombatCommand(player, replyType, scopeSplit.first, targetSplit.first, tokenSplit.first, tokenSplit.second);
            return true;
        }

        if (requestType == "POSITION")
        {
            std::pair<std::string, std::string> const scopeSplit = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const targetSplit = SplitOnce(scopeSplit.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenSplit = SplitOnce(targetSplit.second, kFieldSeparator);

            RunPositionCommand(player, replyType, scopeSplit.first, targetSplit.first, tokenSplit.first, tokenSplit.second);
            return true;
        }

        if (requestType == "LOOT")
        {
            std::pair<std::string, std::string> const scopeSplit = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const targetSplit = SplitOnce(scopeSplit.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenSplit = SplitOnce(targetSplit.second, kFieldSeparator);

            RunLootCommand(player, replyType, scopeSplit.first, targetSplit.first, tokenSplit.first, tokenSplit.second);
            return true;
        }

        if (requestType == "RTI")
        {
            std::pair<std::string, std::string> const scopeSplit = SplitOnce(request.second, kFieldSeparator);
            std::pair<std::string, std::string> const targetSplit = SplitOnce(scopeSplit.second, kFieldSeparator);
            std::pair<std::string, std::string> const tokenSplit = SplitOnce(targetSplit.second, kFieldSeparator);

            RunRTICommand(player, replyType, scopeSplit.first, targetSplit.first, tokenSplit.first, tokenSplit.second);
            return true;
        }

        return false;
    }

    return false;
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
        if (!TryExtractBridgePayload(lang, msg, payload))
            return false;

        if (BridgeConsoleLogsEnabled())
            LOG_INFO("playerbots", "MultiBotBridge RX [{}] type={}", payload, type);

        std::pair<std::string, std::string> const packet = SplitOnce(payload, kFieldSeparator);
        return HandleBridgeOpcode(player, NormalizeReplyChatType(type), packet.first, packet.second);
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
