
#include "fs.h"
#include "settings.h"
#include "string.h"
#include "screen.h"

#define MAX_SETTINGS 16

typedef struct {
    const char* key;
    const char* value;
} Setting;

static Setting settings[MAX_SETTINGS];
static int setting_count = 0;

void settings_load() {
    putint(setting_count);
    const char* raw = fs_read("/Saved/settings.cfg");
    if (!raw) {
        puts("[settings] Failed to load file.\n");
        return;
    }

    puts("[settings] Raw settings file loaded:\n");
    puts(raw);

    char line[64];
    size_t j = 0;

    while (*raw && setting_count < MAX_SETTINGS) {
        putint(setting_count);
        if (*raw == '\n') {
            line[j] = '\0';
            puts("[settings] Line parsed: ");
            puts(line);
            putc('\n');

            char* eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                settings[setting_count].key = strdup(line);
                settings[setting_count].value = strdup(eq + 1);

                puts("[settings] Key = ");
                puts(settings[setting_count].key);
                puts(", Value = ");
                puts(settings[setting_count].value);
                putc('\n');

                setting_count++;
                putint(setting_count);
            } else {
                puts("[settings] Skipping invalid line (no '=')\n");
            }
            j = 0;
        } else if (j < sizeof(line) - 1) {
            line[j++] = *raw;
        }
        raw++;
    }

    // Handle final line if it doesn't end in \n
    if (j > 0 && setting_count < MAX_SETTINGS) {
        line[j] = '\0';
        puts("[settings] Line parsed (EOF): ");
        puts(line);
        putc('\n');

        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            settings[setting_count].key = strdup(line);
            settings[setting_count].value = strdup(eq + 1);

            puts("[settings] Key = ");
            puts(settings[setting_count].key);
            puts(", Value = ");
            puts(settings[setting_count].value);
            putc('\n');

            setting_count++;
            putint(setting_count);
        }
    }

    puts("[settings] Total settings loaded: ");
    putint(setting_count);
    putc('\n');
}

const char* settings_get(const char* key) {
    //puts("[settings_get] Looking for key: ");
    //puts(key);
    //putc('\n');
    for (int i = 0; i < setting_count; i++) {
        //puts("[settings_get] Comparing to: ");
        //puts(settings[i].key);
        //putc('\n');

        if (strcmp(settings[i].key, key) == 0) {
            //puts("[settings_get] Match found. Value: ");
            //puts(settings[i].value);
            //putc('\n');
            return settings[i].value;
        }
    }
    //puts("[settings_get] No match found.\n");
    return NULL;
}
