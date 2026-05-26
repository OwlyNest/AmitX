
#ifndef TASK_H
#define TASK_H

typedef void (*task_func)();

typedef struct {
    const char* name;
    task_func function;
    int active;
} Task;

void init_tasks();
void register_task(const char* name, task_func func);
void yield();
void run_next_task();

#endif
