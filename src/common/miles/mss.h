#pragma once

// Miles is sourced from the local SDK bundle checked into src/thirdparty.
#if __has_include("../../thirdparty/MilesSDKWin/include/mss.h")
#include "../../thirdparty/MilesSDKWin/include/mss.h"
#else
#error "Miles SDK header not found at src/thirdparty/MilesSDKWin/include/mss.h"
#endif
