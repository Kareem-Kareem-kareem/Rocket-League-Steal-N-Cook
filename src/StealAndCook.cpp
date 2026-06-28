#include "pch.h"
#include "StealAndCook.h"

BAKKESMOD_PLUGIN(StealAndCook, "Steal & Cook", "1.1.0", PLUGINTYPE_FREEPLAY)

// ============================================================
//  onLoad
// ============================================================
void StealAndCook::onLoad()
{
    rng.seed(std::random_device{}());

    cvarManager->registerCvar(CV_ENABLED, "1",   "Enable Steal & Cook", true, true, 0, true, 1);
    cvarManager->registerCvar(CV_HUD,     "1",   "Show HUD",            true, true, 0, true, 1);
    cvarManager->registerCvar(CV_SUS,     "0",   "Sus rate",            true, true, 0, true, 100);
    cvarManager->registerCvar(CV_MONEY,   "500", "Player money",        true, true, 0, true, 999999);

    pluginEnabled = std::make_shared<bool>(true);
    hudVisible    = std::make_shared<bool>(true);
    cvarManager->getCvar(CV_ENABLED).bindTo(pluginEnabled);
    cvarManager->getCvar(CV_HUD).bindTo(hudVisible);

    InitRecipes();
    InitShop();

    // ── Tick: NPC movement, sus decay, buff timer, delivery check ──
    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.Tick",
        [this](CarWrapper cw, void*, std::string ev) { OnTick(ev); }
    );

    // ── Boost pad pickup = "grabbed a piece from competitor kitchen" ──
    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.EventBoostActivated",
        [this](CarWrapper cw, void*, std::string ev) { OnBoostPickup(ev); }
    );

    // ── Reset on new freeplay session ────────────────────────
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitGame",
        [this](std::string) {
            DeactivateBuff();
            susRate     = 0.0f;
            holdingPiece= false;
            npcs.clear();
        }
    );

    // ── Console commands ──────────────────────────────────────
    cvarManager->registerNotifier("sc_start_easy",   [this](auto){ StartMission(Difficulty::Easy);   }, "", PERMISSION_ALL);
    cvarManager->registerNotifier("sc_start_normal", [this](auto){ StartMission(Difficulty::Normal); }, "", PERMISSION_ALL);
    cvarManager->registerNotifier("sc_start_hard",   [this](auto){ StartMission(Difficulty::Hard);   }, "", PERMISSION_ALL);
    cvarManager->registerNotifier("sc_cook",         [this](auto){
        if (missionState == MissionState::Success && !buffActive)
            ActivateBuff(missionRecipe);
    }, "", PERMISSION_ALL);

    LOG("Steal & Cook loaded (solo / freeplay mode).");
}

void StealAndCook::onUnload() { DeactivateBuff(); }

// ============================================================
//  NPC cooks
// ============================================================
void StealAndCook::SpawnNpcs(int count, float visionRange, float speed)
{
    npcs.clear();

    // Patrol routes spread across orange half (opponent kitchen)
    // Rocket League field: X ±4096, Y ±5120, orange half Y > 0
    struct Route { Vector a; Vector b; };
    std::vector<Route> routes = {
        { {-2000, 1500, 20}, { 2000, 1500, 20} },  // mid-field horizontal
        { {-3000, 3000, 20}, {-3000, 4500, 20} },  // left lane vertical
        { { 3000, 3000, 20}, { 3000, 4500, 20} },  // right lane vertical
        { { -500, 2000, 20}, { -500, 4800, 20} },  // center lane
        { { 1500, 1800, 20}, { 1500, 4000, 20} },  // right-center
    };

    for (int i = 0; i < count && i < (int)routes.size(); i++) {
        NpcCook c;
        c.label      = "Cook #" + std::to_string(i + 1);
        c.patrolA    = routes[i].a;
        c.patrolB    = routes[i].b;
        c.pos        = routes[i].a;
        c.speed      = speed;
        c.visionRange= visionRange;
        c.t          = 0.0f;
        c.goingToB   = true;
        npcs.push_back(c);
    }
}

