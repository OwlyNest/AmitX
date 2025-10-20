
#include "commands.h"
#include "cyclone.h"
#include "screen.h"
#include "string.h"
#include "utils.h"
#include "kernel.h"
#include "fs.h"
#include "time.h"
#include "tests.h"

extern int tick_count;
extern int load_cyclone;
extern int version;


void execute_command(const char* input) {
    puts(">> ");
    if (starts_with(input, "echo ") || starts_with(input, "hoot ")) {
        const char* message = input + 5;  // Skip "echo "
        puts(message);
    } else if (starts_with(input, "hex ")) {
        const char* num = input + 4;
        uint32_t number = atoi(num);
        puthex(number);
    } else if (starts_with(input, "touch ")) {
        const char* file = input + 6;
        fs_add(file, "");
    } else if (starts_with(input, "test ")) {
        const char* num = input + 5;
        int n = atoi(num);
        load_cyclone = 0;
        test(n);

    } else if (starts_with(input, "read ")) {
        const char* file = input + 5;
        puts(file);
        newline();


        char path[64];
        strcpy(path, "/Saved/");  // Copy folder path into buffer
        strcat(path, file);       // Safely append the filename
        
        puts(path);
        newline();

        const char* content = fs_read(path);
        puts(content);
    } else if (strcmp(input, "coffee") == 0) {
        uint32_t number = 12648430;
        puthex(number);
    } else if (strcmp(input, "time") == 0) {
        int time = tick_count / 100;
        puts("Uptime: ");
        putint(time);
        puts(" seconds");
    } else if (strcmp(input, "back") == 0) {
        load_cyclone = 0;
        clear();
        draw_start();
    } else if (strcmp(input, "switch logo") == 0) {
        if (version == 1) {
            version++;
        } else if (version == 2) {
            version--;
        }
        cyclone_main(0);
    } else if (strcmp(input, "help") == 0) {
        puts("Available commands:\n");
        puts("  echo/hoot <text>   - Print text\n");
        puts("  hex <number>       - Print number as hex\n");
        puts("  time               - Show uptime\n");
        puts("  clear              - Clear screen\n");
        puts("  back               - Return to menu\n");
        puts("  quit               - Exit system\n");
        puts("  coffee             - Print 0xC0FFEE\n");
        puts("  ls                 - Print files\n");
        puts("  switch logo        - Switch Owly ASCII art");
    } else if (strcmp(input, "ls") == 0) {
        puts("\b\b\b");
        fs_debug_list();
    } else if (strcmp(input, "quit") == 0) {
        sleep(1);
        qemu_exit(0);
    } else if (strcmp(input, "clear") == 0) {
        cyclone_main(0);
    } else {
        puts("Unknown command");
    }
    newline();
}
