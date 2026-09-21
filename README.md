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
| **Bot lifecycle** | Structured connect, disconnect and lifecycle state for authorized bot relationships. |
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
| **Group tools** | Formation, Roll and other migrated group controls. |
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
QUEST_PROGRESS_V1
QUEST_PROGRESS_PUSH_V1
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

The next normal project item is a targeted read-only audit of the exact Playerbots selectors behind collective:

```text
follow
attack
stay
```

before any dedicated structured group-order design is proposed.

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
