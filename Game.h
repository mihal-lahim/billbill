#pragma once

#include "CpuPlayer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace townmaker
{
    enum class PlayerId
    {
        Player1,
        Player2
    };

    enum class GameState
    {
        ModeSelection,
        CpuDifficultySelection,
        Player1Selecting,
        Player2Selecting,
        RoundResult,
        GameResult
    };

    enum class RoundWinner
    {
        None,
        Player1,
        Player2
    };

    enum class CategoryWinner
    {
        None,
        Player1,
        Player2
    };

    struct RoundResult
    {
        int building = 0;
        int player1Bid = 0;
        int player2Bid = 0;
        RoundWinner winner = RoundWinner::None;
    };

    struct PlayerSummary
    {
        int buildingCount = 0;
        int buildingTotal = 0;
        int remainingMoneyTotal = 0;
        int points = 0;
    };

    struct FinalResult
    {
        PlayerSummary player1;
        PlayerSummary player2;
        CategoryWinner buildingCountWinner = CategoryWinner::None;
        CategoryWinner buildingTotalWinner = CategoryWinner::None;
        CategoryWinner remainingMoneyWinner = CategoryWinner::None;
        CategoryWinner overallWinner = CategoryWinner::None;
    };

    class Player
    {
    public:
        Player();

        const std::vector<int>& MoneyCards() const noexcept;
        const std::vector<int>& UsedMoneyCards() const noexcept;
        const std::vector<int>& Buildings() const noexcept;
        std::optional<int> SelectedMoneyCard() const noexcept;

        bool ToggleMoneyCard(int value);
        void ClearSelection() noexcept;

    private:
        friend class Game;

        void Reset();
        bool ConsumeMoneyCard(int value);
        void AddBuilding(int value);

        std::vector<int> moneyCards_;
        std::vector<int> usedMoneyCards_;
        std::vector<int> buildings_;
        std::optional<int> selectedMoneyCard_;
    };

    RoundResult ResolveRound(int building, int player1Bid, int player2Bid) noexcept;
    FinalResult CalculateFinalResult(const Player& player1, const Player& player2) noexcept;

    class Game
    {
    public:
        Game();

        bool SelectGameMode(GameMode mode);
        bool SelectCpuDifficulty(CpuDifficulty difficulty);
        void StartNewGame(GameMode mode, CpuDifficulty difficulty);
        void StartNewGame(GameMode mode, CpuDifficulty difficulty, std::uint32_t shuffleSeed);
        void RestartCurrentGame();
        void ShowModeSelection();

        GameState State() const noexcept;
        GameMode Mode() const noexcept;
        CpuDifficulty Difficulty() const noexcept;
        bool IsCpuGame() const noexcept;
        PlayerId ActivePlayer() const noexcept;
        int CurrentRound() const noexcept;
        int CurrentBuilding() const noexcept;
        std::size_t RemainingBuildingCount() const noexcept;
        const Player& Player1() const noexcept;
        const Player& Player2() const noexcept;
        const std::vector<int>& DiscardedBuildings() const noexcept;
        const std::vector<RoundResult>& RoundHistory() const noexcept;
        const RoundResult& LastRoundResult() const noexcept;
        const FinalResult& FinalGameResult() const noexcept;
        CpuGameState CreateCpuGameState() const;

        bool SelectMoneyCard(int value);
        bool CanConfirmSelection() const noexcept;
        bool ConfirmSelection();
        bool SubmitCpuBid(int value);
        bool AdvanceAfterRound();

    private:
        void ResetWithSeed(std::uint32_t shuffleSeed);
        void StartCurrentRound();
        void FinishCurrentRound(int player2Bid);

        Player player1_;
        Player player2_;
        std::vector<int> buildingDeck_;
        std::vector<int> discardedBuildings_;
        std::vector<RoundResult> roundHistory_;
        std::size_t nextBuildingIndex_ = 0;
        int currentRound_ = 0;
        int currentBuilding_ = 0;
        GameState state_ = GameState::ModeSelection;
        GameMode mode_ = GameMode::PlayerVsPlayer;
        CpuDifficulty difficulty_ = CpuDifficulty::Normal;
        std::optional<int> player1Bid_;
        RoundResult lastRoundResult_;
        FinalResult finalResult_;
    };
}
