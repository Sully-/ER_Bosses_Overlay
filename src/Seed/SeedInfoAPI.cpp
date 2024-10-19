#include "SeedInfoAPI.hpp"
#include "../util/crypto.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <memory>
#include "../bosses/data.hpp"
#include "../steamapi.hpp"
#include "../global.hpp"

namespace er::Seed
{
    SeedInfo gSeedInfo;

    const std::string SeedInfo::name_ = "Training 25";
    const std::string SeedInfo::cryptokey_ = "DIGJ91wt1TmafsDYQH6k4N4cnMiAIfB3";


    std::string getLocalAppDataFolder()
    {
        PWSTR path = NULL;
        HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);

        if (SUCCEEDED(result)) {
            std::wstring ws(path);
            std::string localAppDataPath(ws.begin(), ws.end());

            CoTaskMemFree(path);
            return localAppDataPath;
        }
        return ""; // probably should do better error handling here.
    }

    void SeedInfo::initSeedBinaryFolder()
    {
        #pragma region seed_state_file_copy
        // We try to find the location of EROverlay.dll (should be something like [PATH_TO_THE_SEED_DIRECTORY]/randomizer/dll/EROverlay
        wchar_t resultSeedPath[MAX_PATH];
        auto currentModuleHandle = ::er::gModule;
        if (GetModuleFileNameW(currentModuleHandle, resultSeedPath, MAX_PATH) > 0)
        {
            // move up three directory. (Should end up to something like [PATH_TO_THE_SEED_DIRECTORY]/[seedname].seed
            if (PathRemoveFileSpecW(resultSeedPath)) {
                for (int i = 0; i < 3; ++i) {
                    PathRemoveFileSpecW(&resultSeedPath[0]);
                }
                seedBinaryFolder_ = std::wstring(resultSeedPath);
            }
        }
        seedBinaryFolder_ = seedBinaryFolder_ + L"\\" + std::wstring(name_.begin(), name_.end()) + L".seed";

    }
    SeedInfo::SeedInfo() : IGT_(-1), firstDeathTimeStamp_(-1), lastDeathTimeStamp_(-1), nbDeath_(0), score_(0), suspicious_(false), steamID_(-1), playWithoutUpdates_(false), ended_(false)
    {
        
    }

    std::string SeedInfo::ToString() const
    {
        std::ostringstream oss;
        oss << name_ << ";" << steamID_ << ";" << IGT_ << ";" << nbDeath_ << ";" << (suspicious_ ? 1 : 0) << ";" << firstDeathTimeStamp_ << ";" << lastDeathTimeStamp_ << ";" << score_ << ";" << ended_ << ";" << playWithoutUpdates_;
        return oss.str();
    }

    bool SeedInfo::init()
    {
        initSeedBinaryFolder();
        auto steamid = er::getPlayerSteamID();
        steamID_ = steamid;
        LoadFromFile();
        return true;
    }

    std::string SeedInfo::getName() const
    {
        return name_;
    }

    bool SeedInfo::FromString(const std::string& str, SeedInfo& info)
    {
        int suspicious = 0;
        int ended = 0;
        int playWithoutUpdates = 0;
        char name[100]; // not used
        if (std::sscanf(str.c_str(), "%99[^;];%d;%d;%d;%d;%d;%d;%d;%d;%d", &name, &info.steamID_, &info.IGT_, &info.nbDeath_, &suspicious, &info.firstDeathTimeStamp_, &info.lastDeathTimeStamp_, &info.score_, &ended, &playWithoutUpdates) == 10) {
            info.suspicious_ = suspicious != 0;
            info.ended_ = ended != 0;
            info.playWithoutUpdates_ = playWithoutUpdates  != 0;
            return true;
        }
        return false;
    }

    void SeedInfo::LoadFromFile()
    {
        std::unique_lock lock(mutex_);

        std::string directory = getLocalAppDataFolder() + "\\DRL";
        std::ifstream seedfile(directory + "\\" + name_ + ".seed");
        if (!seedfile.is_open()) {
            std::cerr << "Error: Could not open the file for writing!" << std::endl;
        }
        std::stringstream buffer;
        buffer << seedfile.rdbuf();

        FromString(er::util::decrypt(buffer.str(), cryptokey_), *this);

        seedfile.close();
        saveToFile();
    }

    bool SeedInfo::saveToFile()
    {
        auto encryptedState = er::util::encrypt(this->ToString(), cryptokey_);
        std::string directory = getLocalAppDataFolder() + "\\DRL";
        if (CreateDirectory(directory.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            std::ofstream seedfile(directory + "\\" + name_ + ".seed");
            if (!seedfile.is_open()) {
                std::cerr << "Error: Could not open the file for writing!" << std::endl;
                return 1;
            }
            seedfile << encryptedState;
            seedfile.close();

            FILE* file = _wfopen(seedBinaryFolder_.c_str(), L"w");
            std::wfstream resultseedFile(seedBinaryFolder_);
            resultseedFile << std::wstring(encryptedState.begin(), encryptedState.end());
            resultseedFile.close();
        }
        return false;
    }


    void SeedInfo::update()
    {
        std::unique_lock lock(mutex_);
        std::unique_lock bosslock(er::bosses::gBossDataSet.mutex());

        bool needsave = false;
     
        // We already timedout, no need to update the state the player can still continue.
        if (ended_)
            return;

        IGT_ = er::bosses::gBossDataSet.inGameTime();
        if (IGT_ >= er::bosses::TWO_HOURS_IN_MILLISECONDS)
        {
            needsave = true;
            ended_ = true;
        }
        if (er::bosses::gBossDataSet.deaths() != nbDeath_)
        {
            needsave = true;
            if (firstDeathTimeStamp_ == -1)
            {
                firstDeathTimeStamp_ = IGT_;
            }
            lastDeathTimeStamp_ = IGT_;
            nbDeath_ = er::bosses::gBossDataSet.deaths();
            ended_ = true;
            if (nbDeath_ > 1)
            {
                suspicious_ = true;
            }
        }

        if (er::bosses::gBossDataSet.count() != score_)
        {
            score_ = er::bosses::gBossDataSet.count();
            needsave = true;
        }

        if (needsave)
            saveToFile();
    }
    void SeedInfo::Continue()
    {
        std::unique_lock lock(mutex_);
        ended_ = false;
        saveToFile();
    }
    void SeedInfo::ContinueToPlayWithoutUpdates()
    {
        std::unique_lock lock(mutex_);
        playWithoutUpdates_ = true;
        saveToFile();
    }
}