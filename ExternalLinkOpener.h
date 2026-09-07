#pragma once

#include <string_view>

namespace townmaker
{
    class IExternalLinkOpener
    {
    public:
        virtual ~IExternalLinkOpener() = default;
        virtual bool Open(std::wstring_view url) const = 0;
    };

    class Win32ExternalLinkOpener final : public IExternalLinkOpener
    {
    public:
        bool Open(std::wstring_view url) const override;
    };
}
