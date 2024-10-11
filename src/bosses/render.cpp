#include "render.hpp"
#include "data.hpp"

#include "../config.hpp"
#include "../util/string.hpp"

#include <imgui.h>

#include <fmt/format.h>

#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

namespace er::bosses {


inline static std::vector<float> split(const std::string &s) {
    std::vector<float> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) {
            elems.push_back(0);
            continue;
        }
        if (item.back() == '%') {
            item.pop_back();
            auto val = std::stof(item);
            elems.push_back(std::clamp(val / 100.f, -0.999999f, 0.999999f));
            continue;
        }
        elems.push_back(std::stof(item));
    }
    return elems;
}

void Render::init() {
    killText_ = gConfig["boss.boss_kill_text"];
    challengeText_ = gConfig["boss.challenge_status_text"];
    const std::string from = "$n";
    const std::string to = "\n";
    util::replaceAll(killText_, from, to);
    util::replaceAll(challengeText_, from, to);
    allowRevive_ = gConfig.enabled("boss.allow_revive");
    const auto &pos = gConfig["boss.panel_pos"];
    auto posVec = split(pos);
    if (posVec.size() >= 4) {
        posX_ = posVec[0];
        posY_ = posVec[1];
        width_ = posVec[2];
        height_ = posVec[3];
    }
}

inline static float calculatePos(float w, float n) {
    if (n >= 1.f) {
        return n;
    }
    if (n >= 0.f) {
        return w * n;
    }
    if (n <= -1.f) {
        return w + n;
    }
    return w + w * n;
}

std::string ConvertMillisecondsToTimeString(int milliseconds) {
    // Convertir les millisecondes en une durée
    auto duration = std::chrono::milliseconds(milliseconds);

    // Extraire les heures, minutes et secondes
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
    duration -= std::chrono::hours(hours);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
    duration -= std::chrono::minutes(minutes);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    // Utiliser std::ostringstream pour formater le temps
    std::ostringstream timeStream;
    timeStream << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;

    return timeStream.str();

}

