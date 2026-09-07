#pragma once

#include <filesystem>

namespace townmaker
{
    struct Settings
    {
        int masterVolume = 100;
        bool muted = false;
    };

    class ISettingsStore
    {
    public:
        virtual ~ISettingsStore() = default;
        virtual Settings Load() const = 0;
        virtual bool Save(const Settings& settings) const = 0;
    };

    class FileSettingsStore final : public ISettingsStore
    {
    public:
        explicit FileSettingsStore(std::filesystem::path filePath);
        Settings Load() const override;
        bool Save(const Settings& settings) const override;

    private:
        std::filesystem::path filePath_;
    };
}
