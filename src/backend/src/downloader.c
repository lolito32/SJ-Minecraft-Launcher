#include "downloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>

#define MAX_PARALLEL_DOWNLOADS 10

typedef struct {
    char url[512];
    char path[512];
} DownloadItem;

extern void send_json(const char *type, const char *message, int progress);
extern void ensure_parent_dir_exists(const char *filepath);

static DownloadItem *download_queue = NULL;
static int queue_size = 0;
static int queue_capacity = 0;

void add_to_download_queue(const char *url, const char *path) {
    if (_access(path, 0) == 0) return;

    if (queue_size >= queue_capacity) {
        queue_capacity = queue_capacity == 0 ? 100 : queue_capacity * 2;
        download_queue = realloc(download_queue, queue_capacity * sizeof(DownloadItem));
    }

    strncpy(download_queue[queue_size].url, url, 511);
    strncpy(download_queue[queue_size].path, path, 511);
    queue_size++;
}

void process_download_queue() {
    if (queue_size == 0) return;

    int total_to_download = queue_size;
    int downloaded_count = 0;

    CURLM *multi_handle = curl_multi_init();
    int still_running = 0;
    int queue_idx = 0;

    while (queue_idx < queue_size && queue_idx < MAX_PARALLEL_DOWNLOADS) {
        CURL *eh = curl_easy_init();
        ensure_parent_dir_exists(download_queue[queue_idx].path);
        FILE *fp = fopen(download_queue[queue_idx].path, "wb");
        
        curl_easy_setopt(eh, CURLOPT_URL, download_queue[queue_idx].url);
        curl_easy_setopt(eh, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(eh, CURLOPT_PRIVATE, fp);
        curl_easy_setopt(eh, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1L);
        
        curl_multi_add_handle(multi_handle, eh);
        queue_idx++;
    }

    do {
        curl_multi_perform(multi_handle, &still_running);

        int msgs_left;
        CURLMsg *msg;
        while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                CURL *eh = msg->easy_handle;
                FILE *fp;
                curl_easy_getinfo(eh, CURLINFO_PRIVATE, &fp);
                if (fp) fclose(fp);

                downloaded_count++;
                int progress = (downloaded_count * 100) / total_to_download;
                char status_msg[128];
                snprintf(status_msg, sizeof(status_msg), "Downloading files (%d/%d)", downloaded_count, total_to_download);
                send_json("status", status_msg, progress);

                curl_multi_remove_handle(multi_handle, eh);
                curl_easy_cleanup(eh);

                if (queue_idx < queue_size) {
                    CURL *neh = curl_easy_init();
                    ensure_parent_dir_exists(download_queue[queue_idx].path);
                    FILE *nfp = fopen(download_queue[queue_idx].path, "wb");
                    
                    curl_easy_setopt(neh, CURLOPT_URL, download_queue[queue_idx].url);
                    curl_easy_setopt(neh, CURLOPT_WRITEDATA, nfp);
                    curl_easy_setopt(neh, CURLOPT_PRIVATE, nfp);
                    curl_easy_setopt(neh, CURLOPT_USERAGENT, "libcurl-agent/1.0");
                    curl_easy_setopt(neh, CURLOPT_FOLLOWLOCATION, 1L);
                    
                    curl_multi_add_handle(multi_handle, neh);
                    queue_idx++;
                    still_running = 1;
                }
            }
        }

        if (still_running) {
            int numfds;
            curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
        }
    } while (still_running);

    curl_multi_cleanup(multi_handle);
    free(download_queue);
    download_queue = NULL;
    queue_size = 0;
    queue_capacity = 0;
}

void download_file_immediate(const char *url, const char *filename) {
    CURL *curl;
    FILE *fp;
    if (_access(filename, 0) == 0) return;
    ensure_parent_dir_exists(filename);
    curl = curl_easy_init();
    if (curl) {
        fp = fopen(filename, "wb");
        if (fp) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_perform(curl);
            fclose(fp);
        }
        curl_easy_cleanup(curl);
    }
}
