#include "DebugUtils.h"

// Static member definitions
std::ofstream DebugLogger::m_logFile;
std::mutex DebugLogger::m_mutex;
bool DebugLogger::m_initialized = false;
