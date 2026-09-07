#include "CpuPlayer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace townmaker
{
    namespace
    {
        struct NormalCpuSettings
        {
            int cheapBuildingLimit = 6;
            int expensiveBuildingLimit = 14;
            int excessiveOverpay = 6;
            double cheapBuildingGiveUpChance = 0.35;
            double alternateCandidateChance = 0.28;
        };

        constexpr NormalCpuSettings kNormalSettings;

        int Sum(const std::vector<int>& values) noexcept
        {
            return std::accumulate(values.begin(), values.end(), 0);
        }

        double OpponentBidWeight(int bid, int building) noexcept
        {
            if (bid > building)
            {
                return 1.4 / (1.0 + static_cast<double>(bid - building) * 0.13);
            }
            return 0.9 / (1.0 + static_cast<double>(building - bid) * 0.04);
        }

        double EstimateAcquireProbability(int candidate, const CpuGameState& state) noexcept
        {
            if (candidate <= state.currentBuilding)
            {
                return 0.0;
            }
            if (state.opponentMoneyCards.empty())
            {
                return 1.0;
            }

            double totalWeight = 0.0;
            double winningWeight = 0.0;
            for (const int opponentBid : state.opponentMoneyCards)
            {
                const double weight = OpponentBidWeight(opponentBid, state.currentBuilding);
                totalWeight += weight;
                if (candidate > opponentBid)
                {
                    winningWeight += weight;
                }
            }
            return totalWeight > 0.0 ? winningWeight / totalWeight : 0.0;
        }

        std::vector<int> SortedCards(const std::vector<int>& cards)
        {
            std::vector<int> sorted = cards;
            std::sort(sorted.begin(), sorted.end());
            return sorted;
        }
    }

    CpuPlayer::CpuPlayer()
        : randomEngine_(std::random_device{}())
    {
    }

    CpuPlayer::CpuPlayer(std::uint32_t randomSeed, CpuWeights weights)
        : randomEngine_(randomSeed), weights_(weights)
    {
    }

    int CpuPlayer::SelectCard(const CpuGameState& state, CpuDifficulty difficulty)
    {
        if (state.cpuMoneyCards.empty())
        {
            throw std::invalid_argument("CPU cannot select from an empty hand");
        }

        switch (difficulty)
        {
        case CpuDifficulty::Easy:
            return SelectEasy(state);
        case CpuDifficulty::Normal:
            return SelectNormal(state);
        case CpuDifficulty::Hard:
            return SelectHard(state);
        }
        return SelectEasy(state);
    }

    int CpuPlayer::SelectEasy(const CpuGameState& state)
    {
        return SelectRandom(state.cpuMoneyCards);
    }

    int CpuPlayer::SelectNormal(const CpuGameState& state)
    {
        const std::vector<int> cards = SortedCards(state.cpuMoneyCards);
        std::vector<int> affordableCards;
        std::copy_if(
            cards.begin(),
            cards.end(),
            std::back_inserter(affordableCards),
            [&](int card) { return card > state.currentBuilding; });

        if (affordableCards.empty())
        {
            const std::size_t discardPoolSize = std::max<std::size_t>(1, cards.size() / 3);
            return SelectRandom(std::vector<int>(cards.begin(), cards.begin() + discardPoolSize));
        }

        std::vector<int> discardCards;
        std::copy_if(
            cards.begin(),
            cards.end(),
            std::back_inserter(discardCards),
            [&](int card) { return card <= state.currentBuilding; });

        std::bernoulli_distribution giveUp(kNormalSettings.cheapBuildingGiveUpChance);
        const int smallestPurchase = affordableCards.front();
        const bool inefficientCheapPurchase =
            state.currentBuilding <= kNormalSettings.cheapBuildingLimit &&
            smallestPurchase - state.currentBuilding > kNormalSettings.excessiveOverpay;
        if (!discardCards.empty() &&
            state.currentBuilding <= kNormalSettings.cheapBuildingLimit &&
            (inefficientCheapPurchase || giveUp(randomEngine_)))
        {
            return discardCards.front();
        }

        std::vector<int> opponentPurchaseCards;
        std::copy_if(
            state.opponentMoneyCards.begin(),
            state.opponentMoneyCards.end(),
            std::back_inserter(opponentPurchaseCards),
            [&](int card) { return card > state.currentBuilding; });
        std::sort(opponentPurchaseCards.begin(), opponentPurchaseCards.end());

        int target = state.currentBuilding;
        if (!opponentPurchaseCards.empty())
        {
            const std::size_t estimateIndex = opponentPurchaseCards.size() / 3;
            target = std::max(target, opponentPurchaseCards[estimateIndex]);
        }

        auto preferred = std::find_if(
            affordableCards.begin(),
            affordableCards.end(),
            [&](int card) { return card > target; });
        if (preferred == affordableCards.end())
        {
            preferred = affordableCards.begin();
        }

        const double progress = state.totalRounds > 1
            ? static_cast<double>(state.round - 1) / static_cast<double>(state.totalRounds - 1)
            : 1.0;
        if (state.currentBuilding < kNormalSettings.expensiveBuildingLimit &&
            *preferred - state.currentBuilding > kNormalSettings.excessiveOverpay &&
            !discardCards.empty() && progress < 0.7)
        {
            return discardCards.front();
        }

        std::bernoulli_distribution useAlternate(kNormalSettings.alternateCandidateChance);
        const auto nextCandidate = std::next(preferred);
        if (nextCandidate != affordableCards.end() &&
            state.currentBuilding >= kNormalSettings.expensiveBuildingLimit &&
            useAlternate(randomEngine_))
        {
            return *nextCandidate;
        }
        return *preferred;
    }

    int CpuPlayer::SelectHard(const CpuGameState& state)
    {
        const std::vector<int> cards = SortedCards(state.cpuMoneyCards);
        const int moneyBefore = Sum(cards);
        const int maxCard = cards.back();
        const int minCard = cards.front();
        const int cpuBuildingTotal = Sum(state.cpuBuildings);
        const int opponentBuildingTotal = Sum(state.opponentBuildings);
        const int buildingCountDifference = static_cast<int>(state.opponentBuildings.size()) -
            static_cast<int>(state.cpuBuildings.size());
        const int buildingTotalDifference = opponentBuildingTotal - cpuBuildingTotal;
        const double progress = state.totalRounds > 1
            ? static_cast<double>(state.round - 1) / static_cast<double>(state.totalRounds - 1)
            : 1.0;
        std::uniform_real_distribution<double> jitter(-weights_.randomness, weights_.randomness);

        int bestCard = cards.front();
        double bestScore = -std::numeric_limits<double>::infinity();
        for (const int candidate : cards)
        {
            const double acquireProbability = EstimateAcquireProbability(candidate, state);
            const double normalizedBuildingValue =
                static_cast<double>(state.currentBuilding) / 20.0;
            const double countPressure = std::clamp(
                static_cast<double>(buildingCountDifference), -2.0, 2.0);
            const double totalPressure = std::clamp(
                static_cast<double>(buildingTotalDifference) / 20.0, -2.0, 2.0);
            const double acquireReward =
                weights_.acquireBuilding +
                weights_.buildingValue * normalizedBuildingValue +
                weights_.buildingCountImportance * countPressure +
                weights_.buildingTotalImportance * totalPressure;

            const double remainingRatio = moneyBefore > 0
                ? static_cast<double>(moneyBefore - candidate) / static_cast<double>(moneyBefore)
                : 0.0;
            const double remainingWeight = weights_.remainingMoney * (1.0 + progress * 1.4);
            const double preserveHighCards = maxCard > minCard
                ? static_cast<double>(maxCard - candidate) / static_cast<double>(maxCard - minCard)
                : 0.0;
            const double overpay = candidate > state.currentBuilding
                ? static_cast<double>(candidate - state.currentBuilding) / 22.0
                : 0.0;
            const double cheapBuildingMultiplier = state.currentBuilding <= 6 ? 1.6 : 1.0;
            const double efficientDiscardBonus = acquireProbability < 0.15 && maxCard > minCard
                ? 0.35 * static_cast<double>(maxCard - candidate) /
                    static_cast<double>(maxCard - minCard)
                : 0.0;

            const double score =
                acquireProbability * acquireReward +
                remainingRatio * remainingWeight +
                preserveHighCards * weights_.highCardPreservation -
                overpay * weights_.overpayPenalty * cheapBuildingMultiplier +
                efficientDiscardBonus +
                jitter(randomEngine_);

            if (score > bestScore)
            {
                bestScore = score;
                bestCard = candidate;
            }
        }
        return bestCard;
    }

    int CpuPlayer::SelectRandom(const std::vector<int>& cards)
    {
        std::uniform_int_distribution<std::size_t> distribution(0, cards.size() - 1);
        return cards[distribution(randomEngine_)];
    }
}
