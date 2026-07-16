#pragma once

#include "BaseConditions.h"
#include "Conditions.h"
#include <imgui.h>

namespace UICommon
{
	enum class EditorMode : int32_t
	{
		kInspect = 0,
		kUser,
		kAuthor,
	};

	namespace Colors
	{
		inline constexpr ImVec4 Success       { 0.2f, 0.85f, 0.2f, 1.0f };
		inline constexpr ImVec4 Failure       { 0.85f, 0.2f, 0.2f, 1.0f };
		inline constexpr ImVec4 Warning       { 0.9f, 0.7f, 0.1f, 1.0f };
		inline constexpr ImVec4 Unknown       { 0.7f, 0.7f, 0.2f, 1.0f };
		inline constexpr ImVec4 Disabled      { 0.5f, 0.5f, 0.5f, 1.0f };
		inline constexpr ImVec4 Dirty         { 0.9f, 0.6f, 0.1f, 1.0f };
		inline constexpr ImVec4 UserConfig    { 0.9f, 0.85f, 0.3f, 1.0f };
		inline constexpr ImVec4 Invalid       { 0.9f, 0.15f, 0.15f, 1.0f };
		inline constexpr ImVec4 CondAND       { 0.4f, 0.7f, 1.0f, 1.0f };
		inline constexpr ImVec4 CondOR        { 0.9f, 0.5f, 0.3f, 1.0f };
		inline constexpr ImVec4 CondNegated   { 0.8f, 0.3f, 0.8f, 1.0f };
		inline constexpr ImVec4 LogActivate   { 0.3f, 0.85f, 0.3f, 1.0f };
		inline constexpr ImVec4 LogReplace    { 0.9f, 0.8f, 0.2f, 1.0f };
		inline constexpr ImVec4 LogLoop       { 0.3f, 0.5f, 0.95f, 1.0f };
		inline constexpr ImVec4 LogEcho       { 0.2f, 0.8f, 0.8f, 1.0f };
		inline constexpr ImVec4 LogInterrupt  { 0.9f, 0.5f, 0.15f, 1.0f };
		inline constexpr ImVec4 LogEvent      { 0.6f, 0.6f, 0.6f, 1.0f };
		inline constexpr ImVec4 TimingShort   { 0.3f, 0.85f, 0.3f, 1.0f };
		inline constexpr ImVec4 TimingMedium  { 0.9f, 0.75f, 0.2f, 1.0f };
		inline constexpr ImVec4 TimingLong    { 0.9f, 0.25f, 0.2f, 1.0f };
		inline constexpr ImVec4 AccentBlue    { 0.26f, 0.59f, 0.98f, 1.0f };
		inline constexpr ImVec4 HeaderBg      { 0.15f, 0.15f, 0.18f, 1.0f };
		inline constexpr ImVec4 ChildBg       { 0.12f, 0.12f, 0.14f, 1.0f };
		inline constexpr ImVec4 TreeLine      { 0.35f, 0.35f, 0.4f, 0.6f };
		inline constexpr ImVec4 Separator     { 0.3f, 0.3f, 0.35f, 1.0f };
		inline constexpr ImVec4 FilterBg      { 0.18f, 0.18f, 0.22f, 1.0f };
		inline constexpr ImVec4 BottomBar     { 0.1f, 0.1f, 0.12f, 1.0f };
	}

	void DrawComparisonOperatorCombo(const char* a_label, ComparisonOperator& a_op);
	void HelpMarker(const char* a_desc);
	bool DrawFormPicker(const char* a_label, FormComponent& a_form);

	void TextUnformattedColored(const ImVec4& a_color, const char* a_text);
	void TextUnformattedDisabled(const char* a_text);

	void DrawConditionEvalResult(bool a_result, bool a_hasValue);
	void DrawWarningIcon(const char* a_tooltip);

	bool ButtonWithConfirmationModal(const char* a_buttonLabel, const char* a_modalTitle, const char* a_message);

	void DrawTreeLine(float a_indentX, float a_topY, float a_bottomY);

	float FirstColumnWidth(float a_windowWidth, float a_percent = 0.55f);

	void ApplyOARStyle();

	bool FuzzyMatch(const char* a_pattern, const char* a_str);

	std::string ShortenAnimPath(const std::string& a_path);

	// DirectInput (DIK) scan-code helpers used by the toggle hotkey UI.
	const char* DIKCodeToName(std::uint32_t a_dik);
}
