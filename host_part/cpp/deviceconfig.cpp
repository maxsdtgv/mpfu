#include "deviceconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

// Parse "key = value" lines (value decimal or 0x-hex). '#' starts a comment.
// Unknown keys are ignored; missing keys keep their zero-initialised default.
static int parseConfigFile(const char *path, DeviceConfig *cfg)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Strip comments.
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char key[64] = {};
        char val[64] = {};
        // key = value  (whitespace-tolerant)
        if (sscanf(line, " %63[a-zA-Z0-9_] = %63s", key, val) != 2) continue;

        unsigned int v = (unsigned int)strtoul(val, nullptr, 0);

        if      (!strcmp(key, "device_id"))          cfg->device_id = v;
        else if (!strcmp(key, "flash_words"))        cfg->flash_words = v;
        else if (!strcmp(key, "row_words"))          cfg->row_words = v;
        else if (!strcmp(key, "bl_code_start"))      cfg->bl_code_start = v;
        else if (!strcmp(key, "bl_code_end"))        cfg->bl_code_end = v;
        else if (!strcmp(key, "flags_row"))          cfg->flags_row = v;
        else if (!strcmp(key, "app_vector_row"))     cfg->app_vector_row = v;
        else if (!strcmp(key, "blank_word"))         cfg->blank_word = v;
        else if (!strcmp(key, "reset_vector_words")) cfg->reset_vector_words = v;
    }
    fclose(f);
    return 0;
}

// Basic sanity checks so a malformed profile fails loudly instead of producing
// a subtly broken image.
static int validateConfig(const DeviceConfig *cfg)
{
    if (cfg->row_words == 0 || cfg->flash_words == 0) return -1;
    if (cfg->flash_words % cfg->row_words != 0)       return -2;
    if (cfg->app_vector_row % cfg->row_words != 0)    return -3;
    if (cfg->flags_row % cfg->row_words != 0)         return -4;
    if (cfg->app_vector_row >= cfg->flash_words)      return -5;
    if (cfg->reset_vector_words == 0)                 return -6;
    return 0;
}

int LoadDeviceConfig(const char *name, DeviceConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->name, name, sizeof(cfg->name) - 1);

    // Candidate paths: <exe_dir>/configs/<name>.conf, then ./configs/<name>.conf
    char candidates[2][PATH_MAX];
    int n = 0;

    char exe[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = '\0';
            snprintf(candidates[n++], PATH_MAX, "%s/configs/%s.conf", exe, name);
        }
    }
    snprintf(candidates[n++], PATH_MAX, "configs/%s.conf", name);

    int parsed = -1;
    for (int i = 0; i < n; i++) {
        if (parseConfigFile(candidates[i], cfg) == 0) { parsed = 0; break; }
    }
    if (parsed != 0) {
        fprintf(stderr, "[CONFIG] ERROR: cannot find profile '%s' (configs/%s.conf)\n",
                name, name);
        return -1;
    }

    int rc = validateConfig(cfg);
    if (rc != 0) {
        fprintf(stderr, "[CONFIG] ERROR: profile '%s' failed validation (%d)\n", name, rc);
        return rc;
    }
    return 0;
}
