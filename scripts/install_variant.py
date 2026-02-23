"""
Pre-build script: install the nice_nano variant into the Adafruit nRF52
Arduino framework package so PlatformIO can find it at compile time.
"""

Import("env")
import os
import shutil

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoadafruitnrf52")
variant_src = os.path.join(env.get("PROJECT_DIR"), "variants", "nice_nano")
variant_dst = os.path.join(framework_dir, "variants", "nice_nano")

if not os.path.exists(variant_dst):
    print("Installing nice_nano variant into Adafruit nRF52 framework...")
    shutil.copytree(variant_src, variant_dst)
else:
    # Update if source files are newer
    for fname in ("variant.h", "variant.cpp"):
        src = os.path.join(variant_src, fname)
        dst = os.path.join(variant_dst, fname)
        if os.path.getmtime(src) > os.path.getmtime(dst):
            print(f"Updating {fname} in framework variant directory...")
            shutil.copy2(src, dst)
