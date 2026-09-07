#include "SoundManager.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace townmaker
{
    namespace
    {
        std::wstring Quote(const std::filesystem::path& path)
        {
            return L"\"" + path.wstring() + L"\"";
        }
    }

    SoundManager::SoundManager(const std::filesystem::path& soundDirectory)
        : click_(Load(soundDirectory / L"kettei.mp3", L"bill_click")),
          buildingAcquired_(Load(soundDirectory / L"billGet.mp3", L"bill_get")),
          buildingRevealed_(Load(soundDirectory / L"opencard.mp3", L"bill_open"))
    {
    }

    SoundManager::~SoundManager()
    {
        Close(click_);
        Close(buildingAcquired_);
        Close(buildingRevealed_);
    }

    void SoundManager::Play(SoundEffect effect) const noexcept
    {
        switch (effect)
        {
        case SoundEffect::Click:
            PlayLoaded(click_);
            break;
        case SoundEffect::BuildingAcquired:
            PlayLoaded(buildingAcquired_);
            break;
        case SoundEffect::BuildingRevealed:
            PlayLoaded(buildingRevealed_);
            break;
        }
    }

    void SoundManager::SetMasterVolume(int volumePercent, bool muted) noexcept
    {
        volumePercent_ = (std::max)(0, (std::min)(100, volumePercent));
        muted_ = muted;
        ApplyVolume(click_);
        ApplyVolume(buildingAcquired_);
        ApplyVolume(buildingRevealed_);
    }

    SoundManager::Sound SoundManager::Load(
        const std::filesystem::path& path,
        const wchar_t* alias) noexcept
    {
        Sound sound{ alias, false };
        if (!std::filesystem::exists(path))
        {
            return sound;
        }

        const std::wstring command =
            L"open " + Quote(path) + L" type mpegvideo alias " + sound.alias;
        sound.loaded = mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0;
        return sound;
    }

    void SoundManager::Close(const Sound& sound) noexcept
    {
        if (sound.loaded)
        {
            const std::wstring command = L"close " + sound.alias;
            mciSendStringW(command.c_str(), nullptr, 0, nullptr);
        }
    }

    void SoundManager::PlayLoaded(const Sound& sound) noexcept
    {
        if (!sound.loaded)
        {
            return;
        }

        const std::wstring stopCommand = L"stop " + sound.alias;
        const std::wstring seekCommand = L"seek " + sound.alias + L" to start";
        const std::wstring playCommand = L"play " + sound.alias;
        mciSendStringW(stopCommand.c_str(), nullptr, 0, nullptr);
        mciSendStringW(seekCommand.c_str(), nullptr, 0, nullptr);
        mciSendStringW(playCommand.c_str(), nullptr, 0, nullptr);
    }

    void SoundManager::ApplyVolume(const Sound& sound) const noexcept
    {
        if (!sound.loaded)
        {
            return;
        }
        const int effectiveVolume = muted_ ? 0 : volumePercent_ * 10;
        const std::wstring command =
            L"setaudio " + sound.alias + L" volume to " + std::to_wstring(effectiveVolume);
        mciSendStringW(command.c_str(), nullptr, 0, nullptr);
    }
}
