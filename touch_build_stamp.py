# PlatformIO pre-build script: delete BuildStamp.cpp's object file so it is recompiled
# every build and the build date/time on the remote's debug screen is current.
# (Touching the source is not enough: SCons decides by content, not timestamp.)
Import("env")
import glob, os

for obj in glob.glob(os.path.join(env.subst("$BUILD_DIR"), "**", "BuildStamp.cpp.o"), recursive=True):
    os.remove(obj)
