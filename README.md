# 🍔 Steal & Cook — BakkesMod Plugin

> *"Your restaurant is losing. Time to steal the competition's recipe."*

A Rocket League BakkesMod plugin translating the **Steal & Cook** game-design concept:

| Game concept | Rocket League equivalent |
|---|---|
| Steal recipe from competitors | Collect boost pads without opponents noticing |
| Sus rate | Goes up when you're "seen" (demo'd / caught) |
| Research Center | Hidden tab where you process stolen boost pieces |
| Cooking | Activate the researched recipe for a timed car buff |
| Shop / Automation | Buy upgrades: auto-collect, auto-cook, sus reducers |
| 3 Difficulties | Easy / Normal / Hard — different recipes, different sus rates |

---

## Requirements

| Tool | Version |
|---|---|
| Windows | 10 / 11 (64-bit) |
| Visual Studio | 2022 (Desktop C++ workload) |
| CMake | 3.18+ |
| BakkesMod | Latest ([bakkesmod.com](https://bakkesmod.com)) |
| BakkesMod SDK | [github.com/bakkesmodorg/BakkesModSDK](https://github.com/bakkesmodorg/BakkesModSDK) |

---

## Folder layout

```
StealAndCook/          ← this repo
├── CMakeLists.txt
├── cfg/
│   └── StealAndCook.set
├── include/
│   └── StealAndCook.h
└── src/
    ├── pch.h
    └── StealAndCook.cpp

BakkesModSDK/          ← clone next to this repo (sibling folder)
```

---

## Build steps

### 1 – Clone the BakkesMod SDK (sibling folder)

```bash
git clone https://github.com/bakkesmodorg/BakkesModSDK ../BakkesModSDK
```

### 2 – Configure with CMake

```powershell
# From the StealAndCook root:
cmake -B build -G "Visual Studio 17 2022" -A x64
```

If your SDK is somewhere else:
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
      -DBAKKESMODSDK_PATH="C:/path/to/BakkesModSDK"
```

### 3 – Build (Release)

```powershell
cmake --build build --config Release
```

The output is `build/Release/StealAndCook.dll`.

### 4 – Install

```powershell
cmake --install build --config Release
```

This copies:
- `StealAndCook.dll` → `%APPDATA%/bakkesmod/bakkesmod/plugins/`
- `StealAndCook.set` → `%APPDATA%/bakkesmod/bakkesmod/plugins/settings/`

Or copy manually.

### 5 – Load in BakkesMod

Open Rocket League → F2 → Plugins tab → tick **Steal & Cook**.

---

## In-game usage

### Opening the window
`F2` → Plugins → **Steal & Cook** — or bind a key:
```
bind F7 "togglemenu stealandcook"
```

### Console commands

| Command | Action |
|---|---|
| `sc_start_easy` | Start Easy mission |
| `sc_start_normal` | Start Normal mission |
| `sc_start_hard` | Start Hard mission |
| `sc_collect` | Manually collect one recipe piece |
| `sc_cook` | Activate the researched recipe buff |
| `sc_enabled 0/1` | Disable / enable plugin |
| `sc_hud_visible 0/1` | Hide / show HUD window |

### Gameplay loop

1. **Start a mission** (Mission tab) — choose Easy / Normal / Hard.
2. **Collect boost pads** in a Freeplay or match — each pickup smuggles one recipe piece to your Research Center.
3. **Avoid being demo'd** — every demo spikes your Sus rate. At 100% the mission fails.
4. **Cook** the recipe once all pieces are collected (Research Center tab).
5. **Spend earnings** in the Shop on automation upgrades.
6. Repeat for the next stage recipe!

### Recipes (example — randomised each run)

**Easy**
| Recipe | Speed | Boost mult | Jump | Duration |
|---|---|---|---|---|
| Ketchup Rush | +50 | 1.0× | 1.05× | 20s |
| Cheese Glide | +30 | 0.85× | 1.10× | 25s |
| Lettuce Drift | +40 | 0.90× | 1.08× | 18s |

**Normal**
| Recipe | Speed | Boost mult | Jump | Duration |
|---|---|---|---|---|
| Patty Overdrive | +120 | 1.0× | 1.15× | 30s |
| Secret Sauce Mk1 | +80 | 0.75× | 1.20× | 35s |
| Onion Ring Boost | +100 | 1.10× | 1.18× | 28s |

**Hard**
| Recipe | Speed | Boost mult | Jump | Duration |
|---|---|---|---|---|
| Ghost Pepper Nitro | +250 | 1.0× | 1.30× | 45s |
| Triple Bypass | +180 | 0.60× | 1.40× | 50s |
| Black Market Burger | +220 | 0.70× | 1.35× | 55s |

### Shop upgrades

| Item | Cost | Effect |
|---|---|---|
| Sneaker Soles | $200 | −30% sus increase per sighting |
| Auto-Collector Mk1 | $350 | Steals a piece automatically every 3s |
| Auto-Cook Oven | $500 | Auto-activates buff when mission succeeds |
| Decoy Burger | $150 | +1.5 sus/s decay bonus |
| Research Bot | $600 | Faster piece collection (stacks with Auto-Collector) |
| Invisibility Cloak | $800 | −50% sus increase rate |

---

## How the BakkesMod hooks work

| Hook | Purpose |
|---|---|
| `TAGame.Car_TA.Tick` | Drives sus decay, buff timer countdown, speed bonus, auto-collect |
| `TAGame.Car_TA.EventBoostActivated` | Counts as "stealing a recipe piece" |
| `TAGame.Car_TA.Demolish` | Being demo'd = being "seen by cooks" → sus spike |
| `GameEvent_Soccar_TA.Active.StartRound` | Resets buff and sus on new round |

The speed bonus is applied by scaling car velocity each tick.  
The boost multiplier uses `sv_soccar_booststrength` (Freeplay / Custom Training only).

---

## Notes & caveats

- The **speed bonus and boost multiplier are Freeplay-only** in competitive contexts (as with all BakkesMod game-affecting plugins). In online matches these values are server-controlled and cannot be altered — the plugin still tracks sus/recipe state but won't apply the car buffs.
- This plugin is for **fun/training use only**. Do not use any game-altering features in ranked matches.

---

## License

MIT — do whatever you want, just don't sell stolen recipes.
