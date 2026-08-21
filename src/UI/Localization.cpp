#include "UI/Localization.h"

#include "Settings.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    std::unordered_map<std::string, std::string> g_translations;
    std::string g_language;
    ImVector<ImWchar> g_glyph_ranges;
    bool g_fontReloadRequested{ false };
    std::vector<UICommon::LanguageOption> g_languages;

    std::filesystem::path GetLocaleDirectory()
    {
        return std::filesystem::path("Data") / "F4SE" / "Plugins" /
               "OpenAnimationReplacer" / "locales";
    }

    std::filesystem::path GetLocalePath(const std::string& a_language)
    {
        return GetLocaleDirectory() / (a_language + ".json");
    }

    std::vector<std::filesystem::path> GetFontCandidates()
    {
        const auto configured = Settings::GetSingleton()->sCJKFontPath;
        if (!configured.empty()) {
            return { std::filesystem::path(configured) };
        }

        return {
            std::filesystem::path("C:\\Windows\\Fonts\\msyh.ttc"),
            std::filesystem::path("C:\\Windows\\Fonts\\simhei.ttf"),
            std::filesystem::path("C:\\Windows\\Fonts\\simsun.ttc")
        };
    }

    bool AddCurrentLanguageFont()
    {
        auto& io = ImGui::GetIO();
        ImFontConfig defaultFontConfig;
        defaultFontConfig.SizePixels = 18.0f;
        if (g_translations.empty()) {
            io.FontDefault = io.Fonts->AddFontDefault(&defaultFontConfig);
            return false;
        }

        ImFontGlyphRangesBuilder rangesBuilder;
        rangesBuilder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        for (const auto& [source, translation] : g_translations) {
            rangesBuilder.AddText(translation.c_str());
        }
        g_glyph_ranges.clear();
        rangesBuilder.BuildRanges(&g_glyph_ranges);

        for (const auto& path : GetFontCandidates()) {
            if (!std::filesystem::exists(path)) {
                continue;
            }

            ImFont* font = io.Fonts->AddFontFromFileTTF(
                path.string().c_str(),
                18.0f,
                nullptr,
                g_glyph_ranges.Data);
            if (font) {
                io.FontDefault = font;
                logger::info("[OAR] CJK UI font loaded: '{}'", path.string());
                return true;
            }
        }

        io.FontDefault = io.Fonts->AddFontDefault(&defaultFontConfig);
        logger::warn("[OAR] No CJK UI font found; Chinese text may render as missing-glyph boxes");
        return false;
    }
}

namespace UICommon
{
    void LoadLocalization()
    {
        g_translations.clear();
        g_language = Settings::GetSingleton()->sLanguage;
        if (g_language.empty()) {
            g_language = "en_US";
        }

        if (g_language == "en_US" || g_language == "en") {
            logger::info("[OAR] UI language: English fallback");
            return;
        }

        const auto path = GetLocalePath(g_language);
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            logger::warn("[OAR] UI language file not found: '{}' - using English fallback", path.string());
            return;
        }

        try {
            const auto json = nlohmann::json::parse(file);
            if (!json.is_object()) {
                logger::warn("[OAR] UI language file is not an object: '{}'", path.string());
                return;
            }

            for (const auto& [key, value] : json.items()) {
                if (value.is_string()) {
                    g_translations.emplace(key, value.get<std::string>());
                }
            }
            logger::info("[OAR] UI language '{}' loaded: {} translations", g_language, g_translations.size());
        } catch (const std::exception& e) {
            logger::error("[OAR] Failed to parse UI language file '{}': {}", path.string(), e.what());
            g_translations.clear();
        }
    }

    const char* T(const char* a_source)
    {
        if (!a_source || g_translations.empty()) {
            return a_source;
        }

        const auto it = g_translations.find(a_source);
        return it != g_translations.end() ? it->second.c_str() : a_source;
    }

    const std::vector<LanguageOption>& GetAvailableLanguages()
    {
        if (!g_languages.empty()) {
            return g_languages;
        }

        g_languages.push_back({ "en_US", "English" });

        std::error_code error;
        const auto localeDirectory = GetLocaleDirectory();
        if (std::filesystem::is_directory(localeDirectory, error)) {
            for (const auto& entry : std::filesystem::directory_iterator(localeDirectory, error)) {
                if (error || !entry.is_regular_file(error) || entry.path().extension() != ".json") {
                    continue;
                }

                const auto id = entry.path().stem().string();
                if (id.empty() || std::any_of(g_languages.begin(), g_languages.end(),
                        [&](const auto& option) { return option.id == id; })) {
                    continue;
                }

                const auto sourceName = id == "zh_CN" ? "Simplified Chinese" : id;
                g_languages.push_back({ id, sourceName });
            }
        }

        std::sort(g_languages.begin(), g_languages.end(),
            [](const auto& left, const auto& right) { return left.id < right.id; });
        return g_languages;
    }

    bool SetLanguage(const std::string& a_language)
    {
        const auto& languages = GetAvailableLanguages();
        const auto it = std::find_if(languages.begin(), languages.end(),
            [&](const auto& option) { return option.id == a_language; });
        if (it == languages.end()) {
            logger::warn("[OAR] Ignoring unknown UI language '{}'", a_language);
            return false;
        }

        auto* settings = Settings::GetSingleton();
        settings->sLanguage = it->id;
        LoadLocalization();
        settings->Save();
        g_fontReloadRequested = true;
        logger::info("[OAR] UI language changed to '{}'", settings->sLanguage);
        return true;
    }

    bool ConsumeFontReloadRequest()
    {
        const bool requested = g_fontReloadRequested;
        g_fontReloadRequested = false;
        return requested;
    }

    bool LoadCJKFont()
    {
        auto& io = ImGui::GetIO();
        io.Fonts->Clear();
        return AddCurrentLanguageFont();
    }

    bool RebuildFontAtlas()
    {
        if (!ImGui::GetCurrentContext()) {
            return false;
        }

        return LoadCJKFont();
    }
}
