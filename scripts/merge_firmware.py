"""
Post-build step: merge the four flash images PlatformIO produces into one
single-file image that can be written to a blank ESP32 at offset 0x0.

  0x1000   bootloader.bin
  0x8000   partitions.bin
  0xe000   boot_app0.bin      (OTA selector - points at app0)
  0x10000  firmware.bin       (the sketch itself)

The SPIFFS partition is deliberately left out so that flashing this image
does NOT erase the clock's saved settings / WiFi credentials / weather cache.
The web page is embedded at build time; there is no filesystem upload step.

The result is copied to ../BIN/ (the NAS firmware folder) if that folder
exists, so a history of every built version accumulates there.

VERSION AUTO-BUMP: `src/main.cpp`'s `float ver = X.XX;` line is the single
source of truth for the version number. After a successful build, this
script reads that value (the version that was actually just compiled),
names/copies this build's .bin after it, then bumps main.cpp, the
platformio.ini header comment, and docs/DEVELOPMENT.md by +0.01 for the NEXT build.
This means every `pio run` (build or upload) advances the version - that is
intentional, not a bug: see HANDOFF.md for the rationale and tradeoffs.
"""

import os
import re
import shutil
from decimal import Decimal

Import("env")  # noqa: F821  (injected by PlatformIO/SCons)

PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
MAIN_CPP = os.path.join(PROJECT_DIR, "src", "main.cpp")
PIO_INI = os.path.join(PROJECT_DIR, "platformio.ini")
README = os.path.join(PROJECT_DIR, "docs", "DEVELOPMENT.md")

VER_RE = re.compile(r"float ver = (\d+\.\d+);")
INI_TITLE_RE = re.compile(r"(;\s*SmartClock )\d+\.\d+( - PlatformIO / VS Code project)")
README_TITLE_RE = re.compile(r"(# SmartClock — firmware \(v)\d+\.\d+(\))")


def read(path):
    with open(path, "r") as f:
        return f.read()


def write(path, text):
    with open(path, "w") as f:
        f.write(text)


def current_version():
    match = VER_RE.search(read(MAIN_CPP))
    if not match:
        raise RuntimeError("Could not find 'float ver = X.XX;' in main.cpp")
    return match.group(1)


def bump_version_for_next_build(old_str):
    old = Decimal(old_str)
    new = old + Decimal("0.01")
    new_str = "{:.2f}".format(new)

    # Targeted regexes, not a blanket string replace: platformio.ini and
    # DEVELOPMENT.md also contains historical prose ("that conversion is what
    # v2.90 is...") that must stay pinned to the version it actually
    # describes rather than being dragged forward on every bump. Only the
    # "current version" title/header lines are live pointers.
    main_text = read(MAIN_CPP)
    write(MAIN_CPP, VER_RE.sub("float ver = %s;" % new_str, main_text, count=1))

    ini_text = read(PIO_INI)
    write(PIO_INI, INI_TITLE_RE.sub(r"\g<1>%s\g<2>" % new_str, ini_text, count=1))

    if os.path.isfile(README):
        readme_text = read(README)
        write(README, README_TITLE_RE.sub(r"\g<1>%s\g<2>" % new_str, readme_text, count=1))

    print("Version bumped: %s -> %s (takes effect next build)" % (old_str, new_str))


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    board = env.BoardConfig()

    version = current_version()  # the version actually compiled into this build

    boot_app0 = os.path.join(
        env.PioPlatform().get_package_dir("framework-arduinoespressif32"),
        "tools", "partitions", "boot_app0.bin",
    )

    out_name = "SmartClock_v%s.bin" % version
    out_path = os.path.join(build_dir, out_name)

    cmd = [
        env.subst("$PYTHONEXE"), env.subst("$OBJCOPY"),
        "--chip", board.get("build.mcu", "esp32"),
        "merge_bin", "-o", out_path,
        "--flash_mode", board.get("build.flash_mode", "qio"),
        "--flash_freq", "40m",
        "--flash_size", board.get("upload.flash_size", "4MB"),
        "0x1000", os.path.join(build_dir, "bootloader.bin"),
        "0x8000", os.path.join(build_dir, "partitions.bin"),
        "0xe000", boot_app0,
        "0x10000", os.path.join(build_dir, "firmware.bin"),
    ]
    env.Execute(env.VerboseAction(" ".join('"%s"' % c for c in cmd),
                                  "Merging single-file image -> %s" % out_name))

    # Mirror into the NAS BIN folder that sits next to this project. Each
    # version gets its own filename, so this accumulates a history rather
    # than overwriting a single file.
    bin_dir = os.path.normpath(os.path.join(PROJECT_DIR, "..", "BIN"))
    if os.path.isdir(bin_dir) and os.path.isfile(out_path):
        shutil.copy2(out_path, os.path.join(bin_dir, out_name))
        print("Copied %s -> %s" % (out_name, bin_dir))

    # Only bump once this build has fully succeeded (we're in a post-action
    # tied to the .bin actually being produced), so a failed compile never
    # burns a version number.
    bump_version_for_next_build(version)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)  # noqa: F821
