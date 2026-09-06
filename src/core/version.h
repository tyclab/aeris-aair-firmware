#pragma once

// Hand-bumped on release; the build stamp distinguishes two units running
// unreleased work. Only truthful after a clean build, since __DATE__ and
// __TIME__ freeze into whichever translation units actually recompiled.
#define FIRMWARE_VERSION "tyclab v1.1.0+dev"
#define FIRMWARE_BUILD (__DATE__ " " __TIME__)
