#include "pch.h"
#include "StealAndCook.h"

// ── BakkesMod boilerplate ────────────────────────────────────
BAKKESMOD_PLUGIN(StealAndCook, "Steal & Cook", "1.0.0", PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING | PLUGINTYPE_ONLINE)

// ============================================================
//  onLoad  –  Register cvars, hooks, shop, recipes
// ============================================================
void StealAndCook::onLoad()
{
    rng.seed(std::random_device{}());

    // ── Register cvars ────────────────────────────────────────
    cvarManager->registerCvar(CVAR_ENABLED,     "1",    "Enable Steal & Cook plugin", true, true, 0, true, 1);
    cvarManager->registerCvar(CVAR_HUD_VISIBLE, "1",    "Show Steal & Cook HUD",      true, true, 0, true, 1);
    cvarManager->registerCvar(CVAR_SUS_RATE,    "0",    "Current sus rate (0-100)",   true, true, 0, true, 100);
    cvarManager->registerCvar(CVAR_MONEY,       "500",  "Player money",               true, true, 0, true, 999999);

    pluginEnabled = std::make_shared<bool>(true);
    hudVisible    = std::make_shared<bool>(true);
    cvSusRate     = std::make_shared<float>(0.0f);
    cvMoney       = std::make_shared<int>(500);

    cvarManager->getCvar(CVAR_ENABLED).bindTo(pluginEnabled);
    cvarManager->getCvar(CVAR_HUD_VISIBLE).bindTo(hudVisible);

    // ── Init game data ────────────────────────────────────────
    InitRecipes();
    InitShop();

    // ── Hook game events ──────────────────────────────────────
    // Tick – drives sus decay, buff timer, auto-collect
    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.Tick",
        [this](CarWrapper cw, void* params, std::string eventName) {
            OnTick(eventName);
        }
    );

    // Boost pad pickup – "stealing" opponent boost
    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.EventBoostActivated",
        [this](CarWrapper cw, void* params, std::string eventName) {
            OnBoostPickup(eventName);
        }
    );

    // Demolition = "caught" – spikes sus rate hard
    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.Demolish",
        [this](CarWrapper cw, void* params, std::string eventName) {
            OnCarSeen(eventName);
        }
    );

    // Reset buff when a new match starts
    gameWrapper->HookEvent(
        "Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string eventName) {
            DeactivateBuff();
            susRate = 0.0f;
        }
    );

    // ── Console commands ──────────────────────────────────────
    cvarManager->registerNotifier("sc_start_easy",   [this](std::vector<std::string> args){ StartMission(Difficulty::Easy);   }, "Start Easy mission",   PERMISSION_ALL);
    cvarManager->registerNotifier("sc_start_normal", [this](std::vector<std::string> args){ StartMission(Difficulty::Normal); }, "Start Normal mission", PERMISSION_ALL);
    cvarManager->registerNotifier("sc_start_hard",   [this](std::vector<std::string> args){ StartMission(Difficulty::Hard);   }, "Start Hard mission",   PERMISSION_ALL);
    cvarManager->registerNotifier("sc_collect",      [this](std::vector<std::string> args){ CollectBoostPiece(); },              "Collect recipe piece", PERMISSION_ALL);
    cvarManager->registerNotifier("sc_cook",         [this](std::vector<std::string> args){
        if (missionState == MissionState::Success && !buffActive)
            ActivateBuff(missionRecipe);
    }, "Activate researched buff", PERMISSION_ALL);

    LOG("Steal & Cook plugin loaded!");
}

void StealAndCook::onUnload()
{
    DeactivateBuff();
    LOG("Steal & Cook plugin unloaded.");
}

