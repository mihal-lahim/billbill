#include "CpuPlayer.h"
#include "Game.h"

#include <cstdlib>
#include <cstring>
#include <numeric>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define BB_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define BB_EXPORT
#endif

namespace
{
    townmaker::Game game;
    townmaker::CpuPlayer cpu;

    std::string Array(const std::vector<int>& values)
    {
        std::ostringstream out;
        out << '[';
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i) out << ',';
            out << values[i];
        }
        return out.str() + ']';
    }

    std::string StateJson()
    {
        const auto& p1 = game.Player1();
        const auto& p2 = game.Player2();
        std::ostringstream out;
        out << "{\"state\":" << static_cast<int>(game.State())
            << ",\"round\":" << game.CurrentRound()
            << ",\"building\":" << game.CurrentBuilding()
            << ",\"p1Money\":" << Array(p1.MoneyCards())
            << ",\"p2Money\":" << Array(p2.MoneyCards())
            << ",\"p1Buildings\":" << Array(p1.Buildings())
            << ",\"p2Buildings\":" << Array(p2.Buildings())
            << ",\"discarded\":" << Array(game.DiscardedBuildings());
        if (game.State() == townmaker::GameState::RoundResult)
        {
            const auto& result = game.LastRoundResult();
            out << ",\"result\":{\"building\":" << result.building
                << ",\"p1Bid\":" << result.player1Bid
                << ",\"p2Bid\":" << result.player2Bid
                << ",\"winner\":" << static_cast<int>(result.winner) << '}';
        }
        out << '}';
        return out.str();
    }
}

extern "C"
{
    BB_EXPORT void bb_create() { game.ShowModeSelection(); }

    BB_EXPORT void bb_start(int cpuDifficulty, unsigned seed)
    {
        game.StartNewGame(townmaker::GameMode::PlayerVsCpu,
            static_cast<townmaker::CpuDifficulty>(cpuDifficulty), seed);
    }

    BB_EXPORT int bb_select_card(int value) { return game.SelectMoneyCard(value) ? 1 : 0; }

    BB_EXPORT int bb_confirm()
    {
        if (!game.ConfirmSelection()) return 0;
        if (game.IsCpuGame() && game.State() == townmaker::GameState::Player2Selecting)
        {
            const auto view = game.CreateCpuGameState();
            return game.SubmitCpuBid(cpu.SelectCard(view, game.Difficulty())) ? 1 : 0;
        }
        return 1;
    }

    BB_EXPORT int bb_advance() { return game.AdvanceAfterRound() ? 1 : 0; }
    BB_EXPORT void bb_restart() { game.RestartCurrentGame(); }

    BB_EXPORT char* bb_state_json()
    {
        const std::string json = StateJson();
        char* result = static_cast<char*>(std::malloc(json.size() + 1));
        if (result) std::memcpy(result, json.c_str(), json.size() + 1);
        return result;
    }

    BB_EXPORT void bb_free(char* value) { std::free(value); }
}
