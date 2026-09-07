#pragma once

#include "Game.h"

#include <filesystem>
#include <string>

namespace townmaker
{
    struct BattleDataSaveResult
    {
        bool succeeded = false;
        std::filesystem::path filePath;
        std::wstring errorMessage;
    };

    class BattleDataWriter
    {
    public:
        BattleDataSaveResult Save(
            const Game& game,
            const std::filesystem::path& outputDirectory) const;

        static std::string Serialize(const Game& game, const std::string& savedAtLocal);
    };
}
