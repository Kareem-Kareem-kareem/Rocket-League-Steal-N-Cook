#pragma once
// Pre-compiled header for StealAndCook BakkesMod plugin

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
#include <iomanip>

// BakkesMod SDK (expected at ../BakkesModSDK relative to repo)
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameWrapper.h"                           // flat in wrappers/
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"                 // one level deeper
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"  // correct subpath
#include "imgui/imgui.h"
