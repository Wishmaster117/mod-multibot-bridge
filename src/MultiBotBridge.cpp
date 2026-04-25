#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "PlayerbotAI.h"
#include "ChatHelper.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "ScriptedGossip.h"
#include "WorldPacket.h"

#include <algorithm>
#include <array>
#include <map>
#include <cctype>
#include <sstream>
#include <set>
#include <string>
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
void SendAddonPacket(Player* player, ChatMsg chatType, std::string const& opcode, std::string const& payload = "");
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

    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (mgr)
    {
        for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
        {
            Player* const bot = it->second;
            if (!bot)
                continue;

            SendQuestPacketsForBot(player, replyType, bot, mode, token);
        }
    }

    SendAddonPacket(player, replyType, "QUESTS_DONE", token + std::string(1, kFieldSeparator) + mode);
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

Player* FindBotByName(Player* player, std::string const& botName)
{
    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (!mgr)
        return nullptr;

    std::string const wantedName = Trim(botName);
    if (wantedName.empty())
        return nullptr;

    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;

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
    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (!mgr)
        return "";

    std::ostringstream out;
    bool first = true;

    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;

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
    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (!mgr)
    {
        SendAddonPacket(player, replyType, "DETAILS", "");
        return;
    }

    bool sent = false;

    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;

        std::string const payload = BuildBotDetailPayload(bot);
        if (payload.empty())
            continue;

        SendAddonPacket(player, replyType, "DETAIL", payload);
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

std::string BuildPvpStatsPayload(Player* player, std::string const& botName)
{
    Player* const bot = FindBotByName(player, botName);
    if (!bot)
        return "";

    return BuildPvpStatsPayload(bot);
}

void SendPvpStatsPackets(Player* player, ChatMsg replyType)
{
    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (!mgr)
        return;

    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;

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
    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (!mgr)
    {
        SendAddonPacket(player, replyType, "STATES", "");
        return;
    }    

    bool sent = false;
    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;

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
    PlayerbotMgr* const mgr = sPlayerbotsMgr.GetPlayerbotMgr(player);
    if (!mgr)
        return;

    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;

        std::string const payload = BuildStatsPayload(bot);
        if (!payload.empty())
            SendAddonPacket(player, replyType, "STATS", payload);
    }
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
            return true;
        }

        if (requestType == "DETAILS")
        {
            SendDetailPackets(player, replyType);
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

        if (requestType == "SPELLBOOK")
        {
            std::pair<std::string, std::string> const spellbookRequest = SplitOnce(request.second, kFieldSeparator);
            SendSpellbookSnapshot(player, replyType, spellbookRequest.first, Trim(spellbookRequest.second));
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
