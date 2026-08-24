#ifndef PI_MONITOR_SYSTEM_H
#define PI_MONITOR_SYSTEM_H
#include <stddef.h>
void system_status_json(char *out, size_t out_size);
void processes_json(char *out, size_t out_size);
void ports_json(char *out, size_t out_size);
void logs_json(char *out, size_t out_size);
void disks_json(char *out, size_t out_size);
void files_json(char *out, size_t out_size, const char *path);
void network_json(char *out, size_t out_size);
void services_json(char *out, size_t out_size);
void memory_json(char *out, size_t out_size);
void cpu_json(char *out, size_t out_size);
void cpu_cores_json(char *out, size_t out_size);
void system_info_json(char *out, size_t out_size);
int process_signal(int pid, int sig);
#endif
