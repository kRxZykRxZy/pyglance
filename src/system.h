#ifndef PI_MONITOR_SYSTEM_H
#define PI_MONITOR_SYSTEM_H
#include <stddef.h>
void system_status_json(char *out, size_t out_size);
void processes_json(char *out, size_t out_size);
void ports_json(char *out, size_t out_size);
void logs_json(char *out, size_t out_size);
void disks_json(char *out, size_t out_size);
void files_json(char *out, size_t out_size, const char *path);
int process_signal(int pid, int sig);
#endif