void StealAndCook::TickNpcs(float dt)
{
    if (!gameWrapper->IsInFreeplay()) return;

    auto car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;
    Vector carPos = car.GetLocation();

    bool anySeesPlayer = false;

    for (auto& npc : npcs) {
        // ── Move along patrol ─────────────────────────────────
        Vector& from = npc.goingToB ? npc.patrolA : npc.patrolB;
        Vector& to   = npc.goingToB ? npc.patrolB : npc.patrolA;
        Vector  dir  = to - from;
        float   len  = dir.magnitude();
        if (len < 1.0f) { npc.goingToB = !npc.goingToB; continue; }

        float step = npc.speed * dt / len;
        npc.t += step;
        if (npc.t >= 1.0f) {
            npc.t = 0.0f;
            npc.goingToB = !npc.goingToB;
        }
        npc.pos = from + dir * npc.t;

        // ── Vision check ─────────────────────────────────────
        // Cook only raises sus if player is holding a piece
        // (= you're sneaking a burger to the research center)
        float dist = (carPos - npc.pos).magnitude();
        npc.seesPlayer = (dist < npc.visionRange) && holdingPiece;
        if (npc.seesPlayer) anySeesPlayer = true;
    }

    // ── Sus rate update ───────────────────────────────────────
    if (anySeesPlayer) {
        float increase = susIncreasePerSighting;
        for (auto& u : shopItems) {
            if (!u.purchased) continue;
            if (u.name == "Sneaker Soles")      increase *= 0.70f;
            if (u.name == "Invisibility Cloak") increase *= 0.50f;
        }
        susRate = std::min(100.0f, susRate + increase * dt);
        cvarManager->getCvar(CV_SUS).setValue(susRate);

        if (susRate >= 100.0f)
            EndMission(false);
    } else if (susRate > 0.0f) {
        float decay = susDecayRate;
        for (auto& u : shopItems)
            if (u.purchased) decay += u.susDecayBonus;
        susRate = std::max(0.0f, susRate - decay * dt);
        cvarManager->getCvar(CV_SUS).setValue(susRate);
    }
}

bool StealAndCook::InLabZone(Vector carPos)
{
    // Blue-half research center zone (behind blue goal line)
    return (carPos - labZoneCenter).magnitude() < labZoneRadius;
}

// ============================================================
//  OnTick
// ============================================================
void StealAndCook::OnTick(std::string)
{
    if (!*pluginEnabled || missionState != MissionState::Active) return;

    gameWrapper->Execute([this](GameWrapper* gw) {
        float dt = gw->GetEngineTick();

        TickNpcs(dt);

        // ── Delivery check ────────────────────────────────────
        if (holdingPiece) {
            auto car = gw->GetLocalCar();
            if (!car.IsNull() && InLabZone(car.GetLocation())) {
                holdingPiece = false;
                recipePiecesCollected++;
                researchLog.push_back("Piece " + std::to_string(recipePiecesCollected) +
                                      "/" + std::to_string(recipePiecesNeeded) + " delivered to lab!");
                if (recipePiecesCollected >= recipePiecesNeeded)
                    EndMission(true);
            }
        }

        // ── Auto-collect ──────────────────────────────────────
        if (autoCollectEnabled && !holdingPiece) {
            autoCollectTimer -= dt;
            if (autoCollectTimer <= 0.0f) {
                autoCollectTimer = 3.5f;
                OnBoostPickedUp();
            }
        }

        // ── Buff timer ────────────────────────────────────────
        if (buffActive) {
            buffTimer -= dt;
            if (buffTimer <= 0.0f) DeactivateBuff();
        }

        // ── Speed bonus ───────────────────────────────────────
        if (buffActive && speedBonusApplied > 0.0f) {
            auto car = gw->GetLocalCar();
            if (!car.IsNull()) {
                auto vel   = car.GetVelocity();
                float spd  = vel.magnitude();
                if (spd > 10.0f) {
                    float newSpd = spd + speedBonusApplied * dt;
                    float scale  = newSpd / spd;
                    vel.X *= scale; vel.Y *= scale; vel.Z *= scale;
                    car.SetVelocity(vel);
                }
            }
        }
    });
}

// ============================================================
//  Boost pad pickup = player grabbed a recipe piece
//  They now have to carry it to the Lab zone
// ============================================================
void StealAndCook::OnBoostPickup(std::string) { OnBoostPickedUp(); }

void StealAndCook::OnBoostPickedUp()
{
    if (missionState != MissionState::Active || holdingPiece) return;
    holdingPiece = true;
    researchLog.push_back("Piece grabbed — get to the Research Center!");
    gameWrapper->Toast("Steal & Cook", "Piece grabbed! Run to your lab!", "default", 2.5f);
}

