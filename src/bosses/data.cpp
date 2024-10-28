#include "data.hpp"
#include <MinHook.h>

#include "../hooking.hpp"
#include "../memory.hpp"
#include "../global.hpp"
#include "../config.hpp"
#include "../steamapi.hpp"

#include <nlohmann/json.hpp>
#include <ini.h>

#include <fstream>
#include <cstdio>

namespace er::bosses {

BossDataSet gBossDataSet;
static std::vector<bool> deadSwapTmp;
static std::vector<int> deadByRegionSwapTmp;

void BossDataSet::load(bool hasDLC) {
    nlohmann::ordered_json j;
    std::wstring name = er::gConfig.getw("boss.data_file", L"bosses.json");
    std::wstring lang = er::gConfig.getw("common.language", L"");
    if (lang.empty()) {
        lang = getGameLanguage();
    }
    std::wstring filename = std::wstring(er::gModulePath) + L"\\data\\" + lang + L"\\" + name;
    std::ifstream ifs(filename.c_str());
    if (!ifs) {
        /* fallback to use english */
        lang = L"engUS";
        filename = std::wstring(er::gModulePath) + L"\\data\\" + lang + L"\\" + name;
        ifs.open(filename.c_str());
        if (!ifs) {
            fwprintf(stderr, L"Unable to open %ls\n", filename.c_str());
            return;
        }
    }
    j = nlohmann::ordered_json::parse(ifs);
    for (auto &p: j.items()) {
        if (!hasDLC && p.value()["dlc"] == 1) {
            continue;
        }
        auto regionIndex = regions_.size();
        auto &rd = regions_.emplace_back();
        rd.name = p.value()["region_name"];
        for (auto &n: p.value()["regions"]) {
            regionMap_[n.get<uint32_t>()] = (int)regionIndex;
        }
        for (auto &n: p.value()["bosses"]) {
            auto index = bosses_.size();
            auto &bd = bosses_.emplace_back();
            bd.boss = n["boss"];
            bd.place = n["place"];
            bd.flagId = n["flag_id"].get<uint32_t>();
            bd.index = index;
            bd.tip = bd.boss + ": " + bd.place;
            rd.bosses.push_back(index);
            bd.regionIndex = regionIndex;
        }
    }
    dead_.resize(bosses_.size());
    deadSwapTmp.resize(bosses_.size());
    deadByRegionSwapTmp.resize(regions_.size());

    challengeMode_ = gConfig.enabled("boss.challenge_mode");
    if (!challengeMode_) return;
    challengeDeathCount_ = gConfig.get("boss.challenge_death_count", 0);
}

void BossDataSet::loadConfig() {
    if (!challengeMode_) return;
    auto filename = std::wstring(::er::gModulePath) + L"\\Challenge.txt";
    auto *f = _wfopen(filename.c_str(), L"rt");
    if (f == nullptr) {
        return;
    }
    ini_parse_file(f, [](void *user, const char *section, const char *name, const char *value) {
        if (section != nullptr && section[0] != 0) {
            return 0;
        }
        auto *bds = static_cast<BossDataSet *>(user);
        if (lstrcmpA(name, "tries") == 0) {
            bds->challengeTries_ = std::strtol(value, nullptr, 10);
        } else if (lstrcmpA(name, "deaths_on_start") == 0) {
            bds->challengeDeathsOnStart_ = std::strtol(value, nullptr, 10);
        } else if (lstrcmpA(name, "best") == 0) {
            bds->challengeBest_ = std::strtol(value, nullptr, 10);
        }
        return 1;
    }, this);
    fclose(f);
    changeEvent_ = FindFirstChangeNotificationW(::er::gModulePath, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
}

void BossDataSet::saveConfig() const {
    auto filename = std::wstring(::er::gModulePath) + L"\\Challenge.txt";
    auto *f = _wfopen(filename.c_str(), L"wt");
    if (f == nullptr) {
        fprintf(stderr, "Unable to open %ls for writing\n", filename.c_str());
        fflush(stderr);
        return;
    }
    fprintf(f, "# Total tries\ntries=%d\n", challengeTries_);
    fprintf(f, "# Personal Best\nbest=%d\n", challengeBest_);
    fprintf(f, "\n# !!!DO NOT EDIT THIS!!!\ndeaths_on_start=%d\n", challengeDeathsOnStart_);
    fflush(f);
    fclose(f);
    while (WaitForSingleObject(changeEvent_, 0) == WAIT_OBJECT_0) {
        FindNextChangeNotification(changeEvent_);
    }
}


typedef void(__fastcall* TargetFunctionType)(void* context, uint32_t event_flag_id, uint8_t value);
TargetFunctionType originalFunction = nullptr;


void __fastcall Hookedset_event_flag(void* context, uint32_t event_flag_id, uint8_t value) {
    if (event_flag_id == 3005)
        gBossDataSet.set_event_flag_called(event_flag_id, value);
    if (originalFunction) {
        originalFunction(context, event_flag_id, value);
    }
}

void BossDataSet::initMemoryAddresses() {
    {
        Signature sig("48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3");
        auto res = sig.scan();
        if (res) {
            auto addr = res.add(res.add(3).as<std::uint32_t &>() + 7);
            gameDataMan_ = addr.as<uintptr_t>();
        }
    }
    {
        Signature sig("48 8B 3D ?? ?? ?? ?? 48 85 FF ?? ?? 32 C0 E9");
        auto res = sig.scan();
        if (res) {
            auto addr = res.add(res.add(3).as<std::uint32_t &>() + 7);
            eventFlagMan_ = addr.as<uintptr_t>();
        }
    }
    {
        Signature sig("48 8B 0D ?? ?? ?? ?? 48 ?? ?? ?? 44 0F B6 61 ?? E8 ?? ?? ?? ?? 48 63 87 ?? ?? ?? ?? 48 ?? ?? ?? 48 85 C0");
        auto res = sig.scan();
        if (res) {
            auto addr = res.add(res.add(3).as<std::uint32_t &>() + 7);
            fieldArea_ = addr.as<uintptr_t>();
        }
    }


    // hook the call to set_event_flag so we can catch called even flag
    {
        Signature sig("48 89 5c 24 08 44 8b 49 1c 44 8b d2 33 d2 41 8b c2 41 f7 f1 41 8b d8 4c 8b d9");
        auto res = sig.scan();
        if (res)
        {
            auto addr = res.as<std::uintptr_t>();
            void* set_event_flag_addr = reinterpret_cast<void*>(addr);
            // Initialiser MinHook
            if (MH_Initialize() != MH_OK) {
                return;  // Erreur à l'initialisation
            }

            if (MH_CreateHook(set_event_flag_addr, &Hookedset_event_flag, reinterpret_cast<LPVOID*>(&originalFunction)) != MH_OK) {
                return;  // Erreur lors de la création du hook
            }

            // Activer le hook
            if (MH_EnableHook(set_event_flag_addr) != MH_OK) {
                return;  // Erreur lors de l'activation du hook
            }
        }
        
    }
}

void BossDataSet::update() {
    checkForConfigChange();
    if (er::gHooking->screenState() != 0) {
        return;
    }
    auto igt = inGameTime();
    if (igt == 0) return;
    updateBosses();
    if (igt < 0) return;
    if (!challengeMode_)
        updateDeathCount();
    else
        updateChallengeMode();
}

void BossDataSet::revive(int index) {
    if (flagAddress_ == 0) {
        return;
    }
    auto &b = bosses_[index];
    *(uint8_t *)(b.offset) &= ~b.bits;
}

void BossDataSet::resolveFlag(uint32_t flagId, uintptr_t &offset, uint8_t &bits) const {
    auto addr = MemoryHandle(eventFlagMan_).as<uintptr_t &>();
    if (addr == 0) {
        return;
    }
    auto divisor = *(uint32_t *)(addr + 0x1C);
    if (divisor == 0) {
        return;
    }
    auto category = flagId / divisor;
    auto least_significant_digits = flagId - category * divisor;
    auto current_element = MemoryHandle(addr + 0x38).as<uintptr_t &>();
    auto current_sub_element = MemoryHandle(current_element + 0x08).as<uintptr_t &>();
    while (*(uint8_t *)(current_sub_element + 0x19) == 0) {
        if (*(uint32_t *)(current_sub_element + 0x20) < category)
            current_sub_element = MemoryHandle(current_sub_element + 0x10).as<uintptr_t &>();
        else {
            current_element = current_sub_element;
            current_sub_element = MemoryHandle(current_sub_element).as<uintptr_t &>();
        }
    }
    if (current_element == current_sub_element) {
        offset = 0;
        bits = 0;
        return;
    }
    auto mystery_value = *(uint32_t *)(current_element + 0x28) - 1;
    uintptr_t calculated_pointer = 0;
    if (mystery_value != 0) {
        if (mystery_value != 1)
            calculated_pointer = MemoryHandle(current_element + 0x30).as<uintptr_t &>();
        else {
            offset = 0;
            bits = 0;
            return;
        }
    } else {
        calculated_pointer = (*(uint32_t *)(addr + 0x20)) * (*(uint32_t *)(current_element + 0x30)) + flagAddress_;
    }

    if (calculated_pointer == 0) {
        offset = 0;
        bits = 0;
        return;
    }
    auto thing = 7 - (least_significant_digits & 7);
    auto shifted = least_significant_digits >> 3;
    offset = calculated_pointer + shifted;
    bits = 1 << thing;
}

void BossDataSet::updateBosses() {
    if (!flagResolved_) {
        if (flagAddress_ == 0) {
            auto addr1 = MemoryHandle(eventFlagMan_).as<uintptr_t &>();
            if (addr1 == 0) {
                return;
            }
            auto addr2 = MemoryHandle(addr1 + 0x28).as<uintptr_t &>();
            if (addr2 == 0) {
                return;
            }
            flagAddress_ = addr2;
        }
        for (auto &b: bosses_) {
            resolveFlag(b.flagId, b.offset, b.bits);
#if !defined(NDEBUG)
            std::wstring ws;
            ws.resize(MultiByteToWideChar(CP_UTF8, 0, b.boss.c_str(), -1, nullptr, 0));
            MultiByteToWideChar(CP_UTF8, 0, b.boss.c_str(), -1, ws.data(), (int)ws.size());
            while (!ws.empty() && ws.back() == 0) ws.pop_back();
            fwprintf(stderr, L"%ls: %zX %X\n", ws.c_str(), b.offset - flagAddress_, b.bits);
#endif
        }
        flagResolved_ = true;
    }
    int cnt = 0;
    std::fill(deadByRegionSwapTmp.begin(), deadByRegionSwapTmp.end(), 0);
    size_t sz = bosses_.size();
    for (size_t i = 0; i < sz; i++) {
        auto &b = bosses_[i];
        bool al = (*(uint8_t *)(b.offset) & b.bits) != 0;
        deadSwapTmp[i] = al;
        if (al) {
            cnt++;
            deadByRegionSwapTmp[b.regionIndex]++;
        }
    }
    std::unique_lock lock(mutex_);
    sz = regions_.size();
    for (size_t i = 0; i < sz; i++) {
        regions_[i].count = deadByRegionSwapTmp[i];
    }
    dead_.swap(deadSwapTmp);
    count_ = cnt;
    if (challengeMode_ && cnt > challengeBest_ && challengeDeaths() <= challengeDeathCount_) {
        challengeBest_ = cnt;
        saveConfig();
    }
    auto addr1 = *(uintptr_t *)fieldArea_;
    if (addr1 == 0) {
        return;
    }
    auto mapId = *(uint32_t *)(addr1 + (er::gGameVersion < 0x0002000200000000ULL ? 0xE4 : 0xE8));
    if (mapId == 0 || mapId == mapId_) return;
    mapId_ = mapId;
    auto ite = regionMap_.find(mapId / 1000);
    if (ite == regionMap_.end()) return;
    regionIndex_ = ite->second;
}

void BossDataSet::updateDeathCount()
{
    auto deaths = currentDeathCount() + uncountedDeath_;;
    std::unique_lock lock(mutex_);
    if (deaths != playerDeaths_) {
        playerDeaths_ = deaths;
    }
}

void BossDataSet::updateChallengeMode() {
    if (reachStrandedGraveyardFlagOffset_ == 0) {
        // Flag 101: Reached Stranded Graveyard
        resolveFlag(101, reachStrandedGraveyardFlagOffset_, reachStrandedGraveyardFlagBits_);
    }
    auto deaths = currentDeathCount() + uncountedDeath_;
    auto reached = (*(uint8_t *)reachStrandedGraveyardFlagOffset_ & reachStrandedGraveyardFlagBits_) != 0;
    bool needSave = false;
    std::unique_lock lock(mutex_);
    if (reached != reachedStrandedGraveyard_) {
        reachedStrandedGraveyard_ = reached;
        if (reached) {
            challengeDeathsOnStart_ = deaths;
            needSave = true;
        } else {
            playerDeaths_ = 0;
            challengeDeathsOnStart_ = 0;
            needSave = true;
        }
    }
    if (deaths != playerDeaths_) {
        playerDeaths_ = deaths;
        if (challengeDeaths() == challengeDeathCount_ + 1) {
            challengeTries_++;
        }
        needSave = true;
    }
    if (needSave) saveConfig();
}

void BossDataSet::checkForConfigChange() {
    if (changeEvent_ == INVALID_HANDLE_VALUE) return;
    bool needReload = false;
    while (WaitForSingleObject(changeEvent_, 0) == WAIT_OBJECT_0) {
        needReload = true;
        FindNextChangeNotification(changeEvent_);
    }
    if (needReload) loadConfig();
}

int BossDataSet::inGameTime() const {
    auto addr = MemoryHandle(gameDataMan_).as<uintptr_t &>();
    if (addr == 0) {
        return -1;
    }
    return MemoryHandle(addr + 0xA0).as<int&>();
}

int BossDataSet::currentDeathCount() const {
    auto addr = MemoryHandle(gameDataMan_).as<uintptr_t &>();
    if (addr == 0) {
        return 0;
    }
    return MemoryHandle(addr + 0x94).as<int&>();
}


void BossDataSet::set_event_flag_called(uint32_t param1, uint8_t param2)
{
    std::unordered_set< uint32_t > invasion_to_remove = {};

    std::unique_lock lock(gBossDataSet.mutex());
    for (const int& invasion_id : invasionThatDidntCauseDeath_)
    {
        uintptr_t suspiciousInvasionFlagOffset_ = 0;
        uint8_t suspiciousInvasionFlagBits_ = 0;
        this->resolveFlag(invasion_id, suspiciousInvasionFlagOffset_, suspiciousInvasionFlagBits_);
        auto suspicousInvasion = (*(uint8_t*)suspiciousInvasionFlagOffset_ & suspiciousInvasionFlagBits_) != 0;
        if (suspicousInvasion)
        {
            uncountedDeath_++;
            invasion_to_remove.insert(invasion_id);
        }
    }
    for (const int& invasion_id_to_remove : invasion_to_remove)
    {
        invasionThatDidntCauseDeath_.erase(invasion_id_to_remove);
    }
}

}
