#include "BattleDataWriter.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace townmaker
{
    namespace
    {
        std::tm LocalTime(std::time_t time)
        {
            std::tm local{};
            localtime_s(&local, &time);
            return local;
        }

        std::string FormatTimestamp(
            const std::chrono::system_clock::time_point& now,
            const char* format)
        {
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            const std::tm local = LocalTime(time);
            std::ostringstream stream;
            stream << std::put_time(&local, format);
            return stream.str();
        }

        std::string IsoTimestamp(const std::chrono::system_clock::time_point& now)
        {
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            std::ostringstream stream;
            stream << FormatTimestamp(now, "%Y-%m-%dT%H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << milliseconds.count();
            return stream.str();
        }

        const char* ModeName(GameMode mode) noexcept
        {
            return mode == GameMode::PlayerVsCpu ? "PlayerVsCpu" : "PlayerVsPlayer";
        }

        const char* DifficultyName(CpuDifficulty difficulty) noexcept
        {
            switch (difficulty)
            {
            case CpuDifficulty::Easy:
                return "Easy";
            case CpuDifficulty::Normal:
                return "Normal";
            case CpuDifficulty::Hard:
                return "Hard";
            }
            return "Normal";
        }

        const char* RoundWinnerName(RoundWinner winner, bool cpuGame) noexcept
        {
            switch (winner)
            {
            case RoundWinner::Player1:
                return "Player1";
            case RoundWinner::Player2:
                return cpuGame ? "CPU" : "Player2";
            default:
                return "None";
            }
        }

        const char* CategoryWinnerName(CategoryWinner winner, bool cpuGame) noexcept
        {
            switch (winner)
            {
            case CategoryWinner::Player1:
                return "Player1";
            case CategoryWinner::Player2:
                return cpuGame ? "CPU" : "Player2";
            default:
                return "Draw";
            }
        }

        void WriteIntArray(std::ostringstream& stream, const std::vector<int>& values)
        {
            stream << '[';
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    stream << ", ";
                }
                stream << values[index];
            }
            stream << ']';
        }

        void WritePlayer(
            std::ostringstream& stream,
            const Player& player,
            const PlayerSummary& summary,
            const char* id,
            const char* role,
            const char* trailingText)
        {
            stream << "    {\n"
                << "      \"id\": \"" << id << "\",\n"
                << "      \"role\": \"" << role << "\",\n"
                << "      \"remainingMoneyCards\": ";
            WriteIntArray(stream, player.MoneyCards());
            stream << ",\n      \"usedMoneyCards\": ";
            WriteIntArray(stream, player.UsedMoneyCards());
            stream << ",\n      \"acquiredBuildings\": ";
            WriteIntArray(stream, player.Buildings());
            stream << ",\n"
                << "      \"buildingCount\": " << summary.buildingCount << ",\n"
                << "      \"buildingTotal\": " << summary.buildingTotal << ",\n"
                << "      \"remainingMoneyTotal\": " << summary.remainingMoneyTotal << ",\n"
                << "      \"points\": " << summary.points << "\n"
                << "    }" << trailingText;
        }

        std::filesystem::path UniqueFilePath(
            const std::filesystem::path& directory,
            const Game& game,
            const std::chrono::system_clock::time_point& now)
        {
            const std::string modeSuffix = game.IsCpuGame()
                ? std::string("_PvCPU_") + DifficultyName(game.Difficulty())
                : "_PvP";
            const std::string baseName =
                "battle_" + FormatTimestamp(now, "%Y%m%d_%H%M%S") + modeSuffix;

            std::filesystem::path candidate = directory / (baseName + ".json");
            for (int suffix = 2; std::filesystem::exists(candidate); ++suffix)
            {
                candidate = directory / (baseName + '_' + std::to_string(suffix) + ".json");
            }
            return candidate;
        }
    }

    BattleDataSaveResult BattleDataWriter::Save(
        const Game& game,
        const std::filesystem::path& outputDirectory) const
    {
        BattleDataSaveResult result;
        if (game.State() != GameState::GameResult || game.RoundHistory().size() != 10)
        {
            result.errorMessage = L"完了していない対戦データは保存できません。";
            return result;
        }

        try
        {
            std::filesystem::create_directories(outputDirectory);
            const auto now = std::chrono::system_clock::now();
            result.filePath = UniqueFilePath(outputDirectory, game, now);

            std::ofstream output(result.filePath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                result.errorMessage = L"対戦データファイルを開けませんでした。";
                return result;
            }

            output << Serialize(game, IsoTimestamp(now));
            output.flush();
            if (!output)
            {
                result.errorMessage = L"対戦データの書き込みに失敗しました。";
                return result;
            }

            result.succeeded = true;
            return result;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            result.errorMessage = L"BattleDataフォルダの作成またはファイル保存に失敗しました。";
            return result;
        }
    }

    std::string BattleDataWriter::Serialize(const Game& game, const std::string& savedAtLocal)
    {
        const bool cpuGame = game.IsCpuGame();
        const FinalResult& finalResult = game.FinalGameResult();
        std::ostringstream stream;
        stream << "{\n"
            << "  \"formatVersion\": 1,\n"
            << "  \"savedAtLocal\": \"" << savedAtLocal << "\",\n"
            << "  \"mode\": \"" << ModeName(game.Mode()) << "\",\n"
            << "  \"cpuDifficulty\": ";
        if (cpuGame)
        {
            stream << '"' << DifficultyName(game.Difficulty()) << '"';
        }
        else
        {
            stream << "null";
        }
        stream << ",\n  \"rounds\": [\n";

        const std::vector<RoundResult>& rounds = game.RoundHistory();
        for (std::size_t index = 0; index < rounds.size(); ++index)
        {
            const RoundResult& round = rounds[index];
            stream << "    {\"round\": " << index + 1
                << ", \"building\": " << round.building
                << ", \"player1Bid\": " << round.player1Bid
                << ", \"player2Bid\": " << round.player2Bid
                << ", \"winner\": \"" << RoundWinnerName(round.winner, cpuGame) << "\"}";
            stream << (index + 1 == rounds.size() ? "\n" : ",\n");
        }

        stream << "  ],\n  \"discardedBuildings\": ";
        WriteIntArray(stream, game.DiscardedBuildings());
        stream << ",\n  \"players\": [\n";
        WritePlayer(stream, game.Player1(), finalResult.player1, "Player1", "Human", ",\n");
        WritePlayer(
            stream,
            game.Player2(),
            finalResult.player2,
            "Player2",
            cpuGame ? "CPU" : "Human",
            "\n");
        stream << "  ],\n"
            << "  \"categoryWinners\": {\n"
            << "    \"buildingCount\": \""
            << CategoryWinnerName(finalResult.buildingCountWinner, cpuGame) << "\",\n"
            << "    \"buildingTotal\": \""
            << CategoryWinnerName(finalResult.buildingTotalWinner, cpuGame) << "\",\n"
            << "    \"remainingMoney\": \""
            << CategoryWinnerName(finalResult.remainingMoneyWinner, cpuGame) << "\"\n"
            << "  },\n"
            << "  \"overallWinner\": \""
            << CategoryWinnerName(finalResult.overallWinner, cpuGame) << "\"\n"
            << "}\n";
        return stream.str();
    }
}
