// TownMakerProject.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "TownMakerProject.h"

#include "BattleDataWriter.h"
#include "AppConfig.h"
#include "ExternalLinkOpener.h"
#include "Game.h"
#include "GameRenderer.h"
#include "Settings.h"
#include "SoundManager.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <windowsx.h>

#define MAX_LOADSTRING 100

namespace
{
    constexpr int kClientWidth = 1280;
    constexpr int kClientHeight = 720;
    constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    std::filesystem::path GetExecutableDirectory();

    struct AppState
    {
        AppState()
            : settingsStore(GetExecutableDirectory() / L"settings.cfg"),
              settings(settingsStore.Load()),
              soundManager(GetExecutableDirectory() / L"assets" / L"sounds")
        {
            soundManager.SetMasterVolume(settings.masterVolume, settings.muted);
        }

        townmaker::Game game;
        townmaker::CpuPlayer cpuPlayer;
        townmaker::BattleDataWriter battleDataWriter;
        townmaker::GameRenderer renderer;
        townmaker::FileSettingsStore settingsStore;
        townmaker::Settings settings;
        townmaker::SoundManager soundManager;
        townmaker::Win32ExternalLinkOpener externalLinkOpener;
        townmaker::UiPage page = townmaker::UiPage::Game;
        townmaker::UiPage returnPage = townmaker::UiPage::Game;
    };

