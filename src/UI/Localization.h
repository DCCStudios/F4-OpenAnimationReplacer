#pragma once

namespace UICommon
{
    // Loads the language selected in Settings and falls back to the source
    // string when a translation is missing.
    void LoadLocalization();
    const char* T(const char* a_source);

    // Adds a Chinese-capable ImGui font when a system font is available.
    // The UI remains usable with the built-in font if no CJK font is found.
    bool LoadCJKFont();
}
