#pragma once

#include <string>

namespace er {

bool initSteamAPI();
const std::wstring &getGameLanguage();
bool isDLCInstalled(unsigned int dlc);
unsigned long long getPlayerSteamID();
}
