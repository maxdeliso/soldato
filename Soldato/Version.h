#pragma once

// Global version definitions for Soldato
// These can be easily synchronized with git tags and project metadata
//
// Version Scheme:
// - MAJOR: Breaking changes, major feature releases
// - MINOR: New features, backward compatible
// - PATCH: Bug fixes, minor improvements
// - BUILD: Build number (auto-incremented by CI/CD)

#define SOLDATO_VERSION_MAJOR 0
#define SOLDATO_VERSION_MINOR 1
#define SOLDATO_VERSION_PATCH 1
#define SOLDATO_VERSION_BUILD 0

// Build version string macros
#define SOLDATO_VERSION_STRINGIFY(x) #x
#define SOLDATO_VERSION_CONCAT(major, minor, patch, build) \
    SOLDATO_VERSION_STRINGIFY(major) "." \
    SOLDATO_VERSION_STRINGIFY(minor) "." \
    SOLDATO_VERSION_STRINGIFY(patch) "." \
    SOLDATO_VERSION_STRINGIFY(build)

#define SOLDATO_VERSION_STRING \
    SOLDATO_VERSION_CONCAT(SOLDATO_VERSION_MAJOR, SOLDATO_VERSION_MINOR, SOLDATO_VERSION_PATCH, SOLDATO_VERSION_BUILD)

// Version info for Windows resources (major.minor.patch.build)
#define SOLDATO_VERSION_INFO \
    SOLDATO_VERSION_MAJOR, SOLDATO_VERSION_MINOR, SOLDATO_VERSION_PATCH, SOLDATO_VERSION_BUILD

// Display version string (without build number for user-facing display)
#define SOLDATO_DISPLAY_VERSION_STRING \
    SOLDATO_VERSION_STRINGIFY(SOLDATO_VERSION_MAJOR) "." \
    SOLDATO_VERSION_STRINGIFY(SOLDATO_VERSION_MINOR) "." \
    SOLDATO_VERSION_STRINGIFY(SOLDATO_VERSION_PATCH)

// Full version string for display
#define SOLDATO_FULL_VERSION_STRING "Soldato Version " SOLDATO_DISPLAY_VERSION_STRING

// Human-friendly build string with additional info
#define SOLDATO_BUILD_INFO_STRING \
    "Build " SOLDATO_VERSION_STRINGIFY(SOLDATO_VERSION_BUILD)

// Full about string combining version and build info
#define SOLDATO_ABOUT_VERSION_STRING \
    SOLDATO_FULL_VERSION_STRING "\n" SOLDATO_BUILD_INFO_STRING

// Git tag format (used by scripts)
#define SOLDATO_GIT_TAG_FORMAT "v" SOLDATO_DISPLAY_VERSION_STRING

// Product information
#define SOLDATO_PRODUCT_NAME "Soldato"
#define SOLDATO_COMPANY_NAME "Soldato Project"
#define SOLDATO_DESCRIPTION "A secure peer-to-peer chat application"
