#pragma once

#include <filesystem>
#include <string>

namespace townmaker
{
    enum class SoundEffect
    {
        Click,
        BuildingAcquired,
        BuildingRevealed
    };

    class SoundManager
    {
    public:
        explicit SoundManager(const std::filesystem::path& soundDirectory);
        ~SoundManager();

        SoundManager(const SoundManager&) = delete;
        SoundManager& operator=(const SoundManager&) = delete;

        void Play(SoundEffect effect) const noexcept;
        void SetMasterVolume(int volumePercent, bool muted) noexcept;

    private:
        struct Sound
        {
            std::wstring alias;
            bool loaded = false;
        };

        static Sound Load(const std::filesystem::path& path, const wchar_t* alias) noexcept;
        static void Close(const Sound& sound) noexcept;
        static void PlayLoaded(const Sound& sound) noexcept;
        void ApplyVolume(const Sound& sound) const noexcept;

        Sound click_;
        Sound buildingAcquired_;
        Sound buildingRevealed_;
        int volumePercent_ = 100;
        bool muted_ = false;
    };
}