// ============================================================
//  Mission control
// ============================================================
void StealAndCook::StartMission(Difficulty diff)
{
    if (!gameWrapper->IsInFreeplay()) {
        gameWrapper->Toast("Steal & Cook", "Enter Freeplay first!", "default", 3.0f);
        return;
    }

    currentDiff           = diff;
    missionState          = MissionState::Active;
    recipePiecesCollected = 0;
    susRate               = 0.0f;
    holdingPiece          = false;
    researchLog.clear();

    int   npcCount;
    float vision, speed;

    switch (diff) {
        case Difficulty::Easy:
            recipePiecesNeeded  = 4;
            susIncreasePerSighting = 8.0f;
            susDecayRate        = 5.0f;
            npcCount = 2; vision = 500.0f; speed = 350.0f;
            missionRecipe = PickRandom(easyRecipes);
            break;
        case Difficulty::Normal:
            recipePiecesNeeded  = 6;
            susIncreasePerSighting = 16.0f;
            susDecayRate        = 3.0f;
            npcCount = 3; vision = 650.0f; speed = 500.0f;
            missionRecipe = PickRandom(normalRecipes);
            break;
        case Difficulty::Hard:
            recipePiecesNeeded  = 8;
            susIncreasePerSighting = 26.0f;
            susDecayRate        = 1.5f;
            npcCount = 5; vision = 800.0f; speed = 700.0f;
            missionRecipe = PickRandom(hardRecipes);
            break;
    }

    SpawnNpcs(npcCount, vision, speed);
    LOG("Mission started: " + missionRecipe.name);
    gameWrapper->Toast("Steal & Cook",
        "Mission started! Target: " + missionRecipe.name +
        " | " + std::to_string(npcCount) + " cooks patrolling", "default", 4.0f);
}

void StealAndCook::EndMission(bool success)
{
    holdingPiece = false;

    if (success) {
        missionState = MissionState::Success;
        researchedRecipes++;
        int reward = 150 + 100 * currentStage * ((int)currentDiff + 1);
        playerMoney += reward;
        cvarManager->getCvar(CV_MONEY).setValue(playerMoney);
        researchLog.push_back("✓ Recipe stolen: " + missionRecipe.name);
        gameWrapper->Toast("Steal & Cook", "Recipe stolen! +" + std::to_string(reward) + " coins", "default", 5.0f);
        if (autoCookEnabled) ActivateBuff(missionRecipe);
    } else {
        missionState = MissionState::Failed;
        npcs.clear();
        researchLog.push_back("✗ BUSTED. A cook spotted you. Sus 100%.");
        gameWrapper->Toast("Steal & Cook", "BUSTED! A cook saw you!", "default", 5.0f);
    }
}

// ============================================================
//  Buff
// ============================================================
void StealAndCook::ActivateBuff(const Recipe& r)
{
    activeRecipe       = r;
    buffActive         = true;
    buffTimer          = (float)r.durationSeconds;
    speedBonusApplied  = r.speedBonus;
    ApplyBoostMultiplier(r.boostMultiplier);
    gameWrapper->Toast("Steal & Cook", "Cooking: " + r.name + "!", "default", 3.0f);
}

void StealAndCook::DeactivateBuff()
{
    if (!buffActive) return;
    buffActive = false; buffTimer = 0.0f;
    speedBonusApplied = 0.0f;
    ApplyBoostMultiplier(1.0f);
}

void StealAndCook::ApplyBoostMultiplier(float mult)
{
    if (!gameWrapper->IsInFreeplay()) return;
    cvarManager->getCvar("sv_soccar_booststrength").setValue(
        std::clamp(mult, 0.1f, 5.0f));
}

void StealAndCook::ApplySpeedBonus(float bonus) { speedBonusApplied = bonus; }
void StealAndCook::ResetSpeedBonus()            { speedBonusApplied = 0.0f; }

// ============================================================
//  Recipes & Shop
// ============================================================
void StealAndCook::InitRecipes()
{
    easyRecipes = {
        { "Ketchup Rush",    1.0f,  50.0f, 1.05f, 20, "Mild speed kick. Tastes tangy."       },
        { "Cheese Glide",    0.85f, 30.0f, 1.10f, 25, "Smoother steering, melts on turn."    },
        { "Lettuce Drift",   0.90f, 40.0f, 1.08f, 18, "Light handling boost. Crispy."        }
    };
    normalRecipes = {
        { "Patty Overdrive", 1.0f,  120.0f, 1.15f, 30, "Beef-fuelled speed surge."           },
        { "Secret Sauce Mk1",0.75f,  80.0f, 1.20f, 35, "Boost efficiency way up."            },
        { "Onion Ring Boost",1.10f, 100.0f, 1.18f, 28, "Rings of acceleration."              }
    };
    hardRecipes = {
        { "Ghost Pepper Nitro",  1.0f,  250.0f, 1.30f, 45, "Scorching. Illegal in 12 countries." },
        { "Triple Bypass",       0.60f, 180.0f, 1.40f, 50, "Boost barely drains. Suspect."       },
        { "Black Market Burger", 0.70f, 220.0f, 1.35f, 55, "Maximum performance. Stolen from the best." }
    };
}

