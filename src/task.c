// task.c

#include "task.h"
#include "screen.h" // for putf, etc.
#include "time.h"  // if you want to sleep

#define MAX_TASKS 8

static Task tasks[MAX_TASKS];
static int task_count = 0;
static int current_task = -1;

void init_tasks() {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].name = 0;
        tasks[i].function = 0;
        tasks[i].active = 0;
    }
}

void register_task(const char* name, task_func func) {
    if (task_count < MAX_TASKS) {
        tasks[task_count].name = name;
        tasks[task_count].function = func;
        tasks[task_count].active = 1;
        task_count++;
    }
}

void yield() {
    run_next_task();
}

void run_next_task() {
    if (task_count == 0) return;

    for (int i = 0; i < task_count; i++) {
        current_task = (current_task + 1) % task_count;
        if (tasks[current_task].active) {
            tasks[current_task].function();
            break;
        }
    }
}