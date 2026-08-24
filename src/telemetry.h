#ifndef PYGLANCE_TELEMETRY_H
#define PYGLANCE_TELEMETRY_H
#include <stddef.h>
void telemetry_collect(void);
void telemetry_json(char *out,size_t n);
#endif
