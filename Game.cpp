#include "Game.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <random>

namespace townmaker
{
    namespace
    {
        constexpr std::array<int, 12> kInitialMoneyCards{
            1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23
        };

        constexpr std::array<int, 10> kBuildingCards{
            2, 4, 6, 8, 10, 12, 14, 16, 18, 20
        };

        CategoryWinner CompareValues(int player1Value, int player2Value) noexcept
        {
            if (player1Value > player2Value)
            {
                return CategoryWinner::Player1;
            }
            if (player2Value > player1Value)
            {
                return CategoryWinner::Player2;
            }
            return CategoryWinner::None;
        }

        void AwardPoint(CategoryWinner winner, FinalResult& result) noexcept
        {
            if (winner == CategoryWinner::Player1)
            {
                ++result.player1.points;
            }
            else if (winner == CategoryWinner::Player2)
            {
                ++result.player2.points;
            }
        }
    }

    Player::Player()
    {
        Reset();
    }

    const std::vector<int>& Player::MoneyCards() const noexcept
    {
        return moneyCards_;
    }

    const std::vector<int>& Player::UsedMoneyCards() const noexcept
    {
        return usedMoneyCards_;
    }

    const std::vector<int>& Player::Buildings() const noexcept
    {
        return buildings_;
    }

    std::optional<int> Player::SelectedMoneyCard() const noexcept
    {
        return selectedMoneyCard_;
    }

    bool Player::ToggleMoneyCard(int value)
    {
        if (std::find(moneyCards_.begin(), moneyCards_.end(), value) == moneyCards_.end())
        {
            return false;
        }

        if (selectedMoneyCard_ == value)
        {
            selectedMoneyCard_.reset();
        }
        else
        {
            selectedMoneyCard_ = value;
        }
        return true;
    }

    void Player::ClearSelection() noexcept
    {
        selectedMoneyCard_.reset();
    }

    void Player::Reset()
    {
        moneyCards_.assign(kInitialMoneyCards.begin(), kInitialMoneyCards.end());
        usedMoneyCards_.clear();
        buildings_.clear();
        selectedMoneyCard_.reset();
    }

    bool Player::ConsumeMoneyCard(int value)
    {
        const auto card = std::find(moneyCards_.begin(), moneyCards_.end(), value);
        if (card == moneyCards_.end())
        {
            return false;
        }

        usedMoneyCards_.push_back(value);
        moneyCards_.erase(card);
        if (selectedMoneyCard_ == value)
        {
            selectedMoneyCard_.reset();
        }
        return true;
    }

    void Player::AddBuilding(int value)
    {
        buildings_.push_back(value);
    }

    RoundResult ResolveRound(int building, int player1Bid, int player2Bid) noexcept
    {
        RoundResult result{ building, player1Bid, player2Bid, RoundWinner::None };

        if (player1Bid == player2Bid)
        {
            return result;
        }

        const bool player1Qualifies = player1Bid > building && player1Bid > player2Bid;
        const bool player2Qualifies = player2Bid > building && player2Bid > player1Bid;

        if (player1Qualifies)
        {
            result.winner = RoundWinner::Player1;
        }
        else if (player2Qualifies)
        {
            result.winner = RoundWinner::Player2;
        }

        return result;
    }

    FinalResult CalculateFinalResult(const Player& player1, const Player& player2) noexcept
    {
        FinalResult result;
        result.player1.buildingCount = static_cast<int>(player1.Buildings().size());
        result.player2.buildingCount = static_cast<int>(player2.Buildings().size());
        result.player1.buildingTotal = std::accumulate(
            player1.Buildings().begin(), player1.Buildings().end(), 0);
        result.player2.buildingTotal = std::accumulate(
            player2.Buildings().begin(), player2.Buildings().end(), 0);
        result.player1.remainingMoneyTotal = std::accumulate(
            player1.MoneyCards().begin(), player1.MoneyCards().end(), 0);
        result.player2.remainingMoneyTotal = std::accumulate(
            player2.MoneyCards().begin(), player2.MoneyCards().end(), 0);

        result.buildingCountWinner = CompareValues(
            result.player1.buildingCount, result.player2.buildingCount);
        result.buildingTotalWinner = CompareValues(
            result.player1.buildingTotal, result.player2.buildingTotal);
        result.remainingMoneyWinner = CompareValues(
            result.player1.remainingMoneyTotal, result.player2.remainingMoneyTotal);

        AwardPoint(result.buildingCountWinner, result);
        AwardPoint(result.buildingTotalWinner, result);
        AwardPoint(result.remainingMoneyWinner, result);
        result.overallWinner = CompareValues(result.player1.points, result.player2.points);
        return result;
    }