// ============================================================
//  InitRecipes  –  3 per difficulty, all different
// ============================================================
void StealAndCook::InitRecipes()
{
    // EASY  – mild buffs, short duration
    easyRecipes = {
        { "Ketchup Rush",   1.0f,  50.0f, 1.05f, 20, "Mild speed kick. Tastes tangy." },
        { "Cheese Glide",   0.85f, 30.0f, 1.10f, 25, "Smoother steering, melts on turn." },
        { "Lettuce Drift",  0.90f, 40.0f, 1.08f, 18, "Light handling boost. Crispy." }
    };

    // NORMAL – meaningful buffs, moderate duration
    normalRecipes = {
        { "Patty Overdrive", 1.0f,  120.0f, 1.15f, 30, "Beef-fuelled speed surge." },
        { "Secret Sauce Mk1",0.75f, 80.0f,  1.20f, 35, "Boost efficiency way up." },
        { "Onion Ring Boost",1.10f, 100.0f, 1.18f, 28, "Rings of acceleration." }
    };

    // HARD – powerful buffs, long duration, hard to steal
    hardRecipes = {
        { "Ghost Pepper Nitro", 1.0f,  250.0f, 1.30f, 45, "Scorching speed. Illegal in 12 countries." },
        { "Triple Bypass",      0.60f, 180.0f, 1.40f, 50, "Boost barely drains. Suspect." },
        { "Black Market Burger", 0.70f, 220.0f, 1.35f, 55, "Maximum performance. Stolen from the best." }
    };
}

// ============================================================
//  InitShop  –  Automation upgrades
// ============================================================
void StealAndCook::InitShop()
{
    shopItems = {
        { "Sneaker Soles",      "Reduces sus rate increase by 30%",        200, false, false, false, 0.5f },
        { "Auto-Collector Mk1", "Automatically grabs nearby boost pads",   350, false, true,  false, 0.0f },
        { "Auto-Cook Oven",     "Auto-activates formula when fully researched", 500, false, false, true,  0.0f },
        { "Decoy Burger",       "Deploy a fake burger to reset 50% sus",   150, false, false, false, 1.5f },
        { "Research Bot",       "Collects recipe pieces twice as fast",    600, false, false, false, 0.0f },
        { "Invisibility Cloak", "Halves sus increase rate entirely",       800, false, false, false, 2.5f },
    };
}

// ============================================================
//  StartMission
// ============================================================
void StealAndCook::StartMission(Difficulty diff)
{
    currentDiff            = diff;
    missionState           = MissionState::Active;
    recipePiecesCollected  = 0;
    susRate                = 0.0f;
    researchLog.clear();

    // Difficulty tuning
    switch (diff) {
        case Difficulty::Easy:
            recipePiecesNeeded = 4;
            susIncreaseRate    = 10.0f;
            susDecayRate       = 4.0f;
            missionRecipe      = PickRandom(easyRecipes);
            break;
        case Difficulty::Normal:
            recipePiecesNeeded = 6;
            susIncreaseRate    = 18.0f;
            susDecayRate       = 2.5f;
            missionRecipe      = PickRandom(normalRecipes);
            break;
        case Difficulty::Hard:
            recipePiecesNeeded = 8;
            susIncreaseRate    = 28.0f;
            susDecayRate       = 1.5f;
            missionRecipe      = PickRandom(hardRecipes);
            break;
    }

    LOG("Mission started: " + missionRecipe.name);
    gameWrapper->Toast("Steal & Cook", "Mission started! Steal: " + missionRecipe.name, "default", 3.0f);
}

// ============================================================
//  EndMission
// ============================================================
void StealAndCook::EndMission(bool success)
{
    if (success) {
        missionState = MissionState::Success;
        researchedRecipes++;
        int reward = 100 * currentStage * (int)(currentDiff) + 150;
        playerMoney += reward;
        cvarManager->getCvar(CVAR_MONEY).setValue(playerMoney);
        researchLog.push_back("✓ Recipe unlocked: " + missionRecipe.name);
        gameWrapper->Toast("Steal & Cook", "Recipe stolen! +" + std::to_string(reward) + " coins", "default", 4.0f);

        if (autoCookEnabled)
            ActivateBuff(missionRecipe);
    } else {
        missionState = MissionState::Failed;
        researchLog.push_back("✗ Mission blown. Sus rate 100%.");
        gameWrapper->Toast("Steal & Cook", "BUSTED! Sus rate hit 100%!", "default", 4.0f);
    }
}

// ============================================================
//  CollectBoostPiece  –  called on boost pad pickup during mission
// ============================================================
void StealAndCook::CollectBoostPiece()
{
    if (missionState != MissionState::Active) return;

    recipePiecesCollected++;
    researchLog.push_back("Piece " + std::to_string(recipePiecesCollected) +
                          "/" + std::to_string(recipePiecesNeeded) + " collected.");

    if (recipePiecesCollected >= recipePiecesNeeded)
        EndMission(true);
}

