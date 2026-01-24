#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <curl/curl.h>
#include "cJSON.h"

void add_to_download_queue(const char *url, const char *path);
void process_download_queue();
void download_file_immediate(const char *url, const char *filename);

#endif
