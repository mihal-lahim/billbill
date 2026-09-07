#include "BattleDataWriter.h"
#include "Game.h"
#include "Settings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>

namespace
{
    template<typename State>
    concept ExposesOpponentCurrentBid = requires(State state)
    {
        state.opponentCurrentBid;
    };

    static_assert(!ExposesOpponentCurrentBid<townmaker::CpuGameState>,
        "CPU public state must never expose the opponent's current bid");

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    std::size_t CountOccurrences(const std::string& text, const std::string& pattern)
    {
        std::size_t count = 0;
        std::size_t position = 0;
        while ((position = text.find(pattern, position)) != std::string::npos)
        {
            ++count;
            position += pattern.size();
        }
        return count;
    }

    void TestRoundResolution()
    {
        using namespace townmaker;
        Require(ResolveRound(12, 15, 13).winner == RoundWinner::Player1,
            "15 vs 13 for building 12 must award Player1");
        Require(ResolveRound(14, 17, 9).winner == RoundWinner::Player1,
            "17 vs 9 for building 14 must award Player1");
        Require(ResolveRound(18, 5, 13).winner == RoundWinner::None,
            "5 vs 13 for building 18 must discard the building");
        Require(ResolveRound(10, 11, 11).winner == RoundWinner::None,
            "equal bids must discard the building");
        Require(ResolveRound(18, 21, 23).winner == RoundWinner::Player2,
            "21 vs 23 for building 18 must award Player2");
    }

