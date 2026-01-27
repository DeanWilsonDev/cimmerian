#pragma once
#include <string_view>

inline bool g_UmbraTestDebugEnabled = false;

inline void CheckDebug(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-d" || arg == "--debug") {
            g_UmbraTestDebugEnabled = true;
        }
    }
}