    Game::Game() = default;

    bool Game::SelectGameMode(GameMode mode)
    {
        if (state_ != GameState::ModeSelection)
        {
            return false;
        }

        mode_ = mode;
        if (mode == GameMode::PlayerVsCpu)
        {
            state_ = GameState::CpuDifficultySelection;
        }
        else
        {
            StartNewGame(GameMode::PlayerVsPlayer, CpuDifficulty::Normal);
        }
        return true;
    }

    bool Game::SelectCpuDifficulty(CpuDifficulty difficulty)
    {
        if (state_ != GameState::CpuDifficultySelection)
        {
            return false;
        }
        StartNewGame(GameMode::PlayerVsCpu, difficulty);
        return true;
    }

    void Game::StartNewGame(GameMode mode, CpuDifficulty difficulty)
    {
        std::random_device randomDevice;
        StartNewGame(mode, difficulty, randomDevice());
    }

    void Game::StartNewGame(
        GameMode mode,
        CpuDifficulty difficulty,
        std::uint32_t shuffleSeed)
    {
        mode_ = mode;
        difficulty_ = difficulty;
        ResetWithSeed(shuffleSeed);
    }

    void Game::RestartCurrentGame()
    {
        StartNewGame(mode_, difficulty_);
    }

    void Game::ShowModeSelection()
    {
        player1_.Reset();
        player2_.Reset();
        buildingDeck_.clear();
        discardedBuildings_.clear();
        roundHistory_.clear();
        nextBuildingIndex_ = 0;
        currentRound_ = 0;
        currentBuilding_ = 0;
        player1Bid_.reset();
        lastRoundResult_ = {};
        finalResult_ = {};
        state_ = GameState::ModeSelection;
    }

    GameState Game::State() const noexcept
    {
        return state_;
    }

    GameMode Game::Mode() const noexcept
    {
        return mode_;
    }

    CpuDifficulty Game::Difficulty() const noexcept
    {
        return difficulty_;
    }

    bool Game::IsCpuGame() const noexcept
    {
        return mode_ == GameMode::PlayerVsCpu;
    }

    PlayerId Game::ActivePlayer() const noexcept
    {
        return state_ == GameState::Player2Selecting
            ? PlayerId::Player2
            : PlayerId::Player1;
    }

    int Game::CurrentRound() const noexcept
    {
        return currentRound_;
    }

    int Game::CurrentBuilding() const noexcept
    {
        return currentBuilding_;
    }

    std::size_t Game::RemainingBuildingCount() const noexcept
    {
        return buildingDeck_.size() - nextBuildingIndex_;
    }

    const Player& Game::Player1() const noexcept
    {
        return player1_;
    }

    const Player& Game::Player2() const noexcept
    {
        return player2_;
    }

    const std::vector<int>& Game::DiscardedBuildings() const noexcept
    {
        return discardedBuildings_;
    }

    const std::vector<RoundResult>& Game::RoundHistory() const noexcept
    {
        return roundHistory_;
    }

    const RoundResult& Game::LastRoundResult() const noexcept
    {
        return lastRoundResult_;
    }

    const FinalResult& Game::FinalGameResult() const noexcept
    {
        return finalResult_;
    }

    CpuGameState Game::CreateCpuGameState() const
    {
        CpuGameState cpuState;
        cpuState.currentBuilding = currentBuilding_;
        cpuState.round = currentRound_;
        cpuState.totalRounds = static_cast<int>(buildingDeck_.size());
        cpuState.remainingRounds = cpuState.totalRounds - currentRound_ + 1;
        cpuState.cpuMoneyCards = player2_.MoneyCards();
        cpuState.opponentMoneyCards = player1_.MoneyCards();
        cpuState.cpuBuildings = player2_.Buildings();
        cpuState.opponentBuildings = player1_.Buildings();
        cpuState.cpuUsedMoneyCards = player2_.UsedMoneyCards();
        cpuState.opponentUsedMoneyCards = player1_.UsedMoneyCards();
        cpuState.discardedBuildings = discardedBuildings_;
        return cpuState;
    }

