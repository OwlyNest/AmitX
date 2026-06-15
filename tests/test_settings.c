#include "kernel/settings.h"
#include "screen/screen.h"
#include "screen/printk.h"
#include "fs/fs.h"

void test_settings_persistence(void) {
    printk("\n[TEST] Settings Persistence\n");
    printk("=============================\n");
    
    // Test 1: Load defaults
    printk("[1] Loading settings (should use defaults first)\n");
    settings_load();
    
    // Test 2: Get existing values
    printk("[2] Getting default values:\n");
    const char* theme = settings_get("theme");
    const char* logo = settings_get("logo");
    const char* volume = settings_get("volume");
    
    printk("  theme  = %s\n", theme ? theme : "NULL");
    printk("  logo   = %s\n", logo ? logo : "NULL");
    printk("  volume = %s\n", volume ? volume : "NULL");
    
    // Test 3: Modify settings
    puts("[3] Modifying settings:\n");
    settings_set("theme", "dark");
    settings_set("volume", "50");
    settings_set("new_key", "new_value");
    
    theme = settings_get("theme");
    volume = settings_get("volume");
    const char* new_val = settings_get("new_key");
    
    printk("  theme  = %s\n", theme);
    printk("  volume = %s\n", volume);
    printk("  new_key = %s\n", new_val ? new_val : "NULL");
    
    // Test 4: Save settings
    puts("[4] Saving settings to /Saved/settings.cfg\n");
    settings_save();
    
    // Test 5: Verify file was written
    puts("[5] Reading settings file back:\n");
    const char* content = fs_read("/Saved/settings.cfg");
    if (content) {
        printk("  File content:\n%s", content);
    } else {
        puts("  ERROR: Could not read settings file!\n");
    }
    
    puts("[TEST] Settings persistence test complete\n");
    puts("=============================\n\n");
}
