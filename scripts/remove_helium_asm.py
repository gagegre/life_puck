# scripts/remove_helium_asm.py
#
# Stubs out LVGL's ARM Helium assembly file before every build so the
# Xtensa assembler doesn't fail on it. Registered as a pre-action on the
# SCons build target rather than a plain pre-script, which ensures it runs
# every time the target is considered rather than only once per session.

Import("env")  # noqa: F821
import os
import pathlib

STUB = """; ARM Helium (MVE) assembly stub.
; Replaced by scripts/remove_helium_asm.py for Xtensa / ESP32 builds.
; LV_DRAW_SW_ASM_NONE in src/lv_conf.h means no C code calls this anyway.
"""

def patch_helium(source, target, env):
    base = pathlib.Path(".pio") / "libdeps"
    pattern = "**/lvgl/draw/sw/blend/helium/*.S"
    found = list(base.glob(pattern))

    if not found:
        print("[lvgl-patch] No Helium .S files found (nothing to do).")
        return

    for path in found:
        content = path.read_text(errors="replace")
        if content.strip() == STUB.strip():
            print(f"[lvgl-patch] Already stubbed: {path}")
        else:
            path.write_text(STUB)
            print(f"[lvgl-patch] Stubbed ARM Helium ASM: {path}")


# Fire before the first compile action in the environment.
env.AddPreAction("$BUILD_DIR", patch_helium)