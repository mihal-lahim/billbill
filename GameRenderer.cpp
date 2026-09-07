#include "GameRenderer.h"
#include "AppConfig.h"

#include <algorithm>
#include <numeric>
#include <sstream>

namespace townmaker
{
    namespace
    {
        constexpr COLORREF kSky = RGB(103, 202, 238);
        constexpr COLORREF kSkyLight = RGB(213, 244, 252);
        constexpr COLORREF kCream = RGB(255, 249, 230);
        constexpr COLORREF kYellow = RGB(255, 211, 76);
        constexpr COLORREF kYellowLight = RGB(255, 239, 166);
        constexpr COLORREF kCoral = RGB(255, 111, 97);
        constexpr COLORREF kCoralLight = RGB(255, 155, 143);
        constexpr COLORREF kNavy = RGB(35, 53, 78);
        constexpr COLORREF kDark = RGB(50, 57, 70);
        constexpr COLORREF kWhite = RGB(255, 255, 255);
        constexpr COLORREF kGray = RGB(164, 174, 187);
        constexpr COLORREF kGrayLight = RGB(230, 235, 239);
        constexpr COLORREF kGreen = RGB(88, 190, 144);
        constexpr COLORREF kPlayer1 = RGB(64, 164, 224);
        constexpr COLORREF kPlayer1Light = RGB(218, 242, 253);
        constexpr COLORREF kPlayer2 = RGB(255, 126, 111);
        constexpr COLORREF kPlayer2Light = RGB(255, 229, 223);
        constexpr int kHeaderHeight = 64;

        int Width(const RECT& rect) noexcept
        {
            return rect.right - rect.left;
        }

        int Height(const RECT& rect) noexcept
        {
            return rect.bottom - rect.top;
        }

        RECT OffsetRectCopy(RECT rect, int x, int y) noexcept
        {
            OffsetRect(&rect, x, y);
            return rect;
        }

        RECT InsetRectCopy(RECT rect, int amount) noexcept
        {
            InflateRect(&rect, -amount, -amount);
            return rect;
        }

        void FillSolid(HDC dc, const RECT& rect, COLORREF color)
        {
            const HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        }

        void FillRounded(HDC dc, const RECT& rect, COLORREF fill, COLORREF outline, int radius, int penWidth = 1)
        {
            const HBRUSH brush = CreateSolidBrush(fill);
            const HPEN pen = CreatePen(PS_SOLID, penWidth, outline);
            const HGDIOBJ oldBrush = SelectObject(dc, brush);
            const HGDIOBJ oldPen = SelectObject(dc, pen);
            RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
            DeleteObject(brush);
        }

        void DrawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1)
        {
            const HPEN pen = CreatePen(PS_SOLID, width, color);
            const HGDIOBJ oldPen = SelectObject(dc, pen);
            MoveToEx(dc, x1, y1, nullptr);
            LineTo(dc, x2, y2);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }

