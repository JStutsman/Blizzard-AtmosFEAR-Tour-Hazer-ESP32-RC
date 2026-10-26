// BuildStamp.cpp  (Hazer-TX) - compile date and time of this firmware, for the debug screen.
// touch_build_stamp.py deletes its object file before every PlatformIO build so it is always current.
extern const char* const BUILD_STAMP;
const char* const BUILD_STAMP = __DATE__ " " __TIME__;
