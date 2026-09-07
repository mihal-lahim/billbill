#pragma once

#include "Game.h"
#include "Settings.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace townmaker
{
    enum class UiPage
    {
        Game,
        Settings,
        HowToPlay,
        Credits
    };

    struct CardView
    {
        RECT rect{};
        int value = 0;
        PlayerId owner = PlayerId::Player1;
    };

    struct UiLayout
    {
        RECT player1Panel{};
        RECT player2Panel{};
        RECT currentBuildingCard{};
        RECT activeHandArea{};
        RECT unsoldArea{};
        RECT confirmButton{};
    };

    class GameRenderer
    {
    public:
        GameRenderer();
        ~GameRenderer();

        GameRenderer(const GameRenderer&) = delete;
        GameRenderer& operator=(const GameRenderer&) = delete;

        void Draw(HDC deviceContext, const RECT& clientRect, const Game& game,
            UiPage page, const Settings& settings);

        std::optional<int> HitTestMoneyCard(POINT point, PlayerId owner) const noexcept;
        bool HitTestPrimaryButton(POINT point) const noexcept;
        bool HitTestSecondaryButton(POINT point) const noexcept;
        bool HitTestExitButton(POINT point) const noexcept;
        std::optional<GameMode> HitTestGameMode(POINT point) const noexcept;
        std::optional<CpuDifficulty> HitTestCpuDifficulty(POINT point) const noexcept;
        bool HitTestSettingsButton(POINT point) const noexcept;
        bool HitTestHowToPlayButton(POINT point) const noexcept;
        bool HitTestCreditsButton(POINT point) const noexcept;
        bool HitTestFeedbackButton(POINT point) const noexcept;
        bool HitTestBackButton(POINT point) const noexcept;
        bool HitTestMuteButton(POINT point) const noexcept;
        bool HitTestRestartButton(POINT point) const noexcept;
        std::optional<int> HitTestVolumeSlider(POINT point) const noexcept;

        bool UpdateHover(POINT point) noexcept;
        bool ClearHover() noexcept;

    private:
        enum class ButtonTone
        {
            Sky,
            Yellow,
            Coral,
            Navy,
            White
        };

        static UiLayout CreateGameLayout(const RECT& clientRect) noexcept;

        void CreateFonts();
        void DeleteFonts() noexcept;
        void ResetHitRegions() noexcept;

        void DrawTitleScreen(HDC deviceContext, const RECT& clientRect);
        void DrawDifficultySelectionScreen(HDC deviceContext, const RECT& clientRect);
        void DrawGameScreen(HDC deviceContext, const RECT& clientRect, const Game& game);
        void DrawRoundResultScreen(HDC deviceContext, const RECT& clientRect, const Game& game);
        void DrawFinalResultScreen(HDC deviceContext, const RECT& clientRect, const Game& game);
        void DrawSettingsScreen(HDC deviceContext, const RECT& clientRect, const Settings& settings);
        void DrawHowToPlayScreen(HDC deviceContext, const RECT& clientRect);
        void DrawCreditsScreen(HDC deviceContext, const RECT& clientRect);

        void DrawBackground(HDC deviceContext, const RECT& clientRect, bool titleScreen) const;
        void DrawLogo(HDC deviceContext, const RECT& area, bool large) const;
        void DrawHeader(HDC deviceContext, const RECT& clientRect, const std::wstring& status) const;
        void DrawButton(
            HDC deviceContext,
            const RECT& rect,
            const std::wstring& text,
            ButtonTone tone,
            bool enabled = true) const;
        void DrawMoneyCard(
            HDC deviceContext,
            const RECT& rect,
            int value,
            PlayerId owner,
            bool selected,
            bool hovered,
            bool compact = false) const;
        void DrawBuildingCard(HDC deviceContext, const RECT& rect, int value) const;
        void DrawBuilding(
            HDC deviceContext,
            const RECT& rect,
            int value,
            COLORREF bodyColor) const;
        void DrawPlayerCity(
            HDC deviceContext,
            const RECT& panel,
            const Player& player,
            const std::wstring& name,
            PlayerId owner,
            bool highlighted,
            bool large = false) const;
        void DrawMiniHand(
            HDC deviceContext,
            const RECT& area,
            const Player& player,
            PlayerId owner) const;
        void DrawActiveHand(
            HDC deviceContext,
            const RECT& area,
            const Player& player,
            PlayerId owner);
        void DrawUnsoldArea(
            HDC deviceContext,
            const RECT& area,
            const std::vector<int>& buildings) const;
        void DrawDecorativeSkyline(HDC deviceContext, const RECT& clientRect) const;

        void DrawLabel(
            HDC deviceContext,
            const std::wstring& text,
            RECT area,
            HFONT font,
            COLORREF color,
            UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) const;
        bool IsHovered(const RECT& rect) const noexcept;
        int HoverRegionAt(POINT point) const noexcept;

        HFONT logoFont_ = nullptr;
        HFONT logoEnglishFont_ = nullptr;
        HFONT titleFont_ = nullptr;
        HFONT headingFont_ = nullptr;
        HFONT bodyFont_ = nullptr;
        HFONT smallFont_ = nullptr;
        HFONT cardNumberFont_ = nullptr;
        HFONT buildingNumberFont_ = nullptr;

        std::optional<POINT> mousePosition_;
        std::vector<CardView> moneyCardViews_;
        RECT primaryButtonRect_{};
        RECT secondaryButtonRect_{};
        RECT exitButtonRect_{};
        RECT playerVsPlayerButtonRect_{};
        RECT playerVsCpuButtonRect_{};
        std::array<RECT, 3> difficultyButtonRects_{};
        RECT settingsButtonRect_{};
        RECT howToPlayButtonRect_{};
        RECT creditsButtonRect_{};
        RECT feedbackButtonRect_{};
        RECT backButtonRect_{};
        RECT muteButtonRect_{};
        RECT restartButtonRect_{};
        RECT volumeSliderRect_{};
    };
}