    std::filesystem::path GetExecutableDirectory()
    {
        std::array<wchar_t, 32768> executablePath{};
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= static_cast<DWORD>(executablePath.size()))
        {
            return std::filesystem::current_path();
        }
        return std::filesystem::path(executablePath.data()).parent_path();
    }

    AppState* GetAppState(HWND window) noexcept
    {
        return reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    std::filesystem::path GetBattleDataDirectory()
    {
        return GetExecutableDirectory() / L"BattleData";
    }

    void SaveCompletedBattle(HWND window, AppState& appState)
    {
        const std::filesystem::path outputDirectory = GetBattleDataDirectory();
        const townmaker::BattleDataSaveResult result =
            appState.battleDataWriter.Save(appState.game, outputDirectory);
        if (!result.succeeded)
        {
            const std::wstring message = result.errorMessage +
                L"\n\n保存先: " + outputDirectory.wstring();
            MessageBoxW(window, message.c_str(), L"対戦データ保存エラー", MB_OK | MB_ICONERROR);
        }
    }

    void PaintGame(HWND window, HDC targetDeviceContext, AppState& appState)
    {
        RECT clientRect{};
        GetClientRect(window, &clientRect);

        const HDC memoryDeviceContext = CreateCompatibleDC(targetDeviceContext);
        const HBITMAP backBuffer = CreateCompatibleBitmap(
            targetDeviceContext,
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top);

        if (memoryDeviceContext == nullptr || backBuffer == nullptr)
        {
            if (backBuffer != nullptr)
            {
                DeleteObject(backBuffer);
            }
            if (memoryDeviceContext != nullptr)
            {
                DeleteDC(memoryDeviceContext);
            }
            appState.renderer.Draw(targetDeviceContext, clientRect, appState.game,
                appState.page, appState.settings);
            return;
        }

        const HGDIOBJ oldBitmap = SelectObject(memoryDeviceContext, backBuffer);
        appState.renderer.Draw(memoryDeviceContext, clientRect, appState.game,
            appState.page, appState.settings);
        BitBlt(
            targetDeviceContext,
            0,
            0,
            clientRect.right,
            clientRect.bottom,
            memoryDeviceContext,
            0,
            0,
            SRCCOPY);
        SelectObject(memoryDeviceContext, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(memoryDeviceContext);
    }

    void ResolveCpuTurn(AppState& appState) noexcept
    {
        townmaker::Game& game = appState.game;
        if (!game.IsCpuGame() || game.State() != townmaker::GameState::Player2Selecting)
        {
            return;
        }

        const townmaker::CpuGameState publicState = game.CreateCpuGameState();
        try
        {
            const int cpuBid = appState.cpuPlayer.SelectCard(publicState, game.Difficulty());
            if (game.SubmitCpuBid(cpuBid))
            {
                return;
            }
        }
        catch (...)
        {
        }

        if (!publicState.cpuMoneyCards.empty())
        {
            game.SubmitCpuBid(publicState.cpuMoneyCards.front());
        }
    }

    void HandleLeftClick(HWND window, AppState& appState, POINT point)
    {
        if (appState.page != townmaker::UiPage::Game)
        {
            if (appState.page == townmaker::UiPage::Settings)
            {
                if (const std::optional<int> volume =
                    appState.renderer.HitTestVolumeSlider(point))
                {
                    appState.settings.masterVolume = *volume;
                    appState.soundManager.SetMasterVolume(
                        appState.settings.masterVolume, appState.settings.muted);
                    appState.settingsStore.Save(appState.settings);
                    appState.soundManager.Play(townmaker::SoundEffect::Click);
                    InvalidateRect(window, nullptr, FALSE);
                    return;
                }
                if (appState.renderer.HitTestMuteButton(point))
                {
                    appState.settings.muted = !appState.settings.muted;
                    appState.soundManager.SetMasterVolume(
                        appState.settings.masterVolume, appState.settings.muted);
                    appState.settingsStore.Save(appState.settings);
                    appState.soundManager.Play(townmaker::SoundEffect::Click);
                    InvalidateRect(window, nullptr, FALSE);
                    return;
                }
                if (appState.renderer.HitTestHowToPlayButton(point))
                {
                    appState.returnPage = townmaker::UiPage::Settings;
                    appState.page = townmaker::UiPage::HowToPlay;
                }
                else if (appState.renderer.HitTestCreditsButton(point))
                {
                    appState.returnPage = townmaker::UiPage::Settings;
                    appState.page = townmaker::UiPage::Credits;
                }
                else if (appState.renderer.HitTestFeedbackButton(point))
                {
                    appState.externalLinkOpener.Open(townmaker::config::FeedbackUrl);
                }
                else if (appState.renderer.HitTestBackButton(point))
                {
                    appState.page = townmaker::UiPage::Game;
                }
                else
                {
                    return;
                }
            }
            else if (appState.renderer.HitTestBackButton(point))
            {
                appState.page = appState.returnPage;
            }
            else
            {
                return;
            }
            appState.soundManager.Play(townmaker::SoundEffect::Click);
            InvalidateRect(window, nullptr, FALSE);
            return;
        }

        townmaker::Game& game = appState.game;
        const townmaker::GameState state = game.State();

        if (state == townmaker::GameState::ModeSelection)
        {
            if (appState.renderer.HitTestSettingsButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                appState.page = townmaker::UiPage::Settings;
                InvalidateRect(window, nullptr, FALSE);
                return;
            }
            if (appState.renderer.HitTestHowToPlayButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                appState.returnPage = townmaker::UiPage::Game;
                appState.page = townmaker::UiPage::HowToPlay;
                InvalidateRect(window, nullptr, FALSE);
                return;
            }
            if (appState.renderer.HitTestCreditsButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                appState.returnPage = townmaker::UiPage::Game;
                appState.page = townmaker::UiPage::Credits;
                InvalidateRect(window, nullptr, FALSE);
                return;
            }
            if (appState.renderer.HitTestFeedbackButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                appState.externalLinkOpener.Open(townmaker::config::FeedbackUrl);
                return;
            }
            if (appState.renderer.HitTestExitButton(point))
            {
                DestroyWindow(window);
                return;
            }
            const std::optional<townmaker::GameMode> mode =
                appState.renderer.HitTestGameMode(point);
            if (mode.has_value() && game.SelectGameMode(*mode))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                if (game.State() == townmaker::GameState::Player1Selecting)
                {
                    appState.soundManager.Play(townmaker::SoundEffect::BuildingRevealed);
                }
                InvalidateRect(window, nullptr, FALSE);
            }
            return;
        }

        if ((state == townmaker::GameState::Player1Selecting ||
             state == townmaker::GameState::Player2Selecting ||
             state == townmaker::GameState::RoundResult) &&
            appState.renderer.HitTestRestartButton(point))
        {
            appState.soundManager.Play(townmaker::SoundEffect::Click);
            if (MessageBoxW(window, L"対戦を最初からやり直しますか？", L"RESTART",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
            {
                game.RestartCurrentGame();
                appState.soundManager.Play(townmaker::SoundEffect::BuildingRevealed);
                InvalidateRect(window, nullptr, FALSE);
            }
            return;
        }

        if (state == townmaker::GameState::CpuDifficultySelection)
        {
            const std::optional<townmaker::CpuDifficulty> difficulty =
                appState.renderer.HitTestCpuDifficulty(point);
            if (difficulty.has_value() && game.SelectCpuDifficulty(*difficulty))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                appState.soundManager.Play(townmaker::SoundEffect::BuildingRevealed);
                InvalidateRect(window, nullptr, FALSE);
            }
            else if (appState.renderer.HitTestSecondaryButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                game.ShowModeSelection();
                InvalidateRect(window, nullptr, FALSE);
            }
            return;
        }

        if (state == townmaker::GameState::Player1Selecting ||
            state == townmaker::GameState::Player2Selecting)
        {
            const std::optional<int> card = appState.renderer.HitTestMoneyCard(
                point,
                game.ActivePlayer());
            if (card.has_value() && game.SelectMoneyCard(*card))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                InvalidateRect(window, nullptr, FALSE);
                return;
            }

            if (appState.renderer.HitTestPrimaryButton(point) && game.ConfirmSelection())
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                ResolveCpuTurn(appState);
                if (game.State() == townmaker::GameState::RoundResult &&
                    game.LastRoundResult().winner != townmaker::RoundWinner::None)
                {
                    appState.soundManager.Play(townmaker::SoundEffect::BuildingAcquired);
                }
                InvalidateRect(window, nullptr, FALSE);
            }
            return;
        }

        if (state == townmaker::GameState::RoundResult)
        {
            if (appState.renderer.HitTestPrimaryButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                if (game.AdvanceAfterRound() && game.State() == townmaker::GameState::GameResult)
                {
                    SaveCompletedBattle(window, appState);
                }
                else if (game.State() == townmaker::GameState::Player1Selecting)
                {
                    appState.soundManager.Play(townmaker::SoundEffect::BuildingRevealed);
                }
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        else if (state == townmaker::GameState::GameResult)
        {
            if (appState.renderer.HitTestSecondaryButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                game.ShowModeSelection();
                InvalidateRect(window, nullptr, FALSE);
            }
            else if (appState.renderer.HitTestPrimaryButton(point))
            {
                appState.soundManager.Play(townmaker::SoundEffect::Click);
                game.RestartCurrentGame();
                appState.soundManager.Play(townmaker::SoundEffect::BuildingRevealed);
                InvalidateRect(window, nullptr, FALSE);
            }
        }
    }
}

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    lstrcpyW(szTitle, L"ビルビル - BILL BILL");
    LoadStringW(hInstance, IDC_TOWNMAKERPROJECT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    const HACCEL acceleratorTable = LoadAccelerators(
        hInstance,
        MAKEINTRESOURCE(IDC_TOWNMAKERPROJECT));
    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(message.hwnd, acceleratorTable, &message))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

    return static_cast<int>(message.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TOWNMAKERPROJECT));
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = MAKEINTRESOURCEW(IDC_TOWNMAKERPROJECT);
    windowClass.lpszClassName = szWindowClass;
    windowClass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&windowClass);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    RECT windowRect{ 0, 0, kClientWidth, kClientHeight };
    if (!AdjustWindowRect(&windowRect, kWindowStyle, TRUE))
    {
        return FALSE;
    }

    const HWND window = CreateWindowW(
        szWindowClass,
        szTitle,
        kWindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (window == nullptr)
    {
        return FALSE;
    }

    ShowWindow(window, nCmdShow);
    UpdateWindow(window);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        {
            auto appState = std::make_unique<AppState>();
            SetWindowLongPtrW(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(appState.get()));
            appState.release();
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (AppState* appState = GetAppState(window))
        {
            const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            HandleLeftClick(window, *appState, point);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (AppState* appState = GetAppState(window))
        {
            TRACKMOUSEEVENT tracking{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, window, 0 };
            TrackMouseEvent(&tracking);
            const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if ((wParam & MK_LBUTTON) != 0 && appState->page == townmaker::UiPage::Settings)
            {
                if (const std::optional<int> volume =
                    appState->renderer.HitTestVolumeSlider(point))
                {
                    appState->settings.masterVolume = *volume;
                    appState->soundManager.SetMasterVolume(
                        appState->settings.masterVolume, appState->settings.muted);
                    appState->settingsStore.Save(appState->settings);
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            if (appState->renderer.UpdateHover(point))
            {
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        return 0;

    case WM_MOUSELEAVE:
        if (AppState* appState = GetAppState(window))
        {
            if (appState->renderer.ClearHover())
            {
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), window, About);
            return 0;
        case IDM_EXIT:
            DestroyWindow(window);
            return 0;
        default:
            break;
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            const HDC deviceContext = BeginPaint(window, &paint);
            if (AppState* appState = GetAppState(window))
            {
                PaintGame(window, deviceContext, *appState);
            }
            EndPaint(window, &paint);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        {
            std::unique_ptr<AppState> appState(GetAppState(window));
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return DefWindowProc(window, message, wParam, lParam);
        }
    }

    return DefWindowProc(window, message, wParam, lParam);
}

INT_PTR CALLBACK About(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return static_cast<INT_PTR>(TRUE);

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(dialog, LOWORD(wParam));
            return static_cast<INT_PTR>(TRUE);
        }
        break;
    }
    return static_cast<INT_PTR>(FALSE);
}