void StealAndCook::InitShop()
{
    shopItems = {
        { "Sneaker Soles",       "−30% sus per sighting",           200, false, false, false, 0.5f },
        { "Auto-Collector Mk1",  "Auto-grabs a piece every 3.5s",   350, false, true,  false, 0.0f },
        { "Auto-Cook Oven",      "Auto-cooks when mission succeeds", 500, false, false, true,  0.0f },
        { "Decoy Burger",        "+1.5 sus/s extra decay",          150, false, false, false, 1.5f },
        { "Invisibility Cloak",  "−50% sus increase rate",          800, false, false, false, 2.5f },
    };
}

Recipe StealAndCook::PickRandom(const std::vector<Recipe>& pool)
{
    std::uniform_int_distribution<int> d(0, (int)pool.size() - 1);
    return pool[d(rng)];
}

// ============================================================
//  ImGui
// ============================================================
void StealAndCook::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void StealAndCook::RenderWindow()
{
    if (!*pluginEnabled) return;

    ImGui::SetNextWindowSize(ImVec2(520, 580), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Steal & Cook", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End(); return;
    }

    // ── Top bar ───────────────────────────────────────────────
    ImGui::TextColored({1,0.8f,0,1}, "Stage %d", currentStage);
    ImGui::SameLine(180); ImGui::TextColored({0.3f,1,0.3f,1}, "$%d", playerMoney);
    ImGui::SameLine(300);
    float susN = susRate / 100.0f;
    ImVec4 susCol = susN < 0.4f ? ImVec4{0,1,0,1} : susN < 0.75f ? ImVec4{1,0.7f,0,1} : ImVec4{1,0.1f,0.1f,1};
    ImGui::TextColored(susCol, "SUS %.0f%%", susRate);
    ImGui::ProgressBar(susN, {-1, 5}, "");

    // ── Holding-piece indicator ───────────────────────────────
    if (holdingPiece)
        ImGui::TextColored({1,1,0,1}, ">> Carrying a piece! Get to the blue-half lab! <<");

    // ── NPC cook positions ────────────────────────────────────
    if (!npcs.empty()) {
        ImGui::TextColored({0.8f,0.4f,0.2f,1}, "Cooks patrolling:");
        for (auto& npc : npcs) {
            ImVec4 col = npc.seesPlayer ? ImVec4{1,0.1f,0.1f,1} : ImVec4{0.5f,0.5f,0.5f,1};
            ImGui::TextColored(col, "  %s  (%.0f, %.0f)  %s",
                npc.label.c_str(), npc.pos.X, npc.pos.Y,
                npc.seesPlayer ? "!! SEES YOU !!" : "");
        }
    }

    ImGui::Separator();

    ImGui::BeginTabBar("##tabs");
    if (ImGui::BeginTabItem("Mission"))         { RenderMissionPanel();    ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Research Center")) { RenderResearchCenter();  ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Shop"))            { RenderShop();            ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Buff Status"))     { RenderBuffStatus();      ImGui::EndTabItem(); }
    ImGui::EndTabBar();

    ImGui::End();
}

void StealAndCook::RenderMissionPanel()
{
    ImGui::TextWrapped(
        "You're alone in the field. Fake competitor cooks patrol the orange half.\n"
        "Collect boost pads to grab recipe pieces, then drive to your Research Center\n"
        "(blue half, near your own goal) to deliver them.\n"
        "If a cook sees you carrying a piece, sus goes up!"
    );
    ImGui::Spacing();

    const char* states[] = { "IDLE", "ACTIVE", "SUCCESS", "FAILED" };
    ImVec4      cols[]   = { {0.6f,0.6f,0.6f,1},{1,0.8f,0,1},{0.2f,1,0.4f,1},{1,0.2f,0.2f,1} };
    ImGui::TextColored(cols[(int)missionState], "Status: %s", states[(int)missionState]);

    if (missionState == MissionState::Active) {
        ImGui::Text("Target recipe: %s", missionRecipe.name.c_str());
        ImGui::Text("Pieces: %d / %d delivered", recipePiecesCollected, recipePiecesNeeded);
        ImGui::ProgressBar((float)recipePiecesCollected / recipePiecesNeeded, {-1, 12}, "");
    }
    ImGui::Spacing(); ImGui::Separator();
    ImGui::Text("Start Mission:");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, {0.1f,0.5f,0.1f,1});
    if (ImGui::Button("Easy",   {145,34})) StartMission(Difficulty::Easy);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.4f,0.0f,1});
    if (ImGui::Button("Normal", {145,34})) StartMission(Difficulty::Normal);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.05f,0.05f,1});
    if (ImGui::Button("Hard",   {145,34})) StartMission(Difficulty::Hard);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextColored({0.5f,0.8f,1,1},
        "Easy:   2 cooks | 4 pieces\n"
        "Normal: 3 cooks | 6 pieces  (faster, better vision)\n"
        "Hard:   5 cooks | 8 pieces  (fast + wide vision)");
}

