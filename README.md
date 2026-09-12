<div align="center">

# mod-multibot-bridge

### AzerothCore server-side companion for MultiBot Chatless

<strong>mod-multibot-bridge</strong> connects the
<a href="https://github.com/Wishmaster117/MultiBot-Chatless">MultiBot Chatless</a>
WoW 3.3.5a addon to AzerothCore and `mod-playerbots` through structured addon messages.

<br>

<img alt="Core" src="https://img.shields.io/badge/core-AzerothCore-orange" />
<img alt="Protocol" src="https://img.shields.io/badge/protocol-MBOT-success" />
<img alt="Architecture" src="https://img.shields.io/badge/architecture-bridge--first-blue" />

<br>

<img alt="Linux build" src="https://github.com/Wishmaster117/mod-multibot-bridge/actions/workflows/linux-build.yml/badge.svg?branch=main" />
<img alt="Windows build" src="https://github.com/Wishmaster117/mod-multibot-bridge/actions/workflows/windows-build.yml/badge.svg?branch=main" />
<img alt="macOS build" src="https://github.com/Wishmaster117/mod-multibot-bridge/actions/workflows/macos-build.yml/badge.svg?branch=main" />

</div>

---

## Companion Addon Required

This repository contains the **server module**.

The visible UI is provided by:

### [`MultiBot-Chatless`](https://github.com/Wishmaster117/MultiBot-Chatless)

Without the addon, this module has nothing to display.
Without this module, the addon cannot use its structured bridge-first features.

---

# What the Bridge Does

The Bridge replaces many automatic command/reply chat workflows with structured requests and responses.

```text
MultiBot UI
  -> MBOT addon message
  -> mod-multibot-bridge
  -> AzerothCore / validated Playerbots integration
  -> structured result
  -> MultiBot UI refresh
```

It is an **adaptation layer**, not a generic remote command console.

The module does **not** expose an arbitrary Playerbots command executor.

---

# Main Supported Areas

| Area | Purpose |
| --- | --- |
| **Handshake & capability negotiation** | Detect Bridge availability and negotiate supported feature families. |
| **Roster & presence** | Provide bridge-visible bots, account-alt presence and structured roster data. |
| **Bot lifecycle** | Structured unit connect/disconnect/state, bounded real-group bulk connect/disconnect for the Faction Banner, plus human-safe grouped-Playerbot removal for Raidus. |
| **Creator AddClass** | Dedicated `CREATOR_ADDCLASS_V1` endpoint for validated class/gender AddClass requests without exposing an arbitrary Playerbots command channel. |
| **Target resolution** | Resolve an authorized bot name to the canonical lifecycle target used by social rosters. |
| **Bot state** | Framed strategy/state reads used by the addon UI. |
| **Strategy mutations** | Structured strategy changes for migrated controls. |
| **Inventory** | Standard and exact physical inventory snapshots. |
| **Item actions** | Move, equip, unequip, use, destroy, Trade, vendor sale and related structured results. |
| **Bank / Guild Bank** | Structured views/actions including exact physical deposits. |
| **Talents** | Premade specialization and custom talent application with server-side validation. |
| **Professions** | Recipe listing/crafting and exact item-target recipes. |
| **Enchanting** | Dedicated Enchanting Trade Service using the native Trade workflow. |
| **Quests** | Structured quest data and bot quest abandon. |
| **Loot** | Loot-profile control and persistent exact always-loot item rules. |
| **Group tools** | Formation, Roll and other migrated controls, plus dedicated `FOLLOW_ORDER_V1`, `STAY_ORDER_V1`, `ATTACK_ORDER_V1`, `FLEE_ORDER_V1`, bounded `GROUP_ACTION_V1` and `RTSC_ORDER_V1` endpoints. |
| **SelfBot** | Dedicated SelfBot state, strategy and selected action endpoints. |
| **Character information** | Stats, PvP stats, skills, reputations, currencies/emblems, spellbook and related data. |
| **Outfits** | Structured outfit listing and actions. |

---

# Alt Roster & Bot Lifecycle

The lifecycle milestone merged on **30 August 2026** adds the capabilities:

```text
ALT_ROSTER_V1
BOT_LIFECYCLE_V1
BOT_TARGET_RESOLVE_V1
```

These services support the companion addon's My Bots, Group, Guild, Friends and Favorites rosters.

The Bridge:

- exposes account-alt online/offline presence;
- resolves authorized bot targets by canonical character identity;
- handles structured bot connect/disconnect/state;
- keeps in-flight asynchronous connects reserved after the short reporting timeout until completion is observed or the longer retention deadline expires;
- prevents a simple offline group-membership relationship from granting lifecycle control by itself.

