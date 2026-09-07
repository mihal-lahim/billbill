#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace townmaker
{
    enum class GameMode
    {
        PlayerVsPlayer,
        PlayerVsCpu
    };

    enum class CpuDifficulty
    {
        Easy,
        Normal,
        Hard
    };

    struct CpuGameState
    {
        int currentBuilding = 0;
        int round = 0;
        int totalRounds = 10;
        int remainingRounds = 0;

        std::vector<int> cpuMoneyCards;
        std::vector<int> opponentMoneyCards;
        std::vector<int> cpuBuildings;
        std::vector<int> opponentBuildings;
        std::vector<int> cpuUsedMoneyCards;
        std::vector<int> opponentUsedMoneyCards;
        std::vector<int> discardedBuildings;
    };

    struct CpuWeights
    {
        double acquireBuilding = 5.0;
        double buildingValue = 4.0;
        double buildingCountImportance = 1.4;
        double buildingTotalImportance = 1.0;
        double remainingMoney = 2.8;
        double highCardPreservation = 1.4;
        double overpayPenalty = 2.4;
        double randomness = 0.18;
    };

    class CpuPlayer
    {
    public:
        CpuPlayer();
        explicit CpuPlayer(std::uint32_t randomSeed, CpuWeights weights = {});

        int SelectCard(const CpuGameState& state, CpuDifficulty difficulty);

    private:
        int SelectEasy(const CpuGameState& state);
        int SelectNormal(const CpuGameState& state);
        int SelectHard(const CpuGameState& state);
        int SelectRandom(const std::vector<int>& cards);

        std::mt19937 randomEngine_;
        CpuWeights weights_;
    };
}