        HFONT CreateUiFont(int height, int weight, const wchar_t* face = L"Yu Gothic UI")
        {
            return CreateFontW(
                -height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
        }

        int Sum(const std::vector<int>& values) noexcept
        {
            return std::accumulate(values.begin(), values.end(), 0);
        }

        std::wstring CompactValues(const std::vector<int>& values)
        {
            if (values.empty())
            {
                return L"--";
            }

            std::wostringstream stream;
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    stream << L' ';
                }
                stream << values[index];
            }
            return stream.str();
        }

        std::wstring Player2Name(const Game& game)
        {
            if (!game.IsCpuGame())
            {
                return L"PLAYER 2";
            }

            switch (game.Difficulty())
            {
            case CpuDifficulty::Easy:
                return L"CPU - EASY";
            case CpuDifficulty::Normal:
                return L"CPU - NORMAL";
            case CpuDifficulty::Hard:
                return L"CPU - HARD";
            }
            return L"CPU";
        }

        std::wstring ModeText(const Game& game)
        {
            return game.IsCpuGame() ? Player2Name(game) : L"PLAYER VS PLAYER";
        }

        std::wstring WinnerName(CategoryWinner winner, const Game& game)
        {
            switch (winner)
            {
            case CategoryWinner::Player1:
                return L"PLAYER 1";
            case CategoryWinner::Player2:
                return game.IsCpuGame() ? L"CPU" : L"PLAYER 2";
            default:
                return L"DRAW";
            }
        }

        std::wstring RoundWinnerMessage(const RoundResult& result, const Game& game)
        {
            if (result.winner == RoundWinner::None)
            {
                return result.player1Bid == result.player2Bid ? L"DRAW BID!  /  NO SALE" : L"NO SALE!";
            }
            if (result.winner == RoundWinner::Player1)
            {
                return L"PLAYER 1 GETS THE BUILDING!";
            }
            return game.IsCpuGame() ? L"CPU GETS THE BUILDING!" : L"PLAYER 2 GETS THE BUILDING!";
        }

        COLORREF OwnerColor(PlayerId owner) noexcept
        {
            return owner == PlayerId::Player1 ? kPlayer1 : kPlayer2;
        }

        COLORREF OwnerLightColor(PlayerId owner) noexcept
        {
            return owner == PlayerId::Player1 ? kPlayer1Light : kPlayer2Light;
        }
    }

    GameRenderer::GameRenderer()
    {
        CreateFonts();
    }

    GameRenderer::~GameRenderer()
    {
        DeleteFonts();
    }

    void GameRenderer::CreateFonts()
    {
        logoFont_ = CreateUiFont(70, FW_BLACK);
        logoEnglishFont_ = CreateUiFont(24, FW_BOLD, L"Arial Rounded MT Bold");
        titleFont_ = CreateUiFont(30, FW_BLACK);
        headingFont_ = CreateUiFont(20, FW_BOLD);
        bodyFont_ = CreateUiFont(16, FW_NORMAL);
        smallFont_ = CreateUiFont(12, FW_BOLD);
        cardNumberFont_ = CreateUiFont(31, FW_BLACK, L"Arial");
        buildingNumberFont_ = CreateUiFont(38, FW_BLACK, L"Arial");
    }

    void GameRenderer::DeleteFonts() noexcept
    {
        const std::array<HFONT, 8> fonts{
            logoFont_, logoEnglishFont_, titleFont_, headingFont_, bodyFont_, smallFont_,
            cardNumberFont_, buildingNumberFont_
        };
        for (const HFONT font : fonts)
        {
            if (font != nullptr)
            {
                DeleteObject(font);
            }
        }
    }

    UiLayout GameRenderer::CreateGameLayout(const RECT& clientRect) noexcept
    {
        UiLayout layout;
        layout.player1Panel = { 20, 82, 425, 340 };
        layout.player2Panel = { clientRect.right - 425, 82, clientRect.right - 20, 340 };
        layout.currentBuildingCard = { clientRect.right / 2 - 98, 82, clientRect.right / 2 + 98, 306 };
        layout.unsoldArea = { clientRect.right / 2 - 190, 316, clientRect.right / 2 + 190, 349 };
        layout.activeHandArea = { 270, 398, clientRect.right - 270, 572 };
        layout.confirmButton = { clientRect.right / 2 - 135, 625, clientRect.right / 2 + 135, 684 };
        return layout;
    }

    void GameRenderer::ResetHitRegions() noexcept
    {
        moneyCardViews_.clear();
        primaryButtonRect_ = {};
        secondaryButtonRect_ = {};
        exitButtonRect_ = {};
        playerVsPlayerButtonRect_ = {};
        playerVsCpuButtonRect_ = {};
        difficultyButtonRects_ = {};
        settingsButtonRect_ = {};
        howToPlayButtonRect_ = {};
        creditsButtonRect_ = {};
        feedbackButtonRect_ = {};
        backButtonRect_ = {};
        muteButtonRect_ = {};
        restartButtonRect_ = {};
        volumeSliderRect_ = {};
    }

    void GameRenderer::Draw(HDC dc, const RECT& clientRect, const Game& game,
        UiPage page, const Settings& settings)
    {
        ResetHitRegions();
        if (page != UiPage::Game)
        {
            DrawBackground(dc, clientRect, false);
            switch (page)
            {
            case UiPage::Settings: DrawSettingsScreen(dc, clientRect, settings); break;
            case UiPage::HowToPlay: DrawHowToPlayScreen(dc, clientRect); break;
            case UiPage::Credits: DrawCreditsScreen(dc, clientRect); break;
            default: break;
            }
            return;
        }
        DrawBackground(dc, clientRect, game.State() == GameState::ModeSelection);

        switch (game.State())
        {
        case GameState::ModeSelection:
            DrawTitleScreen(dc, clientRect);
            break;
        case GameState::CpuDifficultySelection:
            DrawDifficultySelectionScreen(dc, clientRect);
            break;
        case GameState::Player1Selecting:
        case GameState::Player2Selecting:
            DrawGameScreen(dc, clientRect, game);
            break;
        case GameState::RoundResult:
            DrawRoundResultScreen(dc, clientRect, game);
            break;
        case GameState::GameResult:
            DrawFinalResultScreen(dc, clientRect, game);
            break;
        }
    }

    void GameRenderer::DrawBackground(HDC dc, const RECT& clientRect, bool titleScreen) const
    {
        FillSolid(dc, clientRect, titleScreen ? kSkyLight : kCream);
        if (!titleScreen)
        {
            const RECT skyBand{ 0, kHeaderHeight, clientRect.right, 355 };
            FillSolid(dc, skyBand, kSkyLight);
            const RECT groundBand{ 0, 355, clientRect.right, clientRect.bottom };
            FillSolid(dc, groundBand, kCream);
        }
    }

    void GameRenderer::DrawLabel(
        HDC dc,
        const std::wstring& text,
        RECT area,
        HFONT font,
        COLORREF color,
        UINT format) const
    {
        const HGDIOBJ oldFont = SelectObject(dc, font != nullptr ? font : GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        DrawTextW(dc, text.c_str(), -1, &area, format);
        SelectObject(dc, oldFont);
    }

    void GameRenderer::DrawLogo(HDC dc, const RECT& area, bool large) const
    {
        RECT japaneseArea = area;
        japaneseArea.bottom = large ? area.top + 90 : area.bottom;
        RECT outlineArea = OffsetRectCopy(japaneseArea, large ? 4 : 2, large ? 5 : 2);
        DrawLabel(dc, L"ビルビル", outlineArea, large ? logoFont_ : titleFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, L"ビルビル", japaneseArea, large ? logoFont_ : titleFont_,
            large ? kYellow : kWhite, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (large)
        {
            RECT englishArea{ area.left, area.top + 83, area.right, area.top + 125 };
            DrawLabel(dc, L"BILL BILL", englishArea, logoEnglishFont_, kNavy,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    void GameRenderer::DrawHeader(HDC dc, const RECT& clientRect, const std::wstring& status) const
    {
        const RECT header{ 0, 0, clientRect.right, kHeaderHeight };
        FillSolid(dc, header, kNavy);
        DrawLogo(dc, { 14, 5, 238, 59 }, false);
        DrawLabel(dc, status, { 270, 7, clientRect.right - 24, 57 }, headingFont_, kWhite,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    void GameRenderer::DrawDecorativeSkyline(HDC dc, const RECT& clientRect) const
    {
        const RECT grass{ 0, 575, clientRect.right, clientRect.bottom };
        FillSolid(dc, grass, RGB(143, 211, 138));
        const RECT road{ 0, 670, clientRect.right, clientRect.bottom };
        FillSolid(dc, road, RGB(105, 116, 128));
        DrawLine(dc, 0, 689, clientRect.right, 689, kYellow, 3);

        const std::array<int, 18> values{ 4, 8, 12, 6, 16, 10, 20, 8, 14, 4, 18, 12, 6, 16, 10, 20, 8, 14 };
        int x = 8;
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const int width = 48 + static_cast<int>(index % 3) * 7;
            const int height = 30 + values[index] * 3;
            const RECT building{ x, 670 - height, x + width, 670 };
            const COLORREF color = index % 3 == 0 ? kCoral : index % 3 == 1 ? kYellow : kPlayer1;
            DrawBuilding(dc, building, values[index], color);
            x += width + 15;
        }

        const HBRUSH cloudBrush = CreateSolidBrush(kWhite);
        const HGDIOBJ oldBrush = SelectObject(dc, cloudBrush);
        const HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, 95, 105, 205, 155);
        Ellipse(dc, 150, 84, 270, 157);
        Ellipse(dc, 220, 110, 330, 158);
        Ellipse(dc, 960, 118, 1060, 162);
        Ellipse(dc, 1010, 92, 1135, 164);
        Ellipse(dc, 1090, 116, 1190, 165);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(cloudBrush);
    }

    void GameRenderer::DrawTitleScreen(HDC dc, const RECT& clientRect)
    {
        DrawDecorativeSkyline(dc, clientRect);
        DrawLogo(dc, { 330, 42, clientRect.right - 330, 185 }, true);
        DrawLabel(dc, L"街を競って、ビル街をつくろう！", { 300, 178, clientRect.right - 300, 220 },
            headingFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        playerVsCpuButtonRect_ = { clientRect.right / 2 - 220, 220, clientRect.right / 2 + 220, 273 };
        playerVsPlayerButtonRect_ = { clientRect.right / 2 - 220, 286, clientRect.right / 2 + 220, 339 };
        settingsButtonRect_ = { clientRect.right / 2 - 220, 352, clientRect.right / 2 + 220, 405 };
        howToPlayButtonRect_ = { 310, 420, 520, 469 };
        creditsButtonRect_ = { 535, 420, 745, 469 };
        feedbackButtonRect_ = { 760, 420, 970, 469 };
        exitButtonRect_ = { clientRect.right / 2 - 120, 484, clientRect.right / 2 + 120, 530 };
        DrawButton(dc, playerVsCpuButtonRect_, L"PLAYER VS CPU", ButtonTone::Coral);
        DrawButton(dc, playerVsPlayerButtonRect_, L"PLAYER VS PLAYER", ButtonTone::Sky);
        DrawButton(dc, settingsButtonRect_, L"SETTINGS", ButtonTone::Yellow);
        DrawButton(dc, howToPlayButtonRect_, L"HOW TO PLAY", ButtonTone::White);
        DrawButton(dc, creditsButtonRect_, L"CREDITS", ButtonTone::White);
        DrawButton(dc, feedbackButtonRect_, L"FEEDBACK", ButtonTone::White);
        DrawButton(dc, exitButtonRect_, L"EXIT", ButtonTone::White);
        DrawLabel(dc, L"10ラウンドで完成する、気軽な不動産オークション", { 250, 532, clientRect.right - 250, 568 },
            bodyFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void GameRenderer::DrawDifficultySelectionScreen(HDC dc, const RECT& clientRect)
    {
        DrawHeader(dc, clientRect, L"PLAYER VS CPU");
        DrawLabel(dc, L"CPU LEVEL", { 100, 95, clientRect.right - 100, 150 }, titleFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, L"対戦するCPUの強さを選んでください", { 100, 145, clientRect.right - 100, 185 },
            bodyFont_, kDark, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        difficultyButtonRects_[0] = { 115, 235, 405, 335 };
        difficultyButtonRects_[1] = { 495, 235, 785, 335 };
        difficultyButtonRects_[2] = { 875, 235, 1165, 335 };
        DrawButton(dc, difficultyButtonRects_[0], L"EASY", ButtonTone::Sky);
        DrawButton(dc, difficultyButtonRects_[1], L"NORMAL", ButtonTone::Yellow);
        DrawButton(dc, difficultyButtonRects_[2], L"HARD", ButtonTone::Coral);
        DrawLabel(dc, L"気ままなランダム入札", { 115, 350, 405, 385 }, bodyFont_, kDark,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, L"価格を見てバランス入札", { 495, 350, 785, 385 }, bodyFont_, kDark,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, L"公開情報を読む戦略入札", { 875, 350, 1165, 385 }, bodyFont_, kDark,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        DrawLabel(dc, L"CPUはあなたが今回選んだカードを見ることはできません", { 260, 445, clientRect.right - 260, 490 },
            bodyFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        secondaryButtonRect_ = { 35, clientRect.bottom - 78, 205, clientRect.bottom - 25 };
        DrawButton(dc, secondaryButtonRect_, L"BACK", ButtonTone::White);
    }

    void GameRenderer::DrawButton(
        HDC dc,
        const RECT& rect,
        const std::wstring& text,
        ButtonTone tone,
        bool enabled) const
    {
        COLORREF fill = kSky;
        COLORREF outline = kNavy;
        COLORREF textColor = kWhite;
        switch (tone)
        {
        case ButtonTone::Yellow:
            fill = kYellow;
            textColor = kNavy;
            break;
        case ButtonTone::Coral:
            fill = kCoral;
            break;
        case ButtonTone::Navy:
            fill = kNavy;
            outline = kNavy;
            break;
        case ButtonTone::White:
            fill = kWhite;
            textColor = kNavy;
            break;
        case ButtonTone::Sky:
            break;
        }
        if (!enabled)
        {
            fill = kGrayLight;
            outline = kGray;
            textColor = kGray;
        }

        RECT visual = rect;
        const bool hovered = enabled && IsHovered(rect);
        if (hovered)
        {
            OffsetRect(&visual, 0, -3);
            if (tone == ButtonTone::Coral)
            {
                fill = kCoralLight;
            }
            else if (tone == ButtonTone::Sky)
            {
                fill = RGB(93, 190, 238);
            }
            else if (tone == ButtonTone::Yellow)
            {
                fill = kYellowLight;
            }
        }

        const RECT shadow = OffsetRectCopy(visual, 0, 5);
        FillRounded(dc, shadow, RGB(190, 194, 194), RGB(190, 194, 194), 18);
        FillRounded(dc, visual, fill, outline, 18, 2);
        DrawLabel(dc, text, visual, headingFont_, textColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void GameRenderer::DrawMoneyCard(
        HDC dc,
        const RECT& sourceRect,
        int value,
        PlayerId owner,
        bool selected,
        bool hovered,
        bool compact) const
    {
        RECT rect = sourceRect;
        if (!compact && (selected || hovered))
        {
            OffsetRect(&rect, 0, selected ? -11 : -5);
        }
        const RECT shadow = OffsetRectCopy(rect, 0, compact ? 2 : 4);
        FillRounded(dc, shadow, RGB(199, 204, 207), RGB(199, 204, 207), compact ? 7 : 12);
        FillRounded(dc, rect, selected ? kYellowLight : kWhite,
            selected ? kYellow : OwnerColor(owner), compact ? 7 : 12, selected ? 4 : 2);

        if (compact)
        {
            DrawLabel(dc, std::to_wstring(value), rect, smallFont_, kNavy,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        DrawLabel(dc, L"$", { rect.left + 5, rect.top + 4, rect.right - 5, rect.top + 23 },
            smallFont_, OwnerColor(owner), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, std::to_wstring(value), { rect.left + 2, rect.top + 17, rect.right - 2, rect.bottom - 5 },
            cardNumberFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (selected)
        {
            const RECT marker{ rect.right - 18, rect.top - 8, rect.right + 5, rect.top + 15 };
            FillRounded(dc, marker, kCoral, kWhite, 16, 2);
            DrawLabel(dc, L"✓", marker, smallFont_, kWhite, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    void GameRenderer::DrawBuilding(HDC dc, const RECT& rect, int value, COLORREF bodyColor) const
    {
        if (Width(rect) <= 2 || Height(rect) <= 2)
        {
            return;
        }

        FillSolid(dc, rect, bodyColor);
        const int floors = std::clamp((value + 1) / 2, 1, 10);
        const int columns = value >= 14 ? 3 : value >= 6 ? 2 : 1;
        const int marginX = std::max(3, Width(rect) / 10);
        const int marginY = std::max(3, Height(rect) / 15);
        const int gap = std::max(2, Width(rect) / 18);
        const int windowWidth = std::max(2, (Width(rect) - marginX * 2 - gap * (columns - 1)) / columns);
        const int windowHeight = std::max(2, (Height(rect) - marginY * (floors + 1)) / floors);

        for (int floor = 0; floor < floors; ++floor)
        {
            for (int column = 0; column < columns; ++column)
            {
                const int left = rect.left + marginX + column * (windowWidth + gap);
                const int top = rect.bottom - marginY - (floor + 1) * (windowHeight + marginY);
                const RECT window{ left, top, left + windowWidth, top + windowHeight };
                FillSolid(dc, window, floor % 3 == 0 ? kYellowLight : kSkyLight);
            }
        }

        if (value >= 14)
        {
            const RECT roof{ rect.left + Width(rect) / 4, rect.top - 6, rect.right - Width(rect) / 4, rect.top };
            FillSolid(dc, roof, kNavy);
        }
        if (value >= 18)
        {
            DrawLine(dc, (rect.left + rect.right) / 2, rect.top - 18,
                (rect.left + rect.right) / 2, rect.top - 5, kNavy, 2);
        }
        FillSolid(dc, { rect.left + Width(rect) / 2 - 3, rect.bottom - 12,
            rect.left + Width(rect) / 2 + 4, rect.bottom }, kNavy);
    }

    void GameRenderer::DrawBuildingCard(HDC dc, const RECT& rect, int value) const
    {
        const RECT shadow = OffsetRectCopy(rect, 0, 6);
        FillRounded(dc, shadow, RGB(196, 188, 161), RGB(196, 188, 161), 18);
        FillRounded(dc, rect, kYellowLight, kNavy, 18, 3);
        DrawLabel(dc, L"PROPERTY", { rect.left + 8, rect.top + 5, rect.right - 8, rect.top + 33 },
            smallFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const int maxBuildingHeight = std::max(35, Height(rect) - 98);
        const int buildingHeight = 28 + value * (maxBuildingHeight - 28) / 20;
        const int buildingWidth = 45 + value * 2;
        const RECT building{
            (rect.left + rect.right - buildingWidth) / 2,
            rect.bottom - 52 - buildingHeight,
            (rect.left + rect.right + buildingWidth) / 2,
            rect.bottom - 52
        };
        const COLORREF buildingColor = value >= 16 ? kCoral : value >= 10 ? kPlayer1 : kGreen;
        DrawBuilding(dc, building, value, buildingColor);
        DrawLabel(dc, std::to_wstring(value), { rect.left + 8, rect.bottom - 55, rect.right - 8, rect.bottom - 7 },
            buildingNumberFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void GameRenderer::DrawMiniHand(
        HDC dc,
        const RECT& area,
        const Player& player,
        PlayerId owner) const
    {
        const int count = static_cast<int>(player.MoneyCards().size());
        if (count == 0)
        {
            return;
        }
        const int gap = 3;
        const int cardWidth = std::min(27, (Width(area) - gap * (count - 1)) / count);
        const int totalWidth = cardWidth * count + gap * (count - 1);
        int x = area.left + (Width(area) - totalWidth) / 2;
        for (const int value : player.MoneyCards())
        {
            const RECT card{ x, area.top, x + cardWidth, area.bottom };
            DrawMoneyCard(dc, card, value, owner, false, false, true);
            x += cardWidth + gap;
        }
    }

    void GameRenderer::DrawPlayerCity(
        HDC dc,
        const RECT& panel,
        const Player& player,
        const std::wstring& name,
        PlayerId owner,
        bool highlighted,
        bool large) const
    {
        const RECT shadow = OffsetRectCopy(panel, 0, 5);
        FillRounded(dc, shadow, RGB(207, 207, 196), RGB(207, 207, 196), 16);
        FillRounded(dc, panel, kWhite, highlighted ? kYellow : OwnerColor(owner), 16, highlighted ? 4 : 2);
        const RECT ribbon{ panel.left + 12, panel.top + 10, panel.right - 12, panel.top + 42 };
        FillRounded(dc, ribbon, OwnerColor(owner), OwnerColor(owner), 12);
        DrawLabel(dc, name, ribbon, headingFont_, kWhite, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const std::wstring stats = L"BUILDINGS  " + std::to_wstring(player.Buildings().size()) +
            L"     VALUE  " + std::to_wstring(Sum(player.Buildings()));
        DrawLabel(dc, stats, { panel.left + 15, panel.top + 45, panel.right - 15, panel.top + 69 },
            smallFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT cityArea{ panel.left + 14, panel.top + 70, panel.right - 14, panel.bottom - (large ? 35 : 69) };
        FillSolid(dc, cityArea, kSkyLight);
        const RECT ground{ cityArea.left, cityArea.bottom - 11, cityArea.right, cityArea.bottom };
        FillSolid(dc, ground, RGB(127, 197, 123));
        DrawLine(dc, cityArea.left, cityArea.bottom - 11, cityArea.right, cityArea.bottom - 11, kNavy, 1);

        const int count = static_cast<int>(player.Buildings().size());
        if (count == 0)
        {
            DrawLabel(dc, L"EMPTY LOT", cityArea, smallFont_, kGray,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else
        {
            const int gap = large ? 6 : 4;
            const int availableWidth = Width(cityArea) - 12;
            const int buildingWidth = std::clamp(
                (availableWidth - gap * (count - 1)) / count,
                large ? 24 : 18,
                large ? 58 : 42);
            const int totalWidth = buildingWidth * count + gap * (count - 1);
            int x = cityArea.left + (Width(cityArea) - totalWidth) / 2;
            const int maxHeight = Height(cityArea) - 22;
            for (const int value : player.Buildings())
            {
                const int buildingHeight = 18 + value * (maxHeight - 18) / 20;
                const RECT building{ x, cityArea.bottom - 11 - buildingHeight,
                    x + buildingWidth, cityArea.bottom - 11 };
                DrawBuilding(dc, building, value, OwnerColor(owner));
                x += buildingWidth + gap;
            }
        }

        if (!large)
        {
            DrawLabel(dc, L"HAND", { panel.left + 14, panel.bottom - 66, panel.left + 58, panel.bottom - 42 },
                smallFont_, kNavy, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            DrawMiniHand(dc, { panel.left + 58, panel.bottom - 65, panel.right - 14, panel.bottom - 39 }, player, owner);
            const std::wstring used = L"USED  " + CompactValues(player.UsedMoneyCards());
            DrawLabel(dc, used, { panel.left + 14, panel.bottom - 36, panel.right - 14, panel.bottom - 10 },
                smallFont_, kGray, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    void GameRenderer::DrawActiveHand(
        HDC dc,
        const RECT& area,
        const Player& player,
        PlayerId owner)
    {
        const int count = static_cast<int>(player.MoneyCards().size());
        if (count == 0)
        {
            return;
        }
        constexpr int cardWidth = 50;
        constexpr int cardHeight = 82;
        constexpr int gap = 7;
        const int totalWidth = cardWidth * count + gap * (count - 1);
        int x = area.left + (Width(area) - totalWidth) / 2;
        const int top = area.top + 49;
        for (const int value : player.MoneyCards())
        {
            const RECT card{ x, top, x + cardWidth, top + cardHeight };
            const bool selected = player.SelectedMoneyCard() == value;
            const bool hovered = IsHovered(card);
            DrawMoneyCard(dc, card, value, owner, selected, hovered);
            moneyCardViews_.push_back(CardView{ card, value, owner });
            x += cardWidth + gap;
        }
    }

    void GameRenderer::DrawUnsoldArea(
        HDC dc,
        const RECT& area,
        const std::vector<int>& buildings) const
    {
        FillRounded(dc, area, kWhite, kGrayLight, 12);
        DrawLabel(dc, L"UNSOLD", { area.left + 10, area.top, area.left + 82, area.bottom },
            smallFont_, kGray, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, CompactValues(buildings), { area.left + 82, area.top, area.right - 10, area.bottom },
            smallFont_, kDark, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    void GameRenderer::DrawGameScreen(HDC dc, const RECT& clientRect, const Game& game)
    {
        const UiLayout layout = CreateGameLayout(clientRect);
        DrawHeader(dc, clientRect, L"ROUND " + std::to_wstring(game.CurrentRound()) + L" / 10     " + ModeText(game));
        restartButtonRect_ = { clientRect.right - 150, 12, clientRect.right - 20, 52 };
        DrawButton(dc, restartButtonRect_, L"RESTART", ButtonTone::White);
        const bool player1Turn = game.State() == GameState::Player1Selecting;
        DrawPlayerCity(dc, layout.player1Panel, game.Player1(), L"PLAYER 1", PlayerId::Player1, player1Turn);
        DrawPlayerCity(dc, layout.player2Panel, game.Player2(), Player2Name(game), PlayerId::Player2, !player1Turn);
        DrawBuildingCard(dc, layout.currentBuildingCard, game.CurrentBuilding());
        DrawUnsoldArea(dc, layout.unsoldArea, game.DiscardedBuildings());

        const std::wstring turnText = player1Turn
            ? L"PLAYER 1 － 入札カードを1枚選ぼう！"
            : game.IsCpuGame() ? L"CPU THINKING..." : L"PLAYER 2 － PLAYER 1の入札はヒミツです";
        DrawLabel(dc, turnText, { 180, 355, clientRect.right - 180, 399 }, headingFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const Player& activePlayer = player1Turn ? game.Player1() : game.Player2();
        const PlayerId activeOwner = player1Turn ? PlayerId::Player1 : PlayerId::Player2;
        DrawLabel(dc, L"YOUR MONEY CARDS", { layout.activeHandArea.left, layout.activeHandArea.top,
            layout.activeHandArea.right, layout.activeHandArea.top + 35 }, smallFont_, OwnerColor(activeOwner),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (!game.IsCpuGame() || player1Turn)
        {
            DrawActiveHand(dc, layout.activeHandArea, activePlayer, activeOwner);
        }

        DrawLabel(dc,
            game.CanConfirmSelection() ? L"選んだカードで入札します" : L"カードを選ぶと決定できます",
            { 360, 578, clientRect.right - 360, 615 }, bodyFont_, game.CanConfirmSelection() ? kNavy : kGray,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        primaryButtonRect_ = layout.confirmButton;
        DrawButton(dc, primaryButtonRect_, L"決定  /  BID!", ButtonTone::Coral, game.CanConfirmSelection());
    }

    void GameRenderer::DrawRoundResultScreen(HDC dc, const RECT& clientRect, const Game& game)
    {
        DrawHeader(dc, clientRect, L"ROUND " + std::to_wstring(game.CurrentRound()) + L" / 10     RESULT");
        restartButtonRect_ = { clientRect.right - 150, 12, clientRect.right - 20, 52 };
        DrawButton(dc, restartButtonRect_, L"RESTART", ButtonTone::White);
        const RoundResult& result = game.LastRoundResult();
        DrawPlayerCity(dc, { 18, 78, 392, 283 }, game.Player1(), L"PLAYER 1", PlayerId::Player1,
            result.winner == RoundWinner::Player1);
        DrawPlayerCity(dc, { clientRect.right - 392, 78, clientRect.right - 18, 283 }, game.Player2(),
            Player2Name(game), PlayerId::Player2, result.winner == RoundWinner::Player2);

        const RECT player1Card{ 447, 132, 517, 246 };
        const RECT buildingCard{ clientRect.right / 2 - 82, 78, clientRect.right / 2 + 82, 268 };
        const RECT player2Card{ clientRect.right - 517, 132, clientRect.right - 447, 246 };
        DrawMoneyCard(dc, player1Card, result.player1Bid, PlayerId::Player1, false, false);
        DrawBuildingCard(dc, buildingCard, result.building);
        DrawMoneyCard(dc, player2Card, result.player2Bid, PlayerId::Player2, false, false);
        DrawLabel(dc, L"PLAYER 1", { player1Card.left - 12, 98, player1Card.right + 12, 128 }, smallFont_, kPlayer1,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, game.IsCpuGame() ? L"CPU" : L"PLAYER 2",
            { player2Card.left - 12, 98, player2Card.right + 12, 128 }, smallFont_, kPlayer2,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const bool noWinner = result.winner == RoundWinner::None;
        const RECT banner{ 260, 310, clientRect.right - 260, 374 };
        FillRounded(dc, banner, noWinner ? kGrayLight : kYellowLight, noWinner ? kGray : kYellow, 18, 3);
        DrawLabel(dc, RoundWinnerMessage(result, game), banner, titleFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, L"PLAYER 1 USED   " + CompactValues(game.Player1().UsedMoneyCards()),
            { 210, 395, clientRect.right / 2 - 10, 430 }, smallFont_, kGray,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawLabel(dc, (game.IsCpuGame() ? L"CPU USED   " : L"PLAYER 2 USED   ") +
            CompactValues(game.Player2().UsedMoneyCards()),
            { clientRect.right / 2 + 10, 395, clientRect.right - 210, 430 }, smallFont_, kGray,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawUnsoldArea(dc, { 420, 445, clientRect.right - 420, 480 }, game.DiscardedBuildings());

        primaryButtonRect_ = { clientRect.right / 2 - 135, 610, clientRect.right / 2 + 135, 670 };
        DrawButton(dc, primaryButtonRect_, game.RemainingBuildingCount() == 0 ? L"FINAL RESULT" : L"NEXT",
            ButtonTone::Coral);
    }

    void GameRenderer::DrawSettingsScreen(HDC dc, const RECT& clientRect, const Settings& settings)
    {
        DrawHeader(dc, clientRect, L"SETTINGS");
        DrawLabel(dc, L"MASTER VOLUME", { 300, 105, 980, 150 }, titleFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        volumeSliderRect_ = { 355, 180, 925, 230 };
        const RECT track{ volumeSliderRect_.left, 201, volumeSliderRect_.right, 211 };
        FillRounded(dc, track, kGrayLight, kGray, 8);
        const int knobX = volumeSliderRect_.left +
            Width(volumeSliderRect_) * settings.masterVolume / 100;
        const RECT filled{ track.left, track.top, knobX, track.bottom };
        FillRounded(dc, filled, kSky, kSky, 8);
        const HBRUSH knobBrush = CreateSolidBrush(kWhite);
        const HPEN knobPen = CreatePen(PS_SOLID, 4, kNavy);
        const HGDIOBJ oldBrush = SelectObject(dc, knobBrush);
        const HGDIOBJ oldPen = SelectObject(dc, knobPen);
        Ellipse(dc, knobX - 15, 191, knobX + 15, 221);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(knobPen);
        DeleteObject(knobBrush);
        DrawLabel(dc, std::to_wstring(settings.masterVolume) + L"%",
            { 940, 180, 1040, 230 }, headingFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        muteButtonRect_ = { 490, 260, 790, 315 };
        DrawButton(dc, muteButtonRect_, settings.muted ? L"MUTE  ON" : L"MUTE  OFF",
            settings.muted ? ButtonTone::Coral : ButtonTone::Sky);
        howToPlayButtonRect_ = { 340, 355, 620, 410 };
        creditsButtonRect_ = { 660, 355, 940, 410 };
        feedbackButtonRect_ = { 340, 430, 940, 485 };
        backButtonRect_ = { 490, 535, 790, 590 };
        DrawButton(dc, howToPlayButtonRect_, L"HOW TO PLAY", ButtonTone::Yellow);
        DrawButton(dc, creditsButtonRect_, L"CREDITS", ButtonTone::White);
        DrawButton(dc, feedbackButtonRect_, L"FEEDBACK  /  感想・不具合報告", ButtonTone::Coral);
        DrawButton(dc, backButtonRect_, L"BACK", ButtonTone::Navy);
        DrawLabel(dc, L"音量とミュートだけが次回起動時も保存されます",
            { 280, 610, 1000, 650 }, bodyFont_, kGray,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void GameRenderer::DrawHowToPlayScreen(HDC dc, const RECT& clientRect)
    {
        DrawHeader(dc, clientRect, L"HOW TO PLAY");
        DrawLabel(dc, L"遊び方", { 70, 80, 400, 130 }, titleFont_, kNavy,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc,
            L"1. 毎ラウンド、建物カードが1枚公開されます\n"
            L"2. お互いにお金カードを1枚選びます\n"
            L"3. 建物価格を超え、相手より高いカードを出した側が建物を獲得\n"
            L"4. 使用したお金カードは、勝敗に関係なく消費されます\n"
            L"5. 同額の場合は、どちらも建物を獲得できません",
            { 70, 130, 760, 350 }, bodyFont_, kDark, DT_LEFT | DT_TOP | DT_WORDBREAK);

        DrawLabel(dc, L"カード例", { 820, 90, 1160, 130 }, headingFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawMoneyCard(dc, { 820, 150, 900, 275 }, 15, PlayerId::Player1, false, false);
        DrawBuildingCard(dc, { 955, 125, 1080, 285 }, 12);
        DrawMoneyCard(dc, { 1135, 150, 1215, 275 }, 13, PlayerId::Player2, false, false);
        DrawLabel(dc, L"15 > 13、さらに15 > 建物12\nPLAYER 1が建物を獲得！",
            { 790, 300, 1240, 365 }, bodyFont_, kNavy, DT_CENTER | DT_TOP | DT_WORDBREAK);

        const RECT scoring{ 70, 390, clientRect.right - 70, 545 };
        FillRounded(dc, scoring, kYellowLight, kYellow, 18, 2);
        DrawLabel(dc, L"10ラウンド終了後、3項目で勝敗を決定",
            { scoring.left + 20, scoring.top + 12, scoring.right - 20, scoring.top + 52 },
            headingFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, L"建物枚数        建物合計        残金",
            { scoring.left + 30, scoring.top + 60, scoring.right - 30, scoring.bottom - 18 },
            titleFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        backButtonRect_ = { clientRect.right / 2 - 140, 610, clientRect.right / 2 + 140, 665 };
        DrawButton(dc, backButtonRect_, L"BACK", ButtonTone::Navy);
    }

    void GameRenderer::DrawCreditsScreen(HDC dc, const RECT& clientRect)
    {
        DrawHeader(dc, clientRect, L"CREDITS");
        DrawLabel(dc, L"CREDITS", { 200, 140, clientRect.right - 200, 210 }, titleFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        const RECT card{ 390, 240, clientRect.right - 390, 450 };
        FillRounded(dc, card, kWhite, kSky, 24, 3);
        DrawLabel(dc, L"Creator", { card.left + 20, card.top + 30, card.right - 20, card.top + 90 },
            headingFont_, kGray, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLabel(dc, config::CreatorName,
            { card.left + 20, card.top + 95, card.right - 20, card.bottom - 25 },
            titleFont_, kNavy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        backButtonRect_ = { clientRect.right / 2 - 140, 535, clientRect.right / 2 + 140, 590 };
        DrawButton(dc, backButtonRect_, L"BACK", ButtonTone::Navy);
    }

    void GameRenderer::DrawFinalResultScreen(HDC dc, const RECT& clientRect, const Game& game)
    {
        DrawHeader(dc, clientRect, L"FINAL RESULT     " + ModeText(game));
        const FinalResult& result = game.FinalGameResult();
        const std::wstring winner = result.overallWinner == CategoryWinner::None
            ? L"DRAW!"
            : WinnerName(result.overallWinner, game) + L" WINS!";
        DrawLabel(dc, winner, { 190, 70, clientRect.right - 190, 135 }, titleFont_, kNavy,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        DrawPlayerCity(dc, { 35, 138, 618, 405 }, game.Player1(), L"PLAYER 1 CITY", PlayerId::Player1,
            result.overallWinner == CategoryWinner::Player1, true);
        DrawPlayerCity(dc, { 662, 138, clientRect.right - 35, 405 }, game.Player2(),
            (game.IsCpuGame() ? L"CPU CITY" : L"PLAYER 2 CITY"), PlayerId::Player2,
            result.overallWinner == CategoryWinner::Player2, true);

        const auto drawMetric = [&](int top, const std::wstring& label, int player1Value, int player2Value,
            CategoryWinner categoryWinner)
        {
            const RECT row{ 150, top, clientRect.right - 150, top + 45 };
            FillRounded(dc, row, kWhite, kGrayLight, 12);
            const bool player1Wins = categoryWinner == CategoryWinner::Player1;
            const bool player2Wins = categoryWinner == CategoryWinner::Player2;
            DrawLabel(dc, player1Wins ? L"★  " + std::to_wstring(player1Value) : std::to_wstring(player1Value),
                { row.left + 20, row.top, row.left + 250, row.bottom }, headingFont_, player1Wins ? kPlayer1 : kNavy,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawLabel(dc, label, { row.left + 280, row.top, row.right - 280, row.bottom }, smallFont_, kDark,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawLabel(dc, player2Wins ? std::to_wstring(player2Value) + L"  ★" : std::to_wstring(player2Value),
                { row.right - 250, row.top, row.right - 20, row.bottom }, headingFont_, player2Wins ? kPlayer2 : kNavy,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        };

        drawMetric(424, L"BUILDINGS", result.player1.buildingCount, result.player2.buildingCount,
            result.buildingCountWinner);
        drawMetric(477, L"PROPERTY VALUE", result.player1.buildingTotal, result.player2.buildingTotal,
            result.buildingTotalWinner);
        drawMetric(530, L"CASH LEFT", result.player1.remainingMoneyTotal, result.player2.remainingMoneyTotal,
            result.remainingMoneyWinner);

        secondaryButtonRect_ = { clientRect.right / 2 - 300, 625, clientRect.right / 2 - 20, 682 };
        primaryButtonRect_ = { clientRect.right / 2 + 20, 625, clientRect.right / 2 + 300, 682 };
        DrawButton(dc, secondaryButtonRect_, L"TITLE", ButtonTone::White);
        DrawButton(dc, primaryButtonRect_, L"PLAY AGAIN", ButtonTone::Coral);
    }

    std::optional<int> GameRenderer::HitTestMoneyCard(POINT point, PlayerId owner) const noexcept
    {
        for (const CardView& card : moneyCardViews_)
        {
            if (card.owner == owner && PtInRect(&card.rect, point))
            {
                return card.value;
            }
        }
        return std::nullopt;
    }

    bool GameRenderer::HitTestPrimaryButton(POINT point) const noexcept
    {
        return PtInRect(&primaryButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestSecondaryButton(POINT point) const noexcept
    {
        return PtInRect(&secondaryButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestExitButton(POINT point) const noexcept
    {
        return PtInRect(&exitButtonRect_, point) != FALSE;
    }

    std::optional<GameMode> GameRenderer::HitTestGameMode(POINT point) const noexcept
    {
        if (PtInRect(&playerVsCpuButtonRect_, point))
        {
            return GameMode::PlayerVsCpu;
        }
        if (PtInRect(&playerVsPlayerButtonRect_, point))
        {
            return GameMode::PlayerVsPlayer;
        }
        return std::nullopt;
    }

    std::optional<CpuDifficulty> GameRenderer::HitTestCpuDifficulty(POINT point) const noexcept
    {
        for (std::size_t index = 0; index < difficultyButtonRects_.size(); ++index)
        {
            if (PtInRect(&difficultyButtonRects_[index], point))
            {
                return static_cast<CpuDifficulty>(index);
            }
        }
        return std::nullopt;
    }

    bool GameRenderer::HitTestSettingsButton(POINT point) const noexcept
    {
        return PtInRect(&settingsButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestHowToPlayButton(POINT point) const noexcept
    {
        return PtInRect(&howToPlayButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestCreditsButton(POINT point) const noexcept
    {
        return PtInRect(&creditsButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestFeedbackButton(POINT point) const noexcept
    {
        return PtInRect(&feedbackButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestBackButton(POINT point) const noexcept
    {
        return PtInRect(&backButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestMuteButton(POINT point) const noexcept
    {
        return PtInRect(&muteButtonRect_, point) != FALSE;
    }

    bool GameRenderer::HitTestRestartButton(POINT point) const noexcept
    {
        return PtInRect(&restartButtonRect_, point) != FALSE;
    }

    std::optional<int> GameRenderer::HitTestVolumeSlider(POINT point) const noexcept
    {
        if (PtInRect(&volumeSliderRect_, point) == FALSE)
        {
            return std::nullopt;
        }
        const int width = (std::max)(1, Width(volumeSliderRect_));
        const int relativeX = static_cast<int>(point.x - volumeSliderRect_.left);
        return (std::clamp)(relativeX * 100 / width, 0, 100);
    }

    bool GameRenderer::IsHovered(const RECT& rect) const noexcept
    {
        return mousePosition_.has_value() && PtInRect(&rect, *mousePosition_) != FALSE;
    }

    int GameRenderer::HoverRegionAt(POINT point) const noexcept
    {
        if (PtInRect(&primaryButtonRect_, point)) return 1;
        if (PtInRect(&secondaryButtonRect_, point)) return 2;
        if (PtInRect(&exitButtonRect_, point)) return 3;
        if (PtInRect(&playerVsCpuButtonRect_, point)) return 4;
        if (PtInRect(&playerVsPlayerButtonRect_, point)) return 5;
        if (PtInRect(&settingsButtonRect_, point)) return 6;
        if (PtInRect(&howToPlayButtonRect_, point)) return 7;
        if (PtInRect(&creditsButtonRect_, point)) return 8;
        if (PtInRect(&feedbackButtonRect_, point)) return 9;
        if (PtInRect(&backButtonRect_, point)) return 20;
        if (PtInRect(&muteButtonRect_, point)) return 21;
        if (PtInRect(&restartButtonRect_, point)) return 22;
        if (PtInRect(&volumeSliderRect_, point)) return 23;
        for (std::size_t index = 0; index < difficultyButtonRects_.size(); ++index)
        {
            if (PtInRect(&difficultyButtonRects_[index], point)) return 10 + static_cast<int>(index);
        }
        for (std::size_t index = 0; index < moneyCardViews_.size(); ++index)
        {
            if (PtInRect(&moneyCardViews_[index].rect, point)) return 100 + static_cast<int>(index);
        }
        return 0;
    }

    bool GameRenderer::UpdateHover(POINT point) noexcept
    {
        const int oldRegion = mousePosition_.has_value() ? HoverRegionAt(*mousePosition_) : 0;
        const int newRegion = HoverRegionAt(point);
        mousePosition_ = point;
        return oldRegion != newRegion;
    }

    bool GameRenderer::ClearHover() noexcept
    {
        if (!mousePosition_.has_value())
        {
            return false;
        }
        const bool wasInteractive = HoverRegionAt(*mousePosition_) != 0;
        mousePosition_.reset();
        return wasInteractive;
    }
}