// ============================================================
//  ActivateBuff / DeactivateBuff
// ============================================================
void StealAndCook::ActivateBuff(const Recipe& r)
{
    activeRecipe = r;
    buffActive   = true;
    buffTimer    = (float)r.durationSeconds;

    ApplyBoostMultiplier(r.boostMultiplier);
    ApplySpeedBonus(r.speedBonus);

    gameWrapper->Toast("Steal & Cook", "COOKING: " + r.name + "!", "default", 3.0f);
    LOG("Buff activated: " + r.name);
}

void StealAndCook::DeactivateBuff()
{
    if (!buffActive) return;
    buffActive = false;
    buffTimer  = 0.0f;
    ApplyBoostMultiplier(1.0f);
    ResetSpeedBonus();
    LOG("Buff expired.");
}

// ============================================================
//  Apply helpers using BakkesMod cvars / car manipulation
// ============================================================
void StealAndCook::ApplyBoostMultiplier(float mult)
{
    // TAGame.BoostComponent_TA controls boost consumption.
    // We piggyback on the "sv_soccar_booststrength" approach
    // (works in Freeplay / custom training).
    if (!gameWrapper->IsInFreeplay() && !gameWrapper->IsInCustomTraining()) return;

    auto server = gameWrapper->GetCurrentGameState();
    if (!server) return;

    // Clamp to sane values
    mult = std::clamp(mult, 0.1f, 5.0f);
    cvarManager->getCvar("sv_soccar_booststrength").setValue(mult * 1.0f);
}

void StealAndCook::ApplySpeedBonus(float bonus)
{
    speedBonusApplied = bonus;
    // We'll apply this per-tick via car velocity scaling
}

void StealAndCook::ResetSpeedBonus()
{
    speedBonusApplied = 0.0f;
}

// ============================================================
//  OnTick  –  runs every frame during a game
// ============================================================
void StealAndCook::OnTick(std::string /*eventName*/)
{
    if (!*pluginEnabled) return;

    gameWrapper->Execute([this](GameWrapper* gw) {
        // ── Sus decay ────────────────────────────────────────
        if (!beingSeen && susRate > 0.0f) {
            float decay = susDecayRate;
            // apply upgrade bonuses
            for (auto& u : shopItems)
                if (u.purchased) decay += u.susDecayBonus;
            susRate = std::max(0.0f, susRate - decay * gw->GetEngineTick());
            cvarManager->getCvar(CVAR_SUS_RATE).setValue(susRate);
        }
        beingSeen = false;  // reset each tick; OnCarSeen sets it true

        // ── Buff timer ───────────────────────────────────────
        if (buffActive) {
            buffTimer -= gw->GetEngineTick();
            if (buffTimer <= 0.0f)
                DeactivateBuff();
        }

        // ── Speed bonus via car velocity ─────────────────────
        if (speedBonusApplied > 0.0f && buffActive) {
            auto car = gw->GetLocalCar();
            if (!car.IsNull()) {
                auto vel = car.GetVelocity();
                float speed = vel.magnitude();
                if (speed > 10.0f) {
                    float targetSpeed = speed + speedBonusApplied * gw->GetEngineTick();
                    float scale = targetSpeed / speed;
                    vel.X *= scale; vel.Y *= scale; vel.Z *= scale;
                    car.SetVelocity(vel);
                }
            }
        }

        // ── Auto-collect ─────────────────────────────────────
        if (autoCollectEnabled && missionState == MissionState::Active) {
            autoCollectTimer -= gw->GetEngineTick();
            if (autoCollectTimer <= 0.0f) {
                autoCollectTimer = 3.0f;  // every 3 seconds
                CollectBoostPiece();
            }
        }
    });
}

// ============================================================
//  OnBoostPickup  –  hook for boost pad collection
// ============================================================
void StealAndCook::OnBoostPickup(std::string /*eventName*/)
{
    if (!*pluginEnabled || missionState != MissionState::Active) return;
    CollectBoostPiece();
}

