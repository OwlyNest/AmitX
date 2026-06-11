#include "settings.h"
#include "string.h"
#include "screen.h"
#include "fs.h"

#define MAX_SETTINGS 16

typedef struct {
    const char* key;
    char* value;
} Setting;

static Setting settings[MAX_SETTINGS];
static int setting_count = 0;
static int initialized = 0;

void settings_init_defaults(void) {
    settings[0].key = "theme";
    settings[0].value = "default";
    settings[1].key = "logo";
    settings[1].value = "1";
    settings[2].key = "volume";
    settings[2].value = "100";
    setting_count = 3;
    initialized = 1;
}

void settings_load() {
    if (initialized)
        return;
    
    const char* raw = fs_read("/Saved/settings.cfg");
    
    if (!raw) {
        settings_init_defaults();
        puts("[settings] No settings file, using defaults\n");
        return;
    }
    
    puts("[settings] Loading settings from file\n");
    
    char line[64];
    size_t line_idx = 0;
    const char *p = raw;
    
    while (*p && setting_count < MAX_SETTINGS) {
        if (*p == '\n') {
            if (line_idx > 0) {
                line[line_idx] = '\0';
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    settings[setting_count].key = strdup(line);
                    settings[setting_count].value = strdup(eq + 1);
                    setting_count++;
                }
            }
            line_idx = 0;
            p++;
        } else {
            if (line_idx < sizeof(line) - 1) {
                line[line_idx++] = *p;
            }
            p++;
        }
    }
    
    if (line_idx > 0 && setting_count < MAX_SETTINGS) {
        line[line_idx] = '\0';
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            settings[setting_count].key = strdup(line);
            settings[setting_count].value = strdup(eq + 1);
            setting_count++;
        }
    }
    
    initialized = 1;
    puts("[settings] Loaded ");
    putint(setting_count);
    puts(" settings\n");
}

const char* settings_get(const char* key) {
    if (!initialized)
        settings_load();
    
    for (int i = 0; i < setting_count; i++) {
        if (strcmp(settings[i].key, key) == 0) {
            return settings[i].value;
        }
    }
    
    return NULL;
}

void settings_set(const char* key, const char* value) {
    if (!initialized)
        settings_load();
    
    for (int i = 0; i < setting_count; i++) {
        if (strcmp(settings[i].key, key) == 0) {
            settings[i].value = strdup(value);
            return;
        }
    }
    
    if (setting_count < MAX_SETTINGS) {
        settings[setting_count].key = strdup(key);
        settings[setting_count].value = strdup(value);
        setting_count++;
    }
}

void settings_save(void) {
    if (!initialized)
        return;
    
    char buffer[512];
    size_t pos = 0;
    
    for (int i = 0; i < setting_count && pos < sizeof(buffer) - 2; i++) {
        size_t len = strlen(settings[i].key) + strlen(settings[i].value) + 2;
        if (pos + len >= sizeof(buffer))
            break;
        
        strcpy(buffer + pos, settings[i].key);
        pos += strlen(settings[i].key);
        buffer[pos++] = '=';
        strcpy(buffer + pos, settings[i].value);
        pos += strlen(settings[i].value);
        buffer[pos++] = '\n';
    }
    
    buffer[pos] = '\0';
    
    fs_write("/Saved/settings.cfg", buffer);
    puts("[settings] Settings saved\n");
}
