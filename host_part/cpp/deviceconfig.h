#ifndef MPFU_DEVICECONFIG_H
#define MPFU_DEVICECONFIG_H
/*
 * MPFU per-device profile.
 *
 * A device profile describes the flash layout of a target chip so the host can
 * build a firmware image for it without any chip-specific code. Profiles are
 * plain text files (see configs/<name>.conf), selected on the command line with
 * -c <name> (the host looks for configs/<name>.conf next to the binary or in the
 * current directory).
 */

struct DeviceConfig {
    char          name[32];
    unsigned int  device_id;         // DEVID (0x8006); 0xFFFF in an image = any
    unsigned int  flash_words;       // program flash size in words
    unsigned int  row_words;         // erase/write granularity in words (e.g. 32)
    unsigned int  bl_code_start;     // bootloader code region (inclusive)
    unsigned int  bl_code_end;
    unsigned int  flags_row;         // bootloader flags row (host must not touch)
    unsigned int  app_vector_row;    // row holding the relocated app reset vector
    unsigned int  blank_word;        // erased flash word value (e.g. 0x3FFF)
    unsigned int  reset_vector_words;// words at 0x0000 reserved for BL trampoline
};

// Load a device profile by name (e.g. "16f1789"). Searches for
// "configs/<name>.conf" relative to the executable's directory and the CWD.
// Returns 0 on success and fills *cfg; non-zero on error (not found / parse).
int LoadDeviceConfig(const char *name, DeviceConfig *cfg);

#endif // MPFU_DEVICECONFIG_H
