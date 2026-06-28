#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "imgui/imgui.h"
#include <string>
#include <vector>
#include <map>
#include <random>

// ============================================================
//  STEAL & COOK  –  BakkesMod Plugin
//  Inspired by the "Steal and Cook" game design doc.
//
//  Rocket-League translation:
//    Recipes   → Boost "formulas" (pad combos that give secret buffs)
//    Stealing  → Collecting opponent boost pads without being noticed
//    Sus Rate  → Goes up when opponents see you near their pads
//    Lab       → Off-field hidden zone where you "process" stolen boost
//    Cooking   → Activate collected formula for a timed car buff
//    Automation→ Auto-collect pads / auto-cook while you play
//    Stages    → 3 difficulties, 3 random recipes each
// ============================================================

// --- Recipe (buff formula) ---
struct Recipe {
    std::string name;
    float boostMultiplier;   // boost consumption rate modifier
    float speedBonus;        // flat speed bonus (uu/s)
    float jumpBoostMult;     // jump impulse multiplier
    int   durationSeconds;   // how long the buff lasts
    std::string description;
};

// --- Difficulty tier ---
enum class Difficulty { Easy, Normal, Hard };

// --- Mission state ---
enum class MissionState { Idle, Active, Success, Failed };

// --- Automation upgrade ---
struct AutoUpgrade {
    std::string name;
    std::string desc;
    int   cost;
    bool  purchased;
    // what it does (flags)
    bool  autoCollect;   // auto-steals nearby pads
    bool  autoCook;      // auto-activates formula when threshold met
    float susDecayBonus; // extra sus-rate decay per second
};

class StealAndCook : public BakkesMod::Plugin::BakkesModPlugin,
                     public BakkesMod::Plugin::PluginWindowWrapper<StealAndCook>
{
public:
    // BakkesMod lifecycle
    void onLoad()   override;
    void onUnload() override;

    // ImGui window
    void RenderWindow() override;
    std::string GetMenuName()  override { return "stealandcook"; }
    std::string GetMenuTitle() override { return "Steal & Cook"; }
    void        SetImGuiContext(uintptr_t ctx) override;
    bool        ShouldBlockInput() override { return false; }
    bool        IsActiveOverlay() override  { return true;  }

private:
    // ── Core game state ──────────────────────────────────────
    int          playerMoney       = 500;
    float        susRate           = 0.0f;   // 0-100
    float        susDecayRate      = 2.0f;   // per second while not seen
    float        susIncreaseRate   = 15.0f;  // per "sighting"
    bool         beingSeen         = false;

    int          recipePiecesCollected = 0;
    int          recipePiecesNeeded    = 5;

    MissionState missionState = MissionState::Idle;
    Difficulty   currentDiff  = Difficulty::Easy;
    int          currentStage = 1;
    int          maxStages    = 3;

    // ── Buff / cooking ───────────────────────────────────────
    bool         buffActive   = false;
    float        buffTimer    = 0.0f;
    Recipe       activeRecipe;
    int          researchedRecipes = 0;  // unlocked count

    // ── Available recipes per difficulty ─────────────────────
    std::vector<Recipe> easyRecipes;
    std::vector<Recipe> normalRecipes;
    std::vector<Recipe> hardRecipes;
    Recipe              missionRecipe;   // randomly chosen for current mission

    // ── Automation upgrades ───────────────────────────────────
    std::vector<AutoUpgrade> shopItems;
    bool autoCollectEnabled = false;
    bool autoCookEnabled    = false;
    float autoCollectTimer  = 0.0f;

    // ── Collected pieces for animation ───────────────────────
    std::vector<std::string> researchLog;

    // ── Helpers ───────────────────────────────────────────────
    void InitRecipes();
    void InitShop();
    void StartMission(Difficulty diff);
    void EndMission(bool success);
    void CollectBoostPiece();
    void ActivateBuff(const Recipe& r);
    void DeactivateBuff();
    void ApplyBoostMultiplier(float mult);
    void ApplySpeedBonus(float bonus);
    void ResetSpeedBonus();

    void OnTick(std::string eventName);
    void OnBoostPickup(std::string eventName);
    void OnCarSeen(std::string eventName);   // simulated "sus" trigger

    void RenderHUD();
    void RenderMissionPanel();
    void RenderResearchCenter();
    void RenderShop();
    void RenderBuffStatus();

    Recipe PickRandom(const std::vector<Recipe>& pool);

    // ── Cvar names ────────────────────────────────────────────
    static constexpr const char* CVAR_ENABLED       = "sc_enabled";
    static constexpr const char* CVAR_HUD_VISIBLE   = "sc_hud_visible";
    static constexpr const char* CVAR_SUS_RATE      = "sc_sus_rate";
    static constexpr const char* CVAR_MONEY         = "sc_money";

    std::shared_ptr<bool>  pluginEnabled;
    std::shared_ptr<bool>  hudVisible;
    std::shared_ptr<float> cvSusRate;
    std::shared_ptr<int>   cvMoney;

    float speedBonusApplied = 0.0f;
    std::mt19937 rng;

    // UI state
    int uiTab = 0;  // 0=Mission 1=Research 2=Shop 3=Status
};
