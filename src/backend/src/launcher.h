#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "cJSON.h"

void launch_game(cJSON *json, const char *version_id, const char *username);
void write_classpath_file(const char *classpath);

#endif