// ============================================================
//  OnCarSeen  –  sus rate spike (triggered by demo or hook)
// ============================================================
void StealAndCook::OnCarSeen(std::string /*eventName*/)
{
    if (!*pluginEnabled || missionState != MissionState::Active) return;

    beingSeen = true;
    float increase = susIncreaseRate;

    // Check for sus-reduction upgrades
    for (auto& u : shopItems) {
        if (u.purchased && u.name == "Sneaker Soles")     increase *= 0.70f;
        if (u.purchased && u.name == "Invisibility Cloak") increase *= 0.50f;
    }

    susRate = std::min(100.0f, susRate + increase);
    cvarManager->getCvar(CVAR_SUS_RATE).setValue(susRate);

    if (susRate >= 100.0f)
        EndMission(false);
}

// ============================================================
//  Helper – pick random recipe
// ============================================================
Recipe StealAndCook::PickRandom(const std::vector<Recipe>& pool)
{
    std::uniform_int_distribution<int> dist(0, (int)pool.size() - 1);
    return pool[dist(rng)];
}

// ============================================================
//  ImGui UI
// ============================================================
void StealAndCook::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void StealAndCook::RenderWindow()
{
    if (!*pluginEnabled) return;

    ImGui::SetNextWindowSize(ImVec2(520, 580), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("🍔 Steal & Cook", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // ── Top status bar ────────────────────────────────────────
    ImGui::TextColored(ImVec4(1,0.8f,0,1), "Stage %d / %d", currentStage, maxStages);
    ImGui::SameLine(200);
    ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "$%d", playerMoney);
    ImGui::SameLine(320);

    // Sus meter
    float susNorm = susRate / 100.0f;
    ImVec4 susCol = susNorm < 0.4f ? ImVec4(0,1,0,1) :
                    susNorm < 0.75f ? ImVec4(1,0.7f,0,1) : ImVec4(1,0.1f,0.1f,1);
    ImGui::TextColored(susCol, "SUS: %.0f%%", susRate);
    ImGui::ProgressBar(susNorm, ImVec2(-1, 6), "");

    ImGui::Separator();

    // ── Tabs ─────────────────────────────────────────────────
    const char* tabs[] = { "Mission", "Research Center", "Shop", "Active Buff" };
    ImGui::BeginTabBar("##tabs");

    if (ImGui::BeginTabItem("Mission")) { RenderMissionPanel();    ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Research Center")) { RenderResearchCenter(); ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Shop"))     { RenderShop();           ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Buff Status")) { RenderBuffStatus();  ImGui::EndTabItem(); }

    ImGui::EndTabBar();
    ImGui::End();
}

// ── Tab: Mission ─────────────────────────────────────────────
void StealAndCook::RenderMissionPanel()
{
    ImGui::TextWrapped("Your restaurant is losing to competitors. Sneak into their kitchen, steal their recipe, and bring it back to YOUR Research Center!");
    ImGui::Spacing();

    // Status chip
    const char* stateStr[] = { "IDLE", "ACTIVE", "SUCCESS", "FAILED" };
    ImVec4      stateCol[] = {
        {0.7f,0.7f,0.7f,1}, {1,0.8f,0,1}, {0.2f,1,0.4f,1}, {1,0.2f,0.2f,1}
    };
    ImGui::TextColored(stateCol[(int)missionState], "Status: %s", stateStr[(int)missionState]);

    if (missionState == MissionState::Active) {
        ImGui::Text("Target recipe: %s", missionRecipe.name.c_str());
        ImGui::Text("Pieces collected: %d / %d", recipePiecesCollected, recipePiecesNeeded);
        ImGui::ProgressBar((float)recipePiecesCollected / recipePiecesNeeded, ImVec2(-1,12), "");
        ImGui::TextColored(ImVec4(0.8f,0.8f,0.8f,1), "Tip: Collect boost pads to steal recipe pieces. Avoid being seen!");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Select Difficulty:");
    ImGui::Spacing();

    auto DiffButton = [&](const char* label, Difficulty d, ImVec4 col) {
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        if (ImGui::Button(label, ImVec2(150, 36))) StartMission(d);
        ImGui::PopStyleColor();
    };

    DiffButton("Easy",   Difficulty::Easy,   {0.15f,0.55f,0.15f,1});
    ImGui::SameLine();
    DiffButton("Normal", Difficulty::Normal, {0.55f,0.45f,0.0f,1});
    ImGui::SameLine();
    DiffButton("Hard",   Difficulty::Hard,   {0.55f,0.1f,0.1f,1});

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f,0.8f,1,1),
        "Easy:   4 pieces | Slow sus\n"
        "Normal: 6 pieces | Medium sus\n"
        "Hard:   8 pieces | Fast sus");
}

