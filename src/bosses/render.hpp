#pragma once

#include <string>
#include <cstdint>
#include <imgui.h>

#include "../renderbase.hpp"

namespace er::bosses {

class Render : public RenderBase {
public:
    void init() override;
    void render(bool &showFull) override;

private:
    void drawInfos();
    void popupEnd(int nbDeath, int timer);
    const std::string seedFormatText_ = "{0} : {1}/{2}";
    std::string killText_;
    std::string challengeText_;
    bool allowRevive_ = false;
    bool endValidated_ = false;
    int lastRegionIndex_ = -1;
    int popupBossIndex_ = -1;
    float posX_ = -10.f;
    float posY_ = 10.f;
    float width_ = 0.12f;
    float height_ = 0.9f;
};


constexpr const char * BUTTON_TEXT_WHEN_DEAD = "CONTINUE";
constexpr const char* BUTTON_TEXT_WHEN_TIMEOUT = "TIMEOUT";
constexpr const char* TEXT_WHEN_DEAD = "You Died!";
constexpr const char* TEXT_WHEN_TIMEOUT = "Timeout!";


const ImVec4 greenColor = ImVec4(0.5f, 0.85f, 0.5f, 0.7f);
const ImVec4 blueColor = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
const ImVec4 redColor = ImVec4(0.85f, 0.5f, 0.5f, 0.7f);

}