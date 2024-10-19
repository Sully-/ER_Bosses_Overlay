#ifndef SEEDINFOAPI_HPP
#define SEEDINFOAPI_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <fmt/format.h>
#include <memory>
#include <cstdio>
#include <sstream>
#include <mutex>
#include <thread>

#include "../util/crypto.hpp"

namespace er::Seed
{
    class SeedInfo {

    public:

        SeedInfo();

        std::string getName() const;
        static bool FromString(const std::string& str, SeedInfo& info);
        inline std::mutex& mutex() { return mutex_; }
        inline int Deaths() const { return nbDeath_; }
        inline int Score() const { return score_; }
        inline bool Ended() const { return ended_; }
        inline int IGT() const { return IGT_; }
        inline int PlayWithoutUpdates() const { return playWithoutUpdates_; }
        inline std::string GetCryptedString() const { return er::util::encrypt(this->ToString(), cryptokey_);  }
        
        
        std::string ToString() const;
        void LoadFromFile();
        bool saveToFile();
        void update();
        void Continue();
        void ContinueToPlayWithoutUpdates();

    private:

        int IGT_;
        int firstDeathTimeStamp_;
        int lastDeathTimeStamp_;
        int nbDeath_;
        int score_;
        bool suspicious_;
        int steamID_;
        bool ended_;
        bool playWithoutUpdates_;
        static const std::string name_;
        static const std::string cryptokey_;
        std::mutex mutex_;

        std::wstring seedBinaryFolder_;
        void initSeedBinaryFolder();
    };

    extern SeedInfo gSeedInfo;
}

#endif // SEEDINFOAPI_HPP