// ── Tab: Research Center ──────────────────────────────────────
void StealAndCook::RenderResearchCenter()
{
    ImGui::Text("Hidden lab below your restaurant.");
    ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "Recipes researched: %d", researchedRecipes);
    ImGui::Spacing();

    if (missionState == MissionState::Success) {
        ImGui::TextColored(ImVec4(1,0.9f,0.3f,1), "Recipe ready to cook: %s", missionRecipe.name.c_str());
        ImGui::Text("  Speed bonus:   +%.0f uu/s", missionRecipe.speedBonus);
        ImGui::Text("  Boost mult:    %.2fx", missionRecipe.boostMultiplier);
        ImGui::Text("  Jump power:    %.2fx", missionRecipe.jumpBoostMult);
        ImGui::Text("  Duration:      %d seconds", missionRecipe.durationSeconds);
        ImGui::TextWrapped("  %s", missionRecipe.description.c_str());
        ImGui::Spacing();
        if (!buffActive) {
            if (ImGui::Button("🍳  Cook It!", ImVec2(200, 40)))
                ActivateBuff(missionRecipe);
        } else {
            ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Buff already active!");
        }
    } else if (missionState == MissionState::Active) {
        ImGui::TextColored(ImVec4(1,0.6f,0,1), "Mission in progress — steal all %d pieces first!", recipePiecesNeeded);
    } else {
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "No recipe in progress. Start a mission first.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Research Log:");
    ImGui::BeginChild("##log", ImVec2(-1, 160), true);
    for (auto it = researchLog.rbegin(); it != researchLog.rend(); ++it)
        ImGui::TextUnformatted(it->c_str());
    ImGui::EndChild();
}

// ── Tab: Shop ────────────────────────────────────────────────
void StealAndCook::RenderShop()
{
    ImGui::Text("Spend your stolen gains on automation and upgrades.");
    ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "Balance: $%d", playerMoney);
    ImGui::Spacing();

    for (auto& item : shopItems) {
        if (item.purchased) {
            ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "[✓] %s", item.name.c_str());
            ImGui::SameLine(300);
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Owned");
        } else {
            ImGui::Text("[ ] %s", item.name.c_str());
            ImGui::SameLine(300);
            bool canAfford = playerMoney >= item.cost;
            if (!canAfford) ImGui::BeginDisabled();
            std::string btnLabel = "$" + std::to_string(item.cost) + "##buy_" + item.name;
            if (ImGui::Button(btnLabel.c_str())) {
                playerMoney -= item.cost;
                item.purchased = true;
                if (item.autoCollect) autoCollectEnabled = true;
                if (item.autoCook)    autoCookEnabled    = true;
                cvarManager->getCvar(CVAR_MONEY).setValue(playerMoney);
                researchLog.push_back("Purchased: " + item.name);
            }
            if (!canAfford) ImGui::EndDisabled();
        }
        ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "    %s", item.desc.c_str());
        ImGui::Spacing();
    }
}

// ── Tab: Buff Status ─────────────────────────────────────────
void StealAndCook::RenderBuffStatus()
{
    if (!buffActive) {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "No buff active.");
        ImGui::TextWrapped("Steal a recipe and cook it from the Research Center tab to activate a buff.");
        return;
    }

    ImGui::TextColored(ImVec4(1,0.9f,0.2f,1), "🔥 COOKING: %s", activeRecipe.name.c_str());
    ImGui::Spacing();

    float progress = buffTimer / (float)activeRecipe.durationSeconds;
    ImGui::Text("Time remaining: %.1f sec", buffTimer);
    ImGui::ProgressBar(progress, ImVec2(-1, 16), "");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Active buffs:");
    ImGui::BulletText("Speed bonus:     +%.0f uu/s", activeRecipe.speedBonus);
    ImGui::BulletText("Boost strength:  %.2fx", activeRecipe.boostMultiplier);
    ImGui::BulletText("Jump power:      %.2fx", activeRecipe.jumpBoostMult);

    ImGui::Spacing();
    if (ImGui::Button("Cancel Buff", ImVec2(140,32)))
        DeactivateBuff();
}