void StealAndCook::RenderResearchCenter()
{
    ImGui::Text("Your hidden lab — blue half, near your goal.");
    ImGui::TextColored({0.4f,1,0.4f,1}, "Recipes researched: %d", researchedRecipes);
    ImGui::Spacing();

    if (missionState == MissionState::Success) {
        ImGui::TextColored({1,0.9f,0.3f,1}, "Ready to cook: %s", missionRecipe.name.c_str());
        ImGui::Text("  Speed +%.0f uu/s  |  Boost %.2fx  |  Jump %.2fx  |  %ds",
            missionRecipe.speedBonus, missionRecipe.boostMultiplier,
            missionRecipe.jumpBoostMult, missionRecipe.durationSeconds);
        ImGui::TextWrapped("  %s", missionRecipe.description.c_str());
        ImGui::Spacing();
        if (!buffActive) {
            if (ImGui::Button("Cook It!", {180,38})) ActivateBuff(missionRecipe);
        } else {
            ImGui::TextColored({1,0.4f,0.4f,1}, "Buff already active!");
        }
    } else if (missionState == MissionState::Active) {
        ImGui::TextColored({1,0.6f,0,1},
            "Mission running — deliver all %d pieces to this zone first.",
            recipePiecesNeeded);
    } else {
        ImGui::TextColored({0.5f,0.5f,0.5f,1}, "No active mission.");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Log:");
    ImGui::BeginChild("##log", {-1,150}, true);
    for (auto it = researchLog.rbegin(); it != researchLog.rend(); ++it)
        ImGui::TextUnformatted(it->c_str());
    ImGui::EndChild();
}

void StealAndCook::RenderShop()
{
    ImGui::Text("Spend coins on automation and upgrades.");
    ImGui::TextColored({0.3f,1,0.3f,1}, "Balance: $%d", playerMoney);
    ImGui::Spacing();

    for (auto& item : shopItems) {
        if (item.purchased) {
            ImGui::TextColored({0.4f,1,0.4f,1}, "[owned] %s", item.name.c_str());
        } else {
            ImGui::Text("[ ] %s", item.name.c_str());
            ImGui::SameLine(280);
            bool ok = playerMoney >= item.cost;
            if (!ok) ImGui::BeginDisabled();
            if (ImGui::Button(("$" + std::to_string(item.cost) + "##" + item.name).c_str())) {
                playerMoney -= item.cost;
                item.purchased = true;
                if (item.autoCollect) autoCollectEnabled = true;
                if (item.autoCook)    autoCookEnabled    = true;
                cvarManager->getCvar(CV_MONEY).setValue(playerMoney);
                researchLog.push_back("Bought: " + item.name);
            }
            if (!ok) ImGui::EndDisabled();
        }
        ImGui::TextColored({0.6f,0.6f,0.6f,1}, "    %s", item.desc.c_str());
        ImGui::Spacing();
    }
}

void StealAndCook::RenderBuffStatus()
{
    if (!buffActive) {
        ImGui::TextColored({0.5f,0.5f,0.5f,1}, "No buff active.");
        return;
    }
    ImGui::TextColored({1,0.9f,0.2f,1}, "COOKING: %s", activeRecipe.name.c_str());
    ImGui::Text("Time remaining: %.1f sec", buffTimer);
    ImGui::ProgressBar(buffTimer / (float)activeRecipe.durationSeconds, {-1,14}, "");
    ImGui::Spacing(); ImGui::Separator();
    ImGui::BulletText("Speed:  +%.0f uu/s",  activeRecipe.speedBonus);
    ImGui::BulletText("Boost:  %.2fx",        activeRecipe.boostMultiplier);
    ImGui::BulletText("Jump:   %.2fx",        activeRecipe.jumpBoostMult);
    ImGui::Spacing();
    if (ImGui::Button("Cancel", {120,30})) DeactivateBuff();
}