    void TestSettingsPersistence()
    {
        using namespace townmaker;
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "BillBillSettingsTest.cfg";
        FileSettingsStore store(path);
        Settings saved;
        saved.masterVolume = 37;
        saved.muted = true;
        Require(store.Save(saved), "settings must be saved");
        const Settings loaded = store.Load();
        Require(loaded.masterVolume == 37, "master volume must persist");
        Require(loaded.muted, "mute state must persist");

        std::ofstream invalid(path, std::ios::trunc);
        invalid << "masterVolume 500\nmuted 0\n";
        invalid.close();
        const Settings clamped = store.Load();
        Require(clamped.masterVolume == 100, "loaded volume must be clamped");
        Require(!clamped.muted, "mute off must load correctly");
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    void TestSelectionAndConsumption()
    {
        using namespace townmaker;
        Game game;
        game.StartNewGame(GameMode::PlayerVsPlayer, CpuDifficulty::Normal, 12345);

        Require(!game.CanConfirmSelection(), "confirm must be disabled before selection");
        Require(game.SelectMoneyCard(1), "Player1 must be able to select an available card");
        Require(game.CanConfirmSelection(), "confirm must be enabled after selection");
        Require(game.ConfirmSelection(), "Player1 confirmation must succeed");
        Require(game.State() == GameState::Player2Selecting, "turn must move to Player2");
        Require(std::find(game.Player1().MoneyCards().begin(), game.Player1().MoneyCards().end(), 1)
            != game.Player1().MoneyCards().end(),
            "Player1 bid must remain in the public hand until both players confirm");
        Require(!game.Player1().SelectedMoneyCard().has_value(),
            "Player1 confirmed choice must not remain visibly selected");

        Require(game.SelectMoneyCard(3), "Player2 must be able to select an available card");
        Require(game.ConfirmSelection(), "Player2 confirmation must resolve the round");
        Require(game.State() == GameState::RoundResult, "round must move to result state");
        Require(game.Player1().UsedMoneyCards().size() == 1, "Player1 bid must be consumed");
        Require(game.Player2().UsedMoneyCards().size() == 1, "Player2 bid must be consumed");
        Require(game.Player1().MoneyCards().size() == 11, "Player1 must have 11 cards after one round");
        Require(game.Player2().MoneyCards().size() == 11, "Player2 must have 11 cards after one round");

        Require(game.AdvanceAfterRound(), "next round must start");
        Require(!game.SelectMoneyCard(1), "a used money card must not be selectable again");
    }

    void TestCompleteGameAndFinalResult()
    {
        using namespace townmaker;
        Game game;
        game.StartNewGame(GameMode::PlayerVsPlayer, CpuDifficulty::Normal, 98765);

        for (int round = 1; round <= 10; ++round)
        {
            Require(game.CurrentRound() == round, "round counter must advance exactly once");
            const int player1Bid = game.Player1().MoneyCards().front();
            Require(game.SelectMoneyCard(player1Bid), "Player1 selection must succeed");
            Require(game.ConfirmSelection(), "Player1 confirmation must succeed");

            const int player2Bid = game.Player2().MoneyCards().front();
            Require(game.SelectMoneyCard(player2Bid), "Player2 selection must succeed");
            Require(game.ConfirmSelection(), "Player2 confirmation must succeed");
            Require(game.State() == GameState::RoundResult, "each round must show its result");
            Require(game.AdvanceAfterRound(), "result screen must advance");
        }

        Require(game.State() == GameState::GameResult, "ten rounds must end the game");
        Require(game.Player1().MoneyCards().size() == 2, "Player1 must retain two cards");
        Require(game.Player2().MoneyCards().size() == 2, "Player2 must retain two cards");
        Require(game.Player1().UsedMoneyCards().size() == 10, "Player1 must use ten cards");
        Require(game.Player2().UsedMoneyCards().size() == 10, "Player2 must use ten cards");
        Require(game.DiscardedBuildings().size() == 10, "ten equal bids must discard all buildings");
        Require(game.RoundHistory().size() == 10, "battle history must contain all ten rounds");

        const FinalResult& result = game.FinalGameResult();
        const int expectedPlayer1Money = std::accumulate(
            game.Player1().MoneyCards().begin(), game.Player1().MoneyCards().end(), 0);
        const int expectedPlayer2Money = std::accumulate(
            game.Player2().MoneyCards().begin(), game.Player2().MoneyCards().end(), 0);
        Require(result.player1.buildingCount == 0 && result.player2.buildingCount == 0,
            "equal bids must produce zero acquired buildings");
        Require(result.player1.remainingMoneyTotal == expectedPlayer1Money,
            "Player1 remaining money total must match the remaining hand");
        Require(result.player2.remainingMoneyTotal == expectedPlayer2Money,
            "Player2 remaining money total must match the remaining hand");
        Require(result.buildingCountWinner == CategoryWinner::None,
            "equal building counts must be a category draw");
        Require(result.buildingTotalWinner == CategoryWinner::None,
            "equal building totals must be a category draw");
        Require(result.remainingMoneyWinner == CategoryWinner::None,
            "equal remaining money must be a category draw");
        Require(result.overallWinner == CategoryWinner::None,
            "equal category points must end the game in a draw");

        const std::string json = BattleDataWriter::Serialize(game, "2026-09-04T12:34:56.789");
        Require(json.find("\"formatVersion\": 1") != std::string::npos,
            "battle data must include a format version");
        Require(json.find("\"mode\": \"PlayerVsPlayer\"") != std::string::npos,
            "battle data must include the selected mode");
        Require(json.find("\"cpuDifficulty\": null") != std::string::npos,
            "PvP battle data must not report a CPU difficulty");
        Require(CountOccurrences(json, "\"round\":") == 10,
            "battle data must serialize all ten rounds");
        Require(json.find("\"remainingMoneyCards\": [21, 23]") != std::string::npos,
            "battle data must include final remaining cards");

        const std::filesystem::path testDirectory =
            std::filesystem::temp_directory_path() / "TownMakerBattleDataWriterTests";
        const BattleDataSaveResult saveResult = BattleDataWriter{}.Save(game, testDirectory);
        Require(saveResult.succeeded && std::filesystem::exists(saveResult.filePath),
            "a completed battle must be saved as a separate file");
        const BattleDataSaveResult secondSaveResult = BattleDataWriter{}.Save(game, testDirectory);
        Require(secondSaveResult.succeeded && secondSaveResult.filePath != saveResult.filePath,
            "each battle save must use a distinct file name");
        std::ifstream savedFile(saveResult.filePath, std::ios::binary);
        const std::string savedJson(
            (std::istreambuf_iterator<char>(savedFile)),
            std::istreambuf_iterator<char>());
        Require(CountOccurrences(savedJson, "\"round\":") == 10,
            "the saved battle file must contain all rounds");
        std::error_code cleanupError;
        std::filesystem::remove(saveResult.filePath, cleanupError);
        std::filesystem::remove(secondSaveResult.filePath, cleanupError);
        std::filesystem::remove(testDirectory, cleanupError);
    }

    void TestNonTieFinalAggregation()
    {
        using namespace townmaker;
        Game game;
        game.StartNewGame(GameMode::PlayerVsPlayer, CpuDifficulty::Normal, 24680);

        for (int round = 0; round < 10; ++round)
        {
            Require(game.SelectMoneyCard(game.Player1().MoneyCards().back()),
                "Player1 descending selection must succeed");
            Require(game.ConfirmSelection(), "Player1 confirmation must succeed");
            Require(game.SelectMoneyCard(game.Player2().MoneyCards().front()),
                "Player2 ascending selection must succeed");
            Require(game.ConfirmSelection(), "Player2 confirmation must succeed");
            Require(game.AdvanceAfterRound(), "round result must advance");
        }

        const FinalResult& result = game.FinalGameResult();
        const int player1BuildingTotal = std::accumulate(
            game.Player1().Buildings().begin(), game.Player1().Buildings().end(), 0);
        const int player2BuildingTotal = std::accumulate(
            game.Player2().Buildings().begin(), game.Player2().Buildings().end(), 0);
        const int player1MoneyTotal = std::accumulate(
            game.Player1().MoneyCards().begin(), game.Player1().MoneyCards().end(), 0);
        const int player2MoneyTotal = std::accumulate(
            game.Player2().MoneyCards().begin(), game.Player2().MoneyCards().end(), 0);

        Require(result.player1.buildingCount == static_cast<int>(game.Player1().Buildings().size()),
            "Player1 building count must match acquired cards");
        Require(result.player2.buildingCount == static_cast<int>(game.Player2().Buildings().size()),
            "Player2 building count must match acquired cards");
        Require(result.player1.buildingTotal == player1BuildingTotal,
            "Player1 building total must match acquired cards");
        Require(result.player2.buildingTotal == player2BuildingTotal,
            "Player2 building total must match acquired cards");
        Require(result.player1.remainingMoneyTotal == player1MoneyTotal,
            "Player1 remaining total must match the final hand");
        Require(result.player2.remainingMoneyTotal == player2MoneyTotal,
            "Player2 remaining total must match the final hand");
        Require(result.remainingMoneyWinner == CategoryWinner::Player2,
            "ascending bids must leave Player2 with the larger remaining-money total");

        const int awardedPoints = result.player1.points + result.player2.points;
        const int drawnCategories =
            (result.buildingCountWinner == CategoryWinner::None ? 1 : 0) +
            (result.buildingTotalWinner == CategoryWinner::None ? 1 : 0) +
            (result.remainingMoneyWinner == CategoryWinner::None ? 1 : 0);
        Require(awardedPoints + drawnCategories == 3,
            "every category must award one point or be recorded as a draw");
    }

    townmaker::CpuGameState MakeCpuState(int building)
    {
        townmaker::CpuGameState state;
        state.currentBuilding = building;
        state.round = 2;
        state.totalRounds = 10;
        state.remainingRounds = 9;
        state.cpuMoneyCards = { 1, 3, 5, 7, 17, 19, 21, 23 };
        state.opponentMoneyCards = { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23 };
        return state;
    }

    void TestModeSelection()
    {
        using namespace townmaker;
        Game game;
        Require(game.State() == GameState::ModeSelection,
            "a new game must begin at mode selection");
        Require(game.SelectGameMode(GameMode::PlayerVsCpu),
            "CPU mode must be selectable");
        Require(game.State() == GameState::CpuDifficultySelection,
            "CPU mode must proceed to difficulty selection");
        Require(game.SelectCpuDifficulty(CpuDifficulty::Hard),
            "Hard difficulty must be selectable");
        Require(game.State() == GameState::Player1Selecting,
            "difficulty selection must start the match");
        Require(game.IsCpuGame() && game.Difficulty() == CpuDifficulty::Hard,
            "selected CPU settings must be retained");

        game.ShowModeSelection();
        Require(game.SelectGameMode(GameMode::PlayerVsPlayer),
            "PvP mode must remain selectable");
        Require(game.State() == GameState::Player1Selecting && !game.IsCpuGame(),
            "PvP selection must start the original local match flow");
    }

    void TestCpuDifficultyBehavior()
    {
        using namespace townmaker;
        const CpuGameState randomState = MakeCpuState(10);
        CpuPlayer easyCpu(1001);
        std::set<int> easySelections;
        for (int attempt = 0; attempt < 40; ++attempt)
        {
            const int selected = easyCpu.SelectCard(randomState, CpuDifficulty::Easy);
            Require(std::find(
                randomState.cpuMoneyCards.begin(),
                randomState.cpuMoneyCards.end(),
                selected) != randomState.cpuMoneyCards.end(),
                "Easy CPU must only choose a remaining card");
            easySelections.insert(selected);
        }
        Require(easySelections.size() > 1, "Easy CPU must make random selections");

        CpuPlayer normalCheapCpu(2002);
        CpuPlayer normalExpensiveCpu(2002);
        const int cheapSelection = normalCheapCpu.SelectCard(MakeCpuState(4), CpuDifficulty::Normal);
        const int expensiveSelection = normalExpensiveCpu.SelectCard(MakeCpuState(18), CpuDifficulty::Normal);
        Require(expensiveSelection > cheapSelection,
            "Normal CPU must generally spend more on an expensive building");

        CpuPlayer hardCpu(3003);
        const CpuGameState cheapState = MakeCpuState(4);
        int highestCardSelections = 0;
        for (int attempt = 0; attempt < 30; ++attempt)
        {
            if (hardCpu.SelectCard(cheapState, CpuDifficulty::Hard) == 23)
            {
                ++highestCardSelections;
            }
        }
        Require(highestCardSelections < 30,
            "Hard CPU must not always waste its highest card on a cheap building");

        CpuWeights remainingMoneyWeights;
        remainingMoneyWeights.acquireBuilding = 0.0;
        remainingMoneyWeights.buildingValue = 0.0;
        remainingMoneyWeights.buildingCountImportance = 0.0;
        remainingMoneyWeights.buildingTotalImportance = 0.0;
        remainingMoneyWeights.remainingMoney = 20.0;
        remainingMoneyWeights.highCardPreservation = 0.0;
        remainingMoneyWeights.overpayPenalty = 0.0;
        remainingMoneyWeights.randomness = 0.0;
        CpuPlayer saverCpu(4004, remainingMoneyWeights);
        CpuGameState savingState = MakeCpuState(20);
        savingState.cpuMoneyCards = { 1, 3, 23 };
        Require(saverCpu.SelectCard(savingState, CpuDifficulty::Hard) == 1,
            "Hard evaluation must be able to prioritize final remaining money");
    }

    void TestCompleteCpuGame(townmaker::CpuDifficulty difficulty, std::uint32_t seed)
    {
        using namespace townmaker;
        Game game;
        CpuPlayer cpu(seed + 1);
        game.StartNewGame(GameMode::PlayerVsCpu, difficulty, seed);

        for (int round = 1; round <= 10; ++round)
        {
            const int humanBid = game.Player1().MoneyCards().front();
            Require(game.SelectMoneyCard(humanBid), "Human selection must succeed");
            Require(game.ConfirmSelection(), "Human confirmation must succeed");
            Require(game.State() == GameState::Player2Selecting,
                "CPU turn must begin after Human confirmation");
            Require(!game.CanConfirmSelection() && !game.SelectMoneyCard(game.Player2().MoneyCards().front()),
                "CPU mode must not expose a manual Player2 selection path");

            const CpuGameState publicState = game.CreateCpuGameState();
            Require(std::find(
                publicState.opponentMoneyCards.begin(),
                publicState.opponentMoneyCards.end(),
                humanBid) != publicState.opponentMoneyCards.end(),
                "Human's committed card must remain hidden among the public hand");
            const int cpuBid = cpu.SelectCard(publicState, difficulty);
            Require(std::find(
                publicState.cpuMoneyCards.begin(),
                publicState.cpuMoneyCards.end(),
                cpuBid) != publicState.cpuMoneyCards.end(),
                "CPU must select exactly one available card");
            Require(game.SubmitCpuBid(cpuBid), "a valid CPU bid must resolve the round");
            Require(game.Player2().UsedMoneyCards().back() == cpuBid,
                "the selected CPU card must become used");
            Require(game.State() == GameState::RoundResult,
                "CPU selection must proceed directly to the round result");
            Require(game.AdvanceAfterRound(), "CPU round result must advance");
        }

        Require(game.State() == GameState::GameResult,
            "a CPU game must finish normally after ten rounds");
        Require(game.Player1().MoneyCards().size() == 2 && game.Player2().MoneyCards().size() == 2,
            "both sides must retain two cards after a CPU game");
        Require(game.Player2().UsedMoneyCards().size() == 10,
            "CPU must use exactly ten cards");
        Require(game.RoundHistory().size() == 10,
            "CPU battle history must contain all ten rounds");
        const std::set<int> uniqueCpuCards(
            game.Player2().UsedMoneyCards().begin(),
            game.Player2().UsedMoneyCards().end());
        Require(uniqueCpuCards.size() == 10,
            "CPU must never reuse a consumed card");

        const std::string cpuJson = BattleDataWriter::Serialize(
            game,
            "2026-09-04T12:34:56.789");
        Require(cpuJson.find("\"mode\": \"PlayerVsCpu\"") != std::string::npos,
            "CPU battle data must record CPU mode");
        const char* difficultyName = difficulty == CpuDifficulty::Easy
            ? "Easy"
            : difficulty == CpuDifficulty::Normal ? "Normal" : "Hard";
        Require(cpuJson.find(std::string("\"cpuDifficulty\": \"") + difficultyName + '"') !=
            std::string::npos,
            "CPU battle data must record the selected difficulty");
    }
}

int main()
{
    try
    {
        TestRoundResolution();
        TestSettingsPersistence();
        TestSelectionAndConsumption();
        TestCompleteGameAndFinalResult();
        TestNonTieFinalAggregation();
        TestModeSelection();
        TestCpuDifficultyBehavior();
        TestCompleteCpuGame(townmaker::CpuDifficulty::Easy, 5010);
        TestCompleteCpuGame(townmaker::CpuDifficulty::Normal, 5020);
        TestCompleteCpuGame(townmaker::CpuDifficulty::Hard, 5030);
        std::cout << "All game-logic tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