The final authorization relationship is limited to the audited control relationships used by the project, including same-account, same-guild, AddClass and linked/trusted-account cases.

---

# Bulk Group Lifecycle

The Faction Banner bulk lifecycle path uses the dedicated capability:

```text
BOT_GROUP_LIFECYCLE_V1
```

The endpoint accepts only the bounded actions `CONNECT` and `DISCONNECT`. Its scope is copied from the requester's current server-side `Group::MemberSlotList`, with the requester excluded and a maximum of 39 targets.

The Bridge preserves the audited Playerbots lifecycle model:

- `CONNECT` delegates actual login to `PlayerbotMgr::AddPlayerBot(...)`;
- `DISCONNECT` applies only to currently managed Playerbots and delegates logout to `PlayerbotMgr::LogoutPlayerBot(...)`;
- disconnect does **not** force-remove the group slot;
- authorization, pending-connect accounting, lifecycle rate limiting and replay protection stay server-side;
- connected/non-managed members are not logged out by the Bridge;
- no direct `RemoveFromPlayerbotsMap()` or `WorldSession::LogoutPlayer()` path is introduced.

This intentionally preserves the historical Playerbots group `*` behavior without exposing a generic bulk command executor.

---

# Creator AddClass

The Creator AddClass flow uses the dedicated capability:

```text
CREATOR_ADDCLASS_V1
```

The addon sends only the validated semantic fields `class` and `gender`. The Bridge accepts only the audited class whitelist and `random` / `male` / `female` gender values, then constructs the specialized Playerbots `addclass` operation server-side.

The Bridge does **not** accept a raw Playerbots command from the addon and does not expose a generic Playerbots executor. Existing Playerbots AddClass behavior remains authoritative, including permission checks, AddClass pool selection and Death Knight level restrictions.

Runtime validation on **6 September 2026** confirmed Random, Male, Female and Death Knight AddClass flows, preserved addon auto-group/roster behavior, and no legacy `.playerbot bot addclass ...` SAY on the normal bridge-first path.

`init=auto` remains outside this endpoint and is intentionally unchanged pending its own targeted migration.

---

# Raidus Safe Group Removal

Raidus uses the dedicated capability:

```text
BOT_GROUP_REMOVE_V1
```

This path is intentionally narrower than a generic group-kick endpoint.

For a Raidus outside-layout removal, the Bridge:

1. resolves the requested target;
2. proves that the target is currently managed by `PlayerbotMgr::GetPlayerBot(targetGuid)`;
3. requires the requester and target to be in the same normal party/raid;
4. rejects LFG, battleground and battlefield groups;
5. rejects removal of the group leader;
6. reuses AzerothCore's normal `CanUninviteFromGroup()` permission semantics;
7. logs the bot out through `PlayerbotMgr::LogoutPlayerBot(targetGuid)`;
8. rechecks group state and removes only a residual group membership if it still exists;
9. returns the final structured lifecycle result.

A normal connected human outside the Raidus layout is therefore not eligible for this mutation: group membership, account, guild or other client-visible properties are not substitutes for proving that the target is an active managed Playerbot.

---

# Structured Group Orders — Follow / Stay / Attack

The collective Follow, Stay and Attack controls use dedicated bounded endpoints rather than arbitrary Playerbots command execution.

Advertised capabilities:

```text
FOLLOW_ORDER_V1
STAY_ORDER_V1
ATTACK_ORDER_V1
FLEE_ORDER_V1
```

Follow and Stay reuse the audited Playerbots shortcut actions directly without routing through `HandleCommand()`. Attack uses a Bridge-local adapter over `AttackAction::Attack(Unit*)`, because the stock `AttackMyTargetAction` resolves the target from the bot master rather than from the requesting player.

Attack audiences preserve the audited Playerbots selector semantics:

```text
ALL
TANK   -> IsTank(bot)
HEALER -> IsHeal(bot)
DPS    -> !IsTank(bot) && !IsHeal(bot)
MELEE  -> !IsRanged(bot)
RANGED -> IsRanged(bot)
```

The Attack target is resolved server-side from `requester->GetTarget()`. Group scope, per-bot security, rate limiting, replay protection and bounded bot counts are enforced on the Bridge. ACKs are returned through structured addon messages.

No generic `RUN~ORDER` endpoint is exposed.

---

# Structured Flee Order

Flee uses the dedicated capability:

```text
FLEE_ORDER_V1
```

Supported audiences are `ALL`, `TARGET`, `TANK`, `HEALER`, `DPS`, `MELEE` and `RANGED`.

