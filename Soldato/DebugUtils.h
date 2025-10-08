#pragma once

#ifdef _DEBUG
    #define DEBUG_LOG(msg) OutputDebugStringA(msg)
#else
    #define DEBUG_LOG(msg)
#endif
