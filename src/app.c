
#include "task.h"
#include "screen.h"
#include "time.h"

void task_counter() {
    static int count = 0;
    putf("Counter: ", 14, 0); putint(count++); puts("\n");
    sleep(1);
}

void task_heartbeat() {
    putf("Heartbeat <3\n", 12, 0);
    sleep(2);
}