    bool Game::SelectMoneyCard(int value)
    {
        if (state_ == GameState::Player1Selecting)
        {
            return player1_.ToggleMoneyCard(value);
        }
        if (state_ == GameState::Player2Selecting && mode_ == GameMode::PlayerVsPlayer)
        {
            return player2_.ToggleMoneyCard(value);
        }
        return false;
    }

    bool Game::CanConfirmSelection() const noexcept
    {
        if (state_ == GameState::Player1Selecting)
        {
            return player1_.SelectedMoneyCard().has_value();
        }
        if (state_ == GameState::Player2Selecting && mode_ == GameMode::PlayerVsPlayer)
        {
            return player2_.SelectedMoneyCard().has_value();
        }
        return false;
    }

    bool Game::ConfirmSelection()
    {
        if (!CanConfirmSelection())
        {
            return false;
        }

        if (state_ == GameState::Player1Selecting)
        {
            player1Bid_ = player1_.SelectedMoneyCard();
            player1_.ClearSelection();
            state_ = GameState::Player2Selecting;
            return true;
        }

        const int player2Bid = *player2_.SelectedMoneyCard();
        player2_.ClearSelection();
        FinishCurrentRound(player2Bid);
        return true;
    }

    bool Game::SubmitCpuBid(int value)
    {
        if (mode_ != GameMode::PlayerVsCpu || state_ != GameState::Player2Selecting)
        {
            return false;
        }
        if (std::find(player2_.MoneyCards().begin(), player2_.MoneyCards().end(), value) ==
            player2_.MoneyCards().end())
        {
            return false;
        }

        FinishCurrentRound(value);
        return true;
    }

    bool Game::AdvanceAfterRound()
    {
        if (state_ != GameState::RoundResult)
        {
            return false;
        }

        if (nextBuildingIndex_ >= buildingDeck_.size())
        {
            finalResult_ = CalculateFinalResult(player1_, player2_);
            state_ = GameState::GameResult;
            return true;
        }

        ++currentRound_;
        StartCurrentRound();
        return true;
    }

    void Game::ResetWithSeed(std::uint32_t shuffleSeed)
    {
        player1_.Reset();
        player2_.Reset();
        buildingDeck_.assign(kBuildingCards.begin(), kBuildingCards.end());
        std::mt19937 randomEngine(shuffleSeed);
        std::shuffle(buildingDeck_.begin(), buildingDeck_.end(), randomEngine);
        discardedBuildings_.clear();
        roundHistory_.clear();
        nextBuildingIndex_ = 0;
        currentRound_ = 1;
        currentBuilding_ = 0;
        player1Bid_.reset();
        lastRoundResult_ = {};
        finalResult_ = {};
        StartCurrentRound();
    }

    void Game::StartCurrentRound()
    {
        currentBuilding_ = buildingDeck_.at(nextBuildingIndex_);
        ++nextBuildingIndex_;
        player1Bid_.reset();
        player1_.ClearSelection();
        player2_.ClearSelection();
        state_ = GameState::Player1Selecting;
    }

    void Game::FinishCurrentRound(int player2Bid)
    {
        const int player1Bid = *player1Bid_;
        lastRoundResult_ = ResolveRound(currentBuilding_, player1Bid, player2Bid);
        roundHistory_.push_back(lastRoundResult_);

        player1_.ConsumeMoneyCard(player1Bid);
        player2_.ConsumeMoneyCard(player2Bid);

        if (lastRoundResult_.winner == RoundWinner::Player1)
        {
            player1_.AddBuilding(currentBuilding_);
        }
        else if (lastRoundResult_.winner == RoundWinner::Player2)
        {
            player2_.AddBuilding(currentBuilding_);
        }
        else
        {
            discardedBuildings_.push_back(currentBuilding_);
        }

        state_ = GameState::RoundResult;
    }
}
