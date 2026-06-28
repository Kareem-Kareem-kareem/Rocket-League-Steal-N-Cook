#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <random>
#include <chrono>

struct NpcCook {
    Vector pos, patrolA, patrolB;
    float  t          = 0.0f;
    bool   goingToB   = true;
    float  speed      = 400.0f;
    float  visionRange= 600.0f;
    bool   seesPlayer = false;
    std::string label;
};

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
    std::string name, desc;
    int   cost         = 0;
    bool  purchased    = false;
    bool  autoCollect  = false;
    bool  autoCook     = false;
    float susDecayBonus= 0.0f;
};

// PluginWindow (NOT PluginWindowWrapper<T>) is the correct base class
class StealAndCook : public BakkesMod::Plugin::BakkesModPlugin,
                     public BakkesMod::Plugin::PluginWindow
{
public:
    void onLoad()   override;
    void onUnload() override;

    // PluginWindow interface — no 'override' keyword; these are pure virtuals in the base
    void        Render();
    std::string GetMenuName();
    std::string GetMenuTitle();
    void        SetImGuiContext(uintptr_t ctx);
    bool        ShouldBlockInput();
    bool        IsActiveOverlay();

private:
    int          playerMoney           = 500;
    float        susRate               = 0.0f;
    float        susDecayRate          = 2.0f;
    float        susIncreasePerSighting= 15.0f;
    bool         holdingPiece          = false;
    int          recipePiecesCollected = 0;
    int          recipePiecesNeeded    = 5;
    MissionState missionState = MissionState::Idle;
    Difficulty   currentDiff  = Difficulty::Easy;
    int          currentStage = 1;

    std::vector<NpcCook> npcs;
    void SpawnNpcs(int count, float visionRange, float speed);
    void TickNpcs(float dt);

    Vector labZoneCenter = { -4096, -5000, 20 };
    float  labZoneRadius = 900.0f;
    bool   InLabZone(Vector carPos);

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

    void InitRecipes();
    void InitShop();
    void StartMission(Difficulty diff);
    void EndMission(bool success);
    void OnBoostPickedUp();
    void TryDeliverPiece(Vector carPos);
    void ActivateBuff(const Recipe& r);
    void DeactivateBuff();
    void ApplyBoostMultiplier(float mult);
    Recipe PickRandom(const std::vector<Recipe>& pool);

    // Tick timing — we track wall time ourselves since GetEngineTick() doesn't exist
    std::chrono::steady_clock::time_point lastTick;
    bool firstTick = true;

    void OnTick(std::string ev);
    void OnBoostPickup(std::string ev);

    void RenderMissionPanel();
    void RenderResearchCenter();
    void RenderShop();
    void RenderBuffStatus();

    static constexpr const char* CV_ENABLED = "sc_enabled";
    static constexpr const char* CV_HUD     = "sc_hud_visible";
    static constexpr const char* CV_SUS     = "sc_sus_rate";
    static constexpr const char* CV_MONEY   = "sc_money";

    std::shared_ptr<bool> pluginEnabled;
    std::shared_ptr<bool> hudVisible;
    float speedBonusApplied = 0.0f;
    std::mt19937 rng;
};
