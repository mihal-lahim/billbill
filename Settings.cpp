#include "Settings.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace townmaker
{
    FileSettingsStore::FileSettingsStore(std::filesystem::path filePath)
        : filePath_(std::move(filePath))
    {
    }

    Settings FileSettingsStore::Load() const
    {
        Settings settings;
        std::ifstream input(filePath_);
        std::string key;
        while (input >> key)
        {
            if (key == "masterVolume")
            {
                input >> settings.masterVolume;
            }
            else if (key == "muted")
            {
                int value = 0;
                input >> value;
                settings.muted = value != 0;
            }
        }
        settings.masterVolume = std::clamp(settings.masterVolume, 0, 100);
        return settings;
    }

    bool FileSettingsStore::Save(const Settings& settings) const
    {
        std::error_code error;
        if (filePath_.has_parent_path())
        {
            std::filesystem::create_directories(filePath_.parent_path(), error);
        }
        std::ofstream output(filePath_, std::ios::trunc);
        if (!output)
        {
            return false;
        }
        output << "masterVolume " << std::clamp(settings.masterVolume, 0, 100) << '\n';
        output << "muted " << (settings.muted ? 1 : 0) << '\n';
        return static_cast<bool>(output);
    }
}