The Bridge invokes the audited Playerbots `flee chat shortcut` action rather than duplicating its strategy reset, follow/stay/passive changes or movement semantics. Role selection stays authoritative on the server through the same audited role predicates used by the Bridge (`BotMatchesAttackAudience`).

For role-scoped requests, the Bridge emits one bounded result item per matched bot before the final aggregate ACK:

```text
FLEE_ORDER_ITEM~token~audience~encodedBotName~OK|ERR
FLEE_ORDER_ACK~token~audience~matched~succeeded~failed~reason
```

`FLEE_ORDER_ITEM` uses the existing state-packet wire-budget guard, the matched-bot scope remains capped at 40, and the final ACK schema remains compatible. If the addon cannot assemble a complete authoritative role-name set, it falls back to count feedback instead of displaying an incomplete list.

Requester/session validation, group scope, per-bot Playerbots security, rate limiting and replay protection remain server-side. The Bridge still exposes no generic Playerbots command executor.

Runtime validation on **11 September 2026** confirmed ALL, controlled TARGET, non-bot TARGET rejection, all role audiences, authoritative bot-name feedback and no normal Flee success-whisper spam on the chatless path.

---

# Structured Group Actions

The bounded Group Actions family uses the dedicated capability:

```text
GROUP_ACTION_V1
```

Only four semantic actions are accepted:

```text
DRINK   -> "drink"
RELEASE -> "release"
REVIVE  -> "spirit healer"
SUMMON  -> "summon"
```

The addon does not provide a raw Playerbots command string. The Bridge validates the action against this closed allowlist, revalidates requester/group/bot state and Playerbots security, and reuses the existing group-order rate-limit, replay and 40-bot scope protections before invoking the audited native Playerbots action with `DoSpecificAction(...)`.

Runtime validation on **11 September 2026** confirmed all four actions, including the Release-to-ghost then Revive-at-spirit-healer flow. Structured `GROUP_ACTION_ACK` responses were observed and no automatic PARTY/RAID legacy command transport was observed with the normal fallback policy disabled.

`mod-playerbots` remains strictly read-only.

---

# Structured RTSC Order

RTSC uses the dedicated capability:

```text
RTSC_ORDER_V1
```

The endpoint accepts only the bounded semantic operations `ENABLE`, `RESET`, `SELECT`, `CANCEL`, `SAVE`, `UNSAVE` and `GO`. Audiences are `ALL`, `TANK`, `HEALER`, `DPS`, `MELEE`, `RANGED`, `MELEE_DPS`, `RANGED_DPS` and `GROUPS`; group masks and saved-slot values are validated server-side.

The Bridge adapts these requests to the audited native Playerbots RTSC action with:

```cpp
botAI->DoSpecificAction("rtsc", Event("rtsc", nativeParam, requester), true);
```

It does not accept raw coordinates, does not synthesize `SpellCastTargets`, and does not reimplement the AEDM cast/movement pipeline. `/cast aedm` remains a native WoW spell cast handled by Playerbots.

The RTSC path reuses the validated requester/group scope, per-bot Playerbots security, 40-bot bound, rate limiting and replay protection. Runtime validation on **12 September 2026** covered all role audiences, groups 1..5, multi-group selection, SAVE/GO/UNSAVE, CANCEL and native AEDM movement behavior.

A post-RTSC warning cleanup re-exposes the AzerothCore base `OnPlayerCanUseChat` overload set with a `using` declaration and removes one definition-only guild-bank helper. Windows build and runtime smoke tests passed; disappearance of the corresponding GCC warnings remains to be confirmed on the next Linux build of this branch.

---

# Security Model

All addon input is treated as untrusted.

Bridge write paths are expected to validate, as applicable:

- requester/session/world state;
- bot identity and the requester's right to control it;
- numeric ranges and exact field formats;
- map/session/runtime state;
- exact item identity and physical source where required;
- request frequency;
- replayed request tokens;
- endpoint-specific limits and result postconditions.

Important design rules:

- no generic arbitrary Playerbots command executor;
- no authorization based only on client-side UI checks;
- no assumption that addon-provided item, bot or state data is still current;
- structured success should reflect authoritative server state.

`mod-playerbots` remains an external dependency and is not modified by this module's MultiBot project workflow.

---

# Capability Negotiation

The Bridge currently advertises a growing set of dedicated capabilities, including:

