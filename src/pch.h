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

// BakkesMod SDK
// Real paths confirmed from bakkesmodorg/BakkesModSDK:
//   include/bakkesmod/plugin/bakkesmodplugin.h
//   include/bakkesmod/plugin/pluginwindow.h
//   include/bakkesmod/wrappers/GameWrapper.h
//   include/bakkesmod/wrappers/GameObject/CarWrapper.h
//   include/bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"
#include "imgui/imgui.h"
