Import("env")

from pathlib import Path
from SCons.Script import COMMAND_LINE_TARGETS


def patch_source(path, replacements):
    source = path.read_text(encoding="utf-8")
    patched = source
    for original, replacement in replacements:
        if original in patched:
            patched = patched.replace(original, replacement, 1)
        elif replacement not in patched:
            raise RuntimeError(f"Pinned dependency source changed unexpectedly: {path}")
    if patched != source:
        path.write_text(patched, encoding="utf-8")


filesystem_targets = {"buildfs", "uploadfs", "uploadfsota"}
if not filesystem_targets.intersection(COMMAND_LINE_TARGETS):
    library_root = Path(env["PROJECT_DIR"]) / ".pio" / "libdeps" / env["PIOENV"]
    patch_source(
        library_root / "OneWire" / "OneWire.cpp",
        [
            (
                "#  undef noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)",
                "#  undef noInterrupts",
            ),
            (
                "#  undef interrupts() portEXIT_CRITICAL(&mux);}",
                "#  undef interrupts",
            ),
        ],
    )
    patch_source(
        library_root / "LoRa" / "src" / "LoRa.cpp",
        [
            (
                "readRegister(REG_FREQ_ERROR_MSB) & B111",
                "readRegister(REG_FREQ_ERROR_MSB) & 0b111",
            ),
            (
                "readRegister(REG_FREQ_ERROR_MSB) & B1000",
                "readRegister(REG_FREQ_ERROR_MSB) & 0b1000",
            ),
        ],
    )
