# Building My RPG Part 3 — Melee Combat System Showcase
 
![Unreal Engine](https://img.shields.io/badge/Unreal_Engine-5.0%2B-blue?logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-Gameplay_Programming-blue)
![Architecture](https://img.shields.io/badge/Architecture-Modular-green)
![Combat](https://img.shields.io/badge/System-Melee_Combat-red)
![GAS](https://img.shields.io/badge/GAS-Integrated-purple)
 
---
 
> **This is not a plug-and-play package.**
> This code was extracted from a larger Unreal Engine 5 RPG project and shared as a **reference implementation** for combat architecture, ownership boundaries, and gameplay engineering decisions.
> It is designed to be studied, adapted, and transplanted into another project with context — not dropped into a blank project without modification.
 
---
 
## Overview
 
This repository showcases a modular **Melee Combat System** built in **C++ for Unreal Engine 5**, integrated with the **Gameplay Ability System (GAS)**.
 
The goal is not just to make melee attacks "work" — it's to structure the system so it remains readable, scalable, and safe as the combat layer grows.
 
**Core design goals:**
 
- Keep combat rules inside components, not in signal layers
- Keep the character actor focused on orchestration instead of gameplay rule ownership
- Use animation notifies as triggers, not as rule containers
- Support reliable melee hit registration beyond raw overlap events
- Separate damage application from combat presentation
- Keep combat feedback modular so it can be reused across multiple enemies
 
---

<div align="center">
  <img src="Gifs/CombatSystem.gif" width="100%">
  <p><em>Tooltip generation driven from item data and Gameplay Effect modifier definitions.</em></p>
</div>
 
## What This System Covers
 
| Feature | Description |
|---|---|
| Combo chaining | Basic melee combo system |
| Input buffering | Smoother combo timing via buffered requests |
| Animation-driven windows | Combo, hit, and movement interrupt windows |
| Reliable hit detection | Overlap + sweep-based fallback tracing |
| Crit damage | Roll-based critical hit support |
| Hit stop | Per-hit impact pause |
| Camera shake | Weapon-data-driven screen feedback |
| Overhead health bar | World-space health feedback widget |
| Pooled combat text | Reusable combat text entries |
| Crit text styling | Distinct crit-specific text behavior |
| Damage receiver | Dedicated component for incoming damage |
| Feedback component | Decoupled combat feedback forwarding |
| Dummy target | Lightweight in-editor combat test target |
 
---
 
## Architecture
 
```text
[ Input / Animation Notify ]
            │
            ▼
[ AHeroCharacter ]                  ← Orchestration only
            │
            ▼
[ UCombatComponent ]                ← Combo rules, hit windows, tracing, crits, hit stop
            │
            ▼
[ HitActor->TakeDamage(...) ]       ← Standard engine entry point
            │
            ▼
[ UDamageReceiverComponent ]        ← Damage validation, clamping, health reduction
            │
            ▼
[ UCombatFeedbackComponent ]        ← Feedback forwarding and overhead widget binding
            │
            ▼
[ UOverheadHealthBarWidget ]        ← World-space health + combat text pool owner
            │
            ▼
[ UCombatTextEntryWidget ]          ← Individual pooled combat text item
```
 
**Supporting data and ownership:**
 
| Layer | Responsibility |
|---|---|
| `UWeaponDataAsset` | Combo montages, damage config, hit stop, camera shake |
| `UInventoryComponent` | Equipped weapon state, draw/holster gating, montage safety |
| `UCharacterAttributeSet` | Health, damage, crit chance, movement speed |
| `ACharacterBase` | Shared character orchestration and weapon mesh state |
 
Each layer has a focused responsibility:
 
- `UCombatComponent` owns melee gameplay rules
- `UDamageReceiverComponent` owns incoming damage application
- `UCombatFeedbackComponent` owns combat feedback routing
- `UOverheadHealthBarWidget` owns overhead presentation and combat text pooling
- `ACharacterBase` owns shared actor-level orchestration and mesh state
- Animation notifies only open, close, or signal runtime windows
 
---
 
## Key Features
 
### Modular Combat Ownership
 
The combat slice is intentionally split by responsibility:
 
- `UCombatComponent` — attack flow, combo state, melee hit registration, crit rolls, interrupt logic, and hit stop
- `UDamageReceiverComponent` — incoming damage validation and health reduction
- `UCombatFeedbackComponent` — overhead widget discovery and damage feedback forwarding
- `UOverheadHealthBarWidget` — world-space combat text pooling and presentation
- `ACharacterBase` — engine-facing orchestration layer
 
This keeps gameplay rules out of UI and keeps UI concerns out of damage application.
 
---
 
### Animation-Driven Combat Windows
 
Combat windows are opened and closed by animation notifies and notify states:
 
- Combo advance window
- Melee hit window
- Movement interrupt window
- Combo reset notify
 
> **Important:** The notify layer does not own combat rules. It only signals `UCombatComponent`, which keeps rule ownership consistent.
 
---
 
### Input Buffering
 
`UCombatComponent` supports a short attack input buffer so the player can press attack slightly before the combo window opens.
 
This improves melee feel without introducing a large command queue. The current implementation uses:
 
- A single buffered attack request
- Time-based expiry
- Automatic consumption when the combo window opens
- Cleanup on montage end or invalid state transitions
 
---
 
### Reliable Melee Hit Detection
 
The combat system does not rely only on overlap events. During active melee hit windows, it also performs a **sweep-based reliability trace** using the current and previous weapon collision positions.
 
This helps reduce missed hits caused by:
 
- Fast swings
- Low frame rate
- Overlap timing edge cases
 
The result is a more production-safe melee hit path than overlap-only setups.
 
---
 
### Damage Receiver Ownership
 
Incoming damage is not resolved directly inside `ACharacterBase`. Instead, `ACharacterBase::TakeDamage(...)` forwards the request to `UDamageReceiverComponent`, which handles:
 
- Damage validation
- Zero/negative damage rejection
- Health clamping
- Health attribute reduction through GAS-friendly attribute modification
 
This keeps damage logic in a component layer instead of bloating the base actor.
 
---
 
### Combat Feedback Separation
 
Damage presentation is not owned by the combat rules component. `UCombatFeedbackComponent` listens after damage is applied and forwards the result to the overhead widget layer.
 
This means:
 
- Gameplay rules stay in gameplay components
- Presentation stays in UI
- Characters do not need direct combat text logic inside their base class
 
---
 
### Pooled Combat Text
 
World-space combat text is managed by `UOverheadHealthBarWidget`, which owns a local pool of `UCombatTextEntryWidget` instances. This avoids unnecessary widget creation churn during repeated hits and keeps pooling local to the presentation layer.
 
Current combat text behavior:
 
- Normal damage — white text with black outline, rises upward and fades out
- Crit damage — red text with black outline, spawns slightly higher with a fast scale punch instead of upward movement
 
---
 
### Hit Stop and Camera Shake
 
Weapon data can drive local impact feel through hit stop and camera shake. `UCombatComponent` applies these only when a hit is successfully registered, keeping feedback tied to confirmed combat outcomes.
 
---
 
### Async Safety for Combat Assets
 
Combo montages and weapon-related assets can be soft-referenced and loaded asynchronously. The system includes state guards so that stale async completions do not incorrectly apply old data after runtime state has already changed.
 
This is especially important for:
 
- Combo montage loading
- Weapon draw/holster transitions
- Gameplay states changing while assets are still loading
 
---
 
## Relevant Files
 
### Core Combat Logic
 
| File | Responsibility |
|---|---|
| `CombatComponent.cpp` | Combo state, input buffering, notify window handling, reliable melee tracing, crit resolution, hit stop, camera shake |
| `DamageReceiverComponent.cpp` | Damage validation, applied damage resolution, health reduction |
| `CombatFeedbackComponent.cpp` | Overhead widget discovery, feedback forwarding, critical-hit feedback routing |
 
### Character Orchestration
 
| File | Responsibility |
|---|---|
| `CharacterBase.cpp` | Shared actor orchestration, weapon mesh state, engine damage entry point, component ownership |
| `HeroCharacter.cpp` | Player-facing combat orchestration and attack input routing |
| `DummyTargetCharacter.cpp` | Lightweight combat test target |
 
### Animation Signal Layer
 
| File | Responsibility |
|---|---|
| `AN_ResetCombo.cpp` | Signals combo reset |
| `ANS_ComboWindow.cpp` | Signals combo advance window |
| `ANS_MeleeHitWindow.cpp` | Signals melee hit activation window |
| `ANS_AttackMoveInterruptWindow.cpp` | Signals attack movement cancel window |
 
### UI and Feedback
 
| File | Responsibility |
|---|---|
| `OverheadHealthBarWidget.cpp` | Overhead health display, combat text pooling, combat text animation |
| `CombatTextEntryWidget.cpp` | Single combat text entry presentation |
| `PlayerVitalsWidget.cpp` | Shared vitals interpolation logic reused by the overhead widget |
 
### Supporting Dependencies
 
| File | Responsibility |
|---|---|
| `InventoryComponent.cpp` | Equipped weapon ownership, draw/holster transitions, weapon action safety |
| `WeaponDataAsset.h` | Combo montage lists, weapon tuning, hit stop config, camera shake config |
| `CharacterAttributeSet.h` | Health, damage, crit chance, movement speed, and other combat-relevant attributes |
 
---
 
## How It Works
 
```
1. Player Input
   └─ Player presses attack through the character input flow.
 
2. Combat Request
   └─ AHeroCharacter routes the request into UCombatComponent.
 
3. Combo Processing
   └─ UCombatComponent validates combat readiness, evaluates combo state,
      and plays or loads the correct combo montage.
 
4. Animation Windows
   └─ Animation notifies signal combo windows, hit windows, and interrupt
      windows back into UCombatComponent.
 
5. Hit Registration
   └─ During an active melee hit window, the combat component uses overlap
      plus reliability tracing to register valid hits.
 
6. Damage Application
   └─ The target actor receives Unreal's TakeDamage(...) call, which is
      resolved by UDamageReceiverComponent.
 
7. Feedback Dispatch
   └─ After applied damage is known, UCombatFeedbackComponent forwards it
      to the overhead widget layer.
 
8. Combat Text Presentation
   └─ UOverheadHealthBarWidget acquires or reuses a pooled
      UCombatTextEntryWidget and animates the damage text in world space.
```
 
---
 
## Requirements / Assumptions
 
This showcase assumes a project setup with:
 
- Unreal Engine 5
- C++
- UMG
- Gameplay Ability System
- Gameplay Tags
- Weapon definitions authored as `UWeaponDataAsset`
- A character architecture compatible with `ACharacterBase`
- Animation assets using the expected combat notifies
- Overhead widget blueprints with the expected bindable names (e.g. `DamageTextLayer`)
 
It also assumes a melee-capable character setup where:
 
- A weapon can be equipped and drawn
- The character owns a valid `AbilitySystemComponent`
- The target can receive damage through the standard engine damage entry point
 
---
 
## Current Scope
 
This repository focuses on the **melee combat slice only**.
 
**Demonstrated in this showcase:**
 
- ✅ Combo melee flow
- ✅ Buffered attack input
- ✅ Animation-driven combat windows
- ✅ Reliable melee hit confirmation
- ✅ Crit-based damage feedback
- ✅ Hit stop and camera shake
- ✅ World-space combat feedback
 
**Out of scope for this showcase** (belong to the larger combat architecture):
 
- ❌ Spells or projectile combat
- ❌ Defense outcomes (dodge, parry, block)
- ❌ Death flow
- ❌ AI combat decision-making
- ❌ Multiplayer authority and replication rules
- ❌ Faction filtering
- ❌ Full damage-type or reaction-result pipelines
 
---
 
## Design Philosophy
 
Combat systems should stay readable as they become more reactive.
 
This implementation prioritizes:
 
- Clear ownership boundaries
- Component-driven gameplay rules
- Signal layers that only trigger state
- Presentation separated from gameplay logic
- Event-driven feedback instead of broad polling
- Local pooling where it provides clear value
- Lifecycle-safe handling of animation and async state
- Minimum effective abstractions instead of premature overengineering
 
Or put simply:
 
```
the input layer     → signals
the character       → orchestrates
the combat component → owns combat rules
the damage receiver → owns damage application
the feedback component → owns feedback routing
the widget layer    → renders the result
```
 
---
 
## License
 
Provided as a reference implementation for Unreal Engine 5 melee combat architecture built around GAS-style gameplay systems.
 
Feel free to study it, adapt it, and use it as a foundation for your own project.
 