void Render::render(bool &showFull) {
    auto* vp = ImGui::GetMainViewport();
    
    auto deaths = gBossDataSet.challengeMode() ? gBossDataSet.challengeDeaths() : gBossDataSet.deaths();
    // If you died or we are at the end of the timer and we did not click on 'OK' on this session.
    if ((deaths > 0 || gBossDataSet.inGameTime() >= TWO_HOURS_IN_MILLISECONDS) && !endValidated_)
    {
        const char* dead_text = "Aller on s'arrete maintenant, C'est fini!";
        ImVec2 textSize = ImGui::CalcTextSize(dead_text);
        ImGui::OpenPopup("##dead");
        ImVec2 deadpopuppos;
        
        deadpopuppos.x = (vp->Size.x - textSize.x) / 2.0f; // Centrage horizontal
        deadpopuppos.y = (vp->Size.y - textSize.y) / 2.0f; // Centrage vertical

        ImGui::SetNextWindowPos(deadpopuppos, ImGuiCond_Appearing, ImVec2(.5f, .5f));
        if (ImGui::BeginPopupModal("##dead",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(dead_text);
            ImGui::SameLine();
            if (ImGui::Button("OK :(")) {
                ImGui::CloseCurrentPopup();
                endValidated_ = true;
                popupBossIndex_ = -1;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::SetNextWindowPos(ImVec2(calculatePos(vp->Size.x, posX_), calculatePos(vp->Size.y, posY_)),
                            ImGuiCond_Always,
                            ImVec2(posX_ >= 0 ? 0.f : 1.f, posY_ >= 0 ? 0.f : 1.f));
    if (!showFull) {
        ImGui::Begin("##bosses_window",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                         | ImGuiWindowFlags_AlwaysAutoResize);
        {
            std::unique_lock lock(gBossDataSet.mutex());
            if (gBossDataSet.challengeMode()) {
                auto text = fmt::format(challengeText_, gBossDataSet.count(), gBossDataSet.total(), gBossDataSet.challengeBest(), gBossDataSet.challengeTries(), gBossDataSet.challengeDeaths());
                ImGui::TextUnformatted(text.c_str());
            } else {
                auto totalseconds = gBossDataSet.inGameTime();
                auto text = fmt::format("[{2}] DRL Training : {0}/{1}", gBossDataSet.count(), gBossDataSet.total(), ConvertMillisecondsToTimeString(totalseconds));
                ImGui::TextUnformatted(text.c_str());
                if (deaths > 0)
                {
                    ImGui::SameLine();
                    auto deathText = fmt::format("Deaths : {0}", deaths);
                    ImGui::TextUnformatted(deathText.c_str());
                }

            }
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##bosses_arrow", ImGuiDir_Down)) {
            showFull = true;
        }
    } else {
        ImGui::SetNextWindowSize(ImVec2(calculatePos(vp->Size.x, std::abs(width_)), calculatePos(vp->Size.y, std::abs(height_))), ImGuiCond_Always);
        ImGui::Begin("##bosses_window",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        std::unique_lock lock(gBossDataSet.mutex());
        auto regionIndex = gBossDataSet.regionIndex();
        if (regionIndex != lastRegionIndex_) {
            lastRegionIndex_ = regionIndex;
        } else {
            regionIndex = -1;
        }
        if (gBossDataSet.challengeMode()) {
            auto text = fmt::format(challengeText_, gBossDataSet.count(), gBossDataSet.total(), gBossDataSet.challengeBest(), gBossDataSet.challengeTries(), gBossDataSet.challengeDeaths());
            ImGui::TextUnformatted(text.c_str());
        } else {
            auto text = fmt::format(killText_, gBossDataSet.count(), gBossDataSet.total(), gBossDataSet.challengeDeaths());
            ImGui::TextUnformatted(text.c_str());
        }
        auto &style = ImGui::GetStyle();
        ImGui::SameLine(
            ImGui::GetWindowWidth() - ImGui::GetFrameHeight() - style.WindowPadding.x - style.FramePadding.x);
        if (ImGui::ArrowButton("##bosses_arrow", ImGuiDir_Up)) {
            showFull = false;
        }
        ImGui::BeginChild("##bosses_list", ImGui::GetContentRegionAvail());
        const auto &bosses = gBossDataSet.bosses();
        const auto &regions = gBossDataSet.regions();
        const auto &dead = gBossDataSet.dead();
        int sz = (int)regions.size();
        bool popup = false;
        for (int i = 0; i < sz; i++) {
            const auto &region = regions[i];
            auto bossCount = (int)region.bosses.size();
            if (regionIndex >= 0) {
                ImGui::SetNextItemOpen(i == regionIndex);
            }
            if (ImGui::TreeNode(&region, "%d/%d %s", region.count, bossCount, region.name.c_str())) {
                for (int j = 0; j < bossCount; j++) {
                    auto &bd = bosses[region.bosses[j]];
                    bool on = dead[bd.index];
                    if (ImGui::Checkbox(bd.boss.c_str(), &on, on) && dead[bd.index] && allowRevive_) {
                        popupBossIndex_ = (int)bd.index;
                        popup = true;
                    }
                    if (ImGui::IsItemHovered() && !bd.tip.empty()) {
                        ImGui::SetTooltip("%s", bd.tip.c_str());
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();
        if (allowRevive_) {
            if (popup) {
                ImGui::OpenPopup("##bosses_revive_confirm");
                ImGui::SetNextWindowPos(ImVec2(vp->Size.x * 0.94f, vp->Size.y / 2.0f),
                                        ImGuiCond_Appearing,
                                        ImVec2(.5f, .5f));
            }
            if (ImGui::BeginPopupModal("##bosses_revive_confirm",
                                       nullptr,
                                       ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                           | ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Revive %s?", bosses[popupBossIndex_].boss.c_str());
                if (ImGui::Button("Yes!")) {
                    gBossDataSet.revive(popupBossIndex_);
                    ImGui::CloseCurrentPopup();
                    popupBossIndex_ = -1;
                }
                ImGui::SameLine();
                if (ImGui::Button("NO!")) {
                    ImGui::CloseCurrentPopup();
                    popupBossIndex_ = -1;
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::End();
}

}
