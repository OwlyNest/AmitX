
#ifndef SETTINGS_H
#define SETTINGS_H

void settings_load();
const char* settings_get(const char* key);
void settings_set(const char* key, const char* value);
void settings_save(void);

#endif
