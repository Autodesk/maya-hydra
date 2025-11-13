//
// Copyright 2025 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <mayaHydraLib/envUtils.h>

#include <cstdlib>
#include <vector>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

namespace MAYAHYDRA_NS_DEF {

static inline bool isAscii(const char* s) {
    if (s == nullptr) return false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
        if (*p > 0x7F) return false;
    }
    return true;
}

bool tryGetEnvUtf8(const char* name, std::string& outValue)
{
    outValue.clear();
    if (name == nullptr || *name == '\0') {
        return false;
    }

#if defined(_WIN32)
    // Use wide WinAPI to avoid static buffers and ACP codepage issues.
    // Convert name (assumed ASCII) to wide.
    std::wstring wname;
    if (isAscii(name)) {
        wname.assign(name, name + std::strlen(name));
    } else {
        // If non-ASCII appears in name, fail fast (env var names should be ASCII).
        return false;
    }

    // Query required size (includes terminating null).
    DWORD required = ::GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (required == 0) {
        return false; // not found
    }

    std::vector<wchar_t> wbuf(static_cast<size_t>(required));
    DWORD written = ::GetEnvironmentVariableW(wname.c_str(), wbuf.data(), required);
    if (written == 0 || written >= required) {
        return false;
    }

    // Convert wide to UTF-8.
    int utf8Size = ::WideCharToMultiByte(
        CP_UTF8, 0, wbuf.data(), static_cast<int>(written), nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) {
        return false;
    }
    std::string utf8;
    utf8.resize(static_cast<size_t>(utf8Size));
    int converted = ::WideCharToMultiByte(
        CP_UTF8, 0, wbuf.data(), static_cast<int>(written),
        utf8.data(), utf8Size, nullptr, nullptr);
    if (converted != utf8Size) {
        return false;
    }
    outValue.swap(utf8);
    return true;
#else
    // POSIX: getenv returns pointer to static storage; copy to our own buffer to avoid hazards.
    const char* v = std::getenv(name);
    if (!v) {
        return false;
    }
    outValue.assign(v);
    return true;
#endif
}

std::string getEnvUtf8Or(const char* name, std::string_view fallback)
{
    std::string value;
    if (tryGetEnvUtf8(name, value)) {
        return value;
    }
    return std::string(fallback);
}

} // namespace MAYAHYDRA_NS_DEF


