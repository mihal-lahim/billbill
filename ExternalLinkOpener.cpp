#include "ExternalLinkOpener.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>

namespace townmaker
{
    bool Win32ExternalLinkOpener::Open(std::wstring_view url) const
    {
        const std::wstring value(url);
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", value.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return result > 32;
    }
}