```text
STATE_FRAMING_V1
STRATEGY_MUTATION_V1
OUTFIT_V1
INVENTORY_V1
INVENTORY_EXACT_V1
ITEM_MOVE_V1
ITEM_TRADE_V1
ITEM_DEPOSIT_EXACT_V1
ITEM_EQUIP_V1
ITEM_UNEQUIP_V1
ITEM_DESTROY_V1
ITEM_USE_V1
ITEM_SELL_SINGLE_V1
VENDOR_BUYBACK_V1
INVENTORY_BULK_SELL_V1
INVENTORY_OPEN_V1
LOOT_RULE_ITEM_V1
QUEST_ABANDON_V1
TALENT_APPLY_V1
TALENT_SPEC_APPLY_V1
CRAFT_RECIPE_TARGET_V1
GROUP_ROLL_V1
ENCHANT_TRADE_V1
SELF_BOT_V1
SELF_STRATEGY_V1
SELF_ACTION_V1
ALT_ROSTER_V1
BOT_LIFECYCLE_V1
BOT_TARGET_RESOLVE_V1
BOT_GROUP_REMOVE_V1
BOT_GROUP_LIFECYCLE_V1
FOLLOW_ORDER_V1
STAY_ORDER_V1
ATTACK_ORDER_V1
FLEE_ORDER_V1
GROUP_ACTION_V1
RTSC_ORDER_V1
```

The exact packet schemas are implementation details shared with the addon and may evolve with negotiated capability versions.

---

# Requirements

- AzerothCore WotLK.
- `mod-playerbots` installed and working.
- A normal AzerothCore module build environment.
- The companion [`MultiBot-Chatless`](https://github.com/Wishmaster117/MultiBot-Chatless) addon for the client UI.

---

# Installation

Clone the module into the AzerothCore `modules` directory:

```bash
cd /path/to/azerothcore/modules
git clone https://github.com/Wishmaster117/mod-multibot-bridge.git mod-multibot-bridge
```

Then run the normal AzerothCore CMake/build workflow for your environment.

The module provides:

```text
conf/MultiBotBridge.conf.dist
```

Make sure the module configuration is available to the worldserver installation you actually run.

---

# Current Status

The Bridge is the primary adaptation layer for the MultiBot Chatless project and now covers the major UI refresh families plus a substantial set of validated write actions.

The project is intentionally described as **bridge-first / mostly chatless** until all remaining automatic legacy chat paths have been audited and either migrated, intentionally retained or removed.

Collective **Follow**, **Stay** and **Attack** are implemented through dedicated structured Bridge endpoints and runtime validated. Their technical ACKs no longer depend on automatic chat transport or parsing, and the Bridge still exposes no generic Playerbots command executor.

**Flee** is now also implemented and runtime validated through `FLEE_ORDER_V1`. The Bridge remains authoritative for Tank / Healer / DPS / Melee / Ranged matching and returns per-bot `FLEE_ORDER_ITEM` results before the aggregate ACK so the addon can display the exact selected bot names without reproducing role logic client-side.

The historical group bulk pair `.playerbot bot add *` / `.playerbot bot remove *` is migrated through `BOT_GROUP_LIFECYCLE_V1`. Runtime validation confirmed structured disconnect/reconnect of grouped Playerbots without forcing group-slot removal, while the actual login/logout operations remain delegated to Playerbots.

Creator `addclass` is migrated through the specialized `CREATOR_ADDCLASS_V1` endpoint and runtime validated without a generic command proxy. Obsolete Units/lifecycle legacy cleanup stays deferred to the final global fallback/parser cleanup.

The bounded Group Actions set `drink`, `release`, `revive` and `summon` is migrated through `GROUP_ACTION_V1`. RTSC is now also migrated and runtime validated through `RTSC_ORDER_V1`, while AEDM intentionally remains on the native WoW/Playerbots spell path. The next active migration is Quest interactions, followed by remaining ordinary-bot actions and final legacy parser/fallback cleanup.

The project remains intentionally **bridge-first / mostly chatless** while the remaining chat families are audited and migrated independently.

Deferred work is tracked in the addon roadmap:

### [`MultiBot-Chatless/docs/ROADMAP.md`](https://github.com/Wishmaster117/MultiBot-Chatless/blob/main/docs/ROADMAP.md)

---

# Credits

This module is part of the MultiBot Chatless project and depends on AzerothCore and `mod-playerbots`.

Historical Jellypowered bridge contributions were audited and selectively adapted rather than merged blindly. Attribution and the relevant commit references are preserved in the addon roadmap.

---

# Troubleshooting

### The module does not load

Confirm that:

- the module is under the AzerothCore `modules` directory;
- CMake detected it;
- the server was rebuilt;
- the module configuration is installed where the running worldserver expects it.

### The addon reports Bridge unavailable

Confirm the client is using the matching MultiBot Chatless addon and that the worldserver loaded `mod-multibot-bridge`.
