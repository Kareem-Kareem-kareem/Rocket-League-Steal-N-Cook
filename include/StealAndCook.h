#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "imgui/imgui.h"
#include <string>
#include <vector>
#include <random>

// ============================================================
//  STEAL & COOK  –  BakkesMod Plugin  (Solo / Freeplay only)
//
//  You are alone in Freeplay. Fake NPC "competitor cooks"
//  patrol the field. Collect boost pads to steal recipe pieces
//  and smuggle them to your Research Center zone (blue half).
//  If an NPC cook "sees" you (you enter their detection radius
//  while holding an undelivered piece), sus goes up.
//  Hit 100% sus = mission failed.
// ============================================================

// ── NPC cook (fake competitor) ────────────────────────────────
struct NpcCook {
    Vector      pos;            // current world position
    Vector      patrolA;        // patrol point A
    Vector      patrolB;        // patrol point B
    float       t           = 0.0f;   // lerp 0→1 along patrol
    bool        goingToB    = true;
    float       speed       = 400.0f; // uu/s patrol speed
    float       visionRange = 600.0f; // detection radius (uu)
    bool        seesPlayer  = false;
    std::string label;          // e.g. "Cook #1"
};

// ── Recipe (buff formula) ─────────────────────────────────────
struct Recipe {
    std::string name;
    float boostMultiplier;
    float speedBonus;
    float jumpBoostMult;
    int   durationSeconds;
    std::string description;
};

enum class Difficulty   { Easy, Normal, Hard };
enum class MissionState { Idle, Active, Success, Failed };

struct AutoUpgrade {
    std::string name;
    std::string desc;
    int   cost;
    bool  purchased    = false;
    bool  autoCollect  = false;
    bool  autoCook     = false;
    float susDecayBonus= 0.0f;
};

// ─────────────────────────────────────────────────────────────
class StealAndCook : public BakkesMod::Plugin::BakkesModPlugin,
                     public BakkesMod::Plugin::PluginWindowWrapper<StealAndCook>
{
public:
    void onLoad()   override;
    void onUnload() override;

    void        RenderWindow() override;
    std::string GetMenuName()  override { return "stealandcook"; }
    std::string GetMenuTitle() override { return "Steal & Cook"; }
    void        SetImGuiContext(uintptr_t ctx) override;
    bool        ShouldBlockInput() override { return false; }
    bool        IsActiveOverlay() override  { return true;  }

private:
    // ── State ─────────────────────────────────────────────────
    int          playerMoney           = 500;
    float        susRate               = 0.0f;
    float        susDecayRate          = 2.0f;
    float        susIncreasePerSighting= 15.0f;
    bool         holdingPiece          = false;  // smuggling a piece
    int          recipePiecesCollected = 0;
    int          recipePiecesNeeded    = 5;
    MissionState missionState = MissionState::Idle;
    Difficulty   currentDiff  = Difficulty::Easy;
    int          currentStage = 1;

    // ── NPCs ──────────────────────────────────────────────────
    std::vector<NpcCook> npcs;
    void SpawnNpcs(int count, float visionRange, float speed);
    void TickNpcs(float dt);
    void DrawNpcOverlay();           // ImGui overlay circles

    // ── Lab zone (blue-half goal area) ────────────────────────
    // Player must reach this zone while holdingPiece = true
    Vector labZoneCenter   = { -4096, -5000, 20 }; // approx blue goal mouth
    float  labZoneRadius   = 900.0f;
    bool   InLabZone(Vector carPos);

    // ── Buff ─────────────────────────────────────────────────
    bool   buffActive = false;
    float  buffTimer  = 0.0f;
    Recipe activeRecipe;

    std::vector<Recipe> easyRecipes, normalRecipes, hardRecipes;
    Recipe              missionRecipe;

    std::vector<AutoUpgrade> shopItems;
    bool  autoCollectEnabled = false;
    bool  autoCookEnabled    = false;
    float autoCollectTimer   = 0.0f;

    std::vector<std::string> researchLog;
    int researchedRecipes = 0;

    // ── Helpers ───────────────────────────────────────────────
    void InitRecipes();
    void InitShop();
    void StartMission(Difficulty diff);
    void EndMission(bool success);
    void OnBoostPickedUp();
    void TryDeliverPiece(Vector carPos);
    void ActivateBuff(const Recipe& r);
    void DeactivateBuff();
    void ApplyBoostMultiplier(float mult);
    void ApplySpeedBonus(float bonus);
    void ResetSpeedBonus();
    Recipe PickRandom(const std::vector<Recipe>& pool);

    void OnTick(std::string ev);
    void OnBoostPickup(std::string ev);

    // ── UI ────────────────────────────────────────────────────
    void RenderMissionPanel();
    void RenderResearchCenter();
    void RenderShop();
    void RenderBuffStatus();

    // ── Cvars ─────────────────────────────────────────────────
    static constexpr const char* CV_ENABLED = "sc_enabled";
    static constexpr const char* CV_HUD     = "sc_hud_visible";
    static constexpr const char* CV_SUS     = "sc_sus_rate";
    static constexpr const char* CV_MONEY   = "sc_money";

    std::shared_ptr<bool>  pluginEnabled;
    std::shared_ptr<bool>  hudVisible;
    float  speedBonusApplied = 0.0f;
    std::mt19937 rng;
};
