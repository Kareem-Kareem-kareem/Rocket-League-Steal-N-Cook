#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <random>
#include <functional>
#include <sstream>

// BakkesMod SDK (paths confirmed from bakkesmodorg/BakkesModSDK)
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"

// ImGui lives in the plugin's own source tree (not in the SDK).
// Fetched by build.yml into vendor/imgui/ at CI time,
// and expected locally at <repo>/vendor/imgui/imgui.h
#include "imgui.h"   // resolved via the vendor/imgui include path in CMakeLists
