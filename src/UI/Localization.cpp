#include "UI/Localization.h"

#include "Settings.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <Windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    std::unordered_map<std::string, std::string> g_translations;
    std::string g_language;
    ImVector<ImWchar> g_glyph_ranges;
    bool g_fontReloadRequested{ false };
    std::vector<UICommon::LanguageOption> g_languages;

	std::filesystem::path GetPluginDirectory()
	{
		HMODULE module = nullptr;
		const auto anchor = reinterpret_cast<LPCWSTR>(std::addressof(g_translations));
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				anchor, &module)) {
			logger::warn("[OAR] Could not resolve the plugin module for localization paths");
			return {};
		}

		std::array<wchar_t, 32768> modulePath{};
		const DWORD length = GetModuleFileNameW(module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
		if (length == 0 || length >= modulePath.size()) {
			logger::warn("[OAR] Could not resolve the plugin path for localization files");
			return {};
		}
		return std::filesystem::path(std::wstring_view(modulePath.data(), length)).parent_path();
	}

    std::filesystem::path GetLocaleDirectory()
    {
		const auto pluginDirectory = GetPluginDirectory();
		if (!pluginDirectory.empty()) {
			return pluginDirectory / "OpenAnimationReplacer" / "locales";
		}
		return std::filesystem::path("Data") / "F4SE" / "Plugins" /
			"OpenAnimationReplacer" / "locales";
    }

    std::vector<std::filesystem::path> GetLocaleDirectories()
    {
        std::vector<std::filesystem::path> directories;
        const auto addUnique = [&directories](const std::filesystem::path& a_directory) {
            if (a_directory.empty() || std::find(directories.begin(), directories.end(), a_directory) != directories.end()) {
                return;
            }
            directories.push_back(a_directory);
        };

        // The module directory is the normal location for OAR's own files.
        // The relative Data path is also required so MO2 can merge extension
        // packs supplied by other mods into the same virtual directory.
        addUnique(GetLocaleDirectory());
        addUnique(std::filesystem::path("Data") / "F4SE" / "Plugins" /
            "OpenAnimationReplacer" / "locales");
        return directories;
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

	std::vector<std::string> ExtractFormatTokens(const std::string& a_text)
	{
		static const std::regex printfToken(
			R"(%(?:[1-9][0-9]*\$)?[-+ #0']*(?:\*|[0-9]+)?(?:\.(?:\*|[0-9]+))?(?:hh|h|ll|l|j|z|t|L)?[diuoxXfFeEgGaAcspn])");
		std::vector<std::string> tokens;
		for (std::sregex_iterator it(a_text.begin(), a_text.end(), printfToken), end; it != end; ++it) {
			tokens.push_back(it->str());
		}

		for (size_t i = 0; i < a_text.size(); ++i) {
			if (a_text[i] != '{') continue;
			if (i + 1 < a_text.size() && a_text[i + 1] == '{') {
				++i;
				continue;
			}
			const auto close = a_text.find('}', i + 1);
			if (close == std::string::npos) break;
			tokens.push_back(a_text.substr(i, close - i + 1));
			i = close;
		}
		return tokens;
	}

	bool HasValidFormatBraces(const std::string& a_text)
	{
		for (size_t i = 0; i < a_text.size(); ++i) {
			if (a_text[i] == '{') {
				if (i + 1 < a_text.size() && a_text[i + 1] == '{') {
					++i;
					continue;
				}
				const auto close = a_text.find('}', i + 1);
				if (close == std::string::npos || a_text.find('{', i + 1) < close) return false;
				i = close;
			} else if (a_text[i] == '}') {
				if (i + 1 < a_text.size() && a_text[i + 1] == '}') {
					++i;
					continue;
				}
				return false;
			}
		}
		return true;
	}

	bool ValidateAndNormalizeTranslation(const std::string& a_source, std::string& a_translation)
	{
		if (a_source.starts_with("##")) {
			return false;
		}

		if (!HasValidFormatBraces(a_translation) ||
			ExtractFormatTokens(a_source) != ExtractFormatTokens(a_translation)) {
			logger::warn("[OAR] Ignoring translation with incompatible format tokens: '{}'", a_source);
			return false;
		}

		const auto idPos = a_source.find("##");
		if (idPos != std::string::npos) {
			if (const auto translatedID = a_translation.find("##"); translatedID != std::string::npos) {
				a_translation.erase(translatedID);
			}
			a_translation.append(a_source.substr(idPos));
		}
		return true;
	}

    bool LoadTranslationFile(const std::filesystem::path& a_path, bool a_required, std::size_t& a_loaded)
    {
        std::ifstream file(a_path, std::ios::binary);
        if (!file) {
            if (a_required) {
                logger::warn("[OAR] UI language file not found: '{}' - using English fallback", a_path.string());
            }
            return false;
        }

        try {
            const auto json = nlohmann::json::parse(file);
            if (!json.is_object()) {
                logger::warn("[OAR] UI language file is not an object: '{}'", a_path.string());
                return false;
            }

            for (const auto& [key, value] : json.items()) {
                if (value.is_string()) {
                    auto translation = value.get<std::string>();
                    if (ValidateAndNormalizeTranslation(key, translation)) {
                        // Extension packs are loaded after the base file. A
                        // later pack may intentionally override a translation
                        // without changing any runtime condition identifier.
                        g_translations[key] = std::move(translation);
                        ++a_loaded;
                    }
                }
            }
            return true;
        } catch (const std::exception& e) {
            logger::error("[OAR] Failed to parse UI language file '{}': {}", a_path.string(), e.what());
            return false;
        }
    }

    std::vector<std::filesystem::path> GetTranslationExtensionFiles(const std::filesystem::path& a_localeDirectory,
        const std::string& a_language)
    {
        std::vector<std::filesystem::path> files;
        std::error_code error;
        const auto directory = a_localeDirectory / (a_language + ".d");
        if (!std::filesystem::is_directory(directory, error)) {
            return files;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
            if (error) break;
            if (entry.is_regular_file(error) && entry.path().extension() == ".json") {
                files.push_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
            return left.filename().string() < right.filename().string();
        });
        return files;
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

        std::size_t loaded = 0;
        bool baseLoaded = false;
        const auto localeDirectories = GetLocaleDirectories();
        for (std::size_t i = 0; i < localeDirectories.size(); ++i) {
            const auto candidate = localeDirectories[i] / (g_language + ".json");
            if (LoadTranslationFile(candidate, i + 1 == localeDirectories.size(), loaded)) {
                baseLoaded = true;
                break;
            }
        }
        if (!baseLoaded) {
            g_translations.clear();
            return;
        }

        std::unordered_set<std::string> loadedExtensionNames;
        std::size_t extensionPackCount = 0;
        for (const auto& localeDirectory : localeDirectories) {
            const auto extensionFiles = GetTranslationExtensionFiles(localeDirectory, g_language);
            for (const auto& extensionFile : extensionFiles) {
                // Pack filenames are the stable identity of an extension
                // pack. This prevents loading OAR's own file twice when the
                // module directory is also visible through the MO2 Data path.
                if (!loadedExtensionNames.insert(extensionFile.filename().string()).second) {
                    continue;
                }

                const auto before = loaded;
                if (LoadTranslationFile(extensionFile, false, loaded)) {
                    ++extensionPackCount;
                    logger::info("[OAR] UI language extension '{}' loaded: {} translations",
                        extensionFile.filename().string(), loaded - before);
                }
            }
        }

        logger::info("[OAR] UI language '{}' loaded: {} translations ({} extension packs)",
            g_language, g_translations.size(), extensionPackCount);
    }

    const char* T(const char* a_source)
    {
        if (!a_source || g_translations.empty()) {
            return a_source;
        }

        const auto it = g_translations.find(a_source);
        if (it != g_translations.end()) {
            return it->second.c_str();
        }

        // Keep ImGui IDs untranslated while still translating the visible
        // label. This lets a locale contain one `Plugin` entry that covers
        // Plugin##kw, Plugin##manual, and similar stable-ID labels.
        const std::string_view source(a_source);
        const auto idPos = source.find("##");
        if (idPos != std::string_view::npos) {
            const auto visible = g_translations.find(std::string(source.substr(0, idPos)));
            if (visible != g_translations.end()) {
                thread_local std::string localizedLabel;
                localizedLabel = visible->second;
                localizedLabel.append(source.substr(idPos));
                return localizedLabel.c_str();
            }
        }

        return a_source;
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
