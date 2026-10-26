# PlatformIO pre-build script: touch BuildStamp.cpp so it is recompiled every build
# and the build date/time on the remote's debug screen is current.
Import("env")
import os

os.utime(os.path.join(env.subst("$PROJECT_SRC_DIR"), "BuildStamp.cpp"), None)
