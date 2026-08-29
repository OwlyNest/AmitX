
#ifndef __KERNEL_SETTINGS_H__
#define __KERNEL_SETTINGS_H__

void settings_load();
const char* settings_get(const char* key);
void settings_set(const char* key, const char* value);
void settings_save(void);

#endif
