#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#include "cJSON.h"
#include "downloader.h"
#include "launcher.h"

#define MANIFEST_URL "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"
#define VERSIONS_DIR "versions"
#define LIBRARIES_DIR "libraries"
#define ASSETS_DIR "assets"
#define NATIVES_DIR "natives"

void send_json(const char *type, const char *message, int progress) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddNumberToObject(root, "progress", progress);
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);
    fflush(stdout);
    free(json_str);
    cJSON_Delete(root);
}

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void ensure_directory_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) _mkdir(path);
}

void ensure_path_exists(const char *path) {
    char tmp[512];
    char *p = NULL;
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            _mkdir(tmp);
            *p = c;
        }
    }
    _mkdir(tmp);
}

void ensure_parent_dir_exists(const char *filepath) {
    char *last_slash = strrchr(filepath, '/');
    if (!last_slash) last_slash = strrchr(filepath, '\\');
    if (last_slash) {
        char dir[512];
        size_t len = last_slash - filepath;
        strncpy(dir, filepath, len);
        dir[len] = '\0';
        ensure_path_exists(dir);
    }
}

void ensure_version_dirs_exist(const char *version_id) {
    ensure_directory_exists(VERSIONS_DIR);
    char version_path[128];
    snprintf(version_path, sizeof(version_path), "%s/%s", VERSIONS_DIR, version_id);
    ensure_directory_exists(version_path);
}

char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    fclose(f);
    return buffer;
}

void extract_jar(const char *jar_path, const char *dest_dir) {
    char command[1024];
    ensure_directory_exists(dest_dir);
    snprintf(command, sizeof(command), "tar -xf \"%s\" -C \"%s\"", jar_path, dest_dir);
    system(command);
}

int is_rule_allowed(cJSON *rules) {
    if (!rules) return 1;
    int allowed = 0;
    cJSON *rule = NULL;
    cJSON_ArrayForEach(rule, rules) {
        cJSON *action = cJSON_GetObjectItemCaseSensitive(rule, "action");
        if (!cJSON_IsString(action)) continue;
        cJSON *os = cJSON_GetObjectItemCaseSensitive(rule, "os");
        if (os) {
            cJSON *os_name = cJSON_GetObjectItemCaseSensitive(os, "name");
            if (cJSON_IsString(os_name) && strcmp(os_name->valuestring, "windows") == 0) {
                allowed = (strcmp(action->valuestring, "allow") == 0);
            }
        } else {
            allowed = (strcmp(action->valuestring, "allow") == 0);
        }
    }
    return allowed;
}

void queue_libraries(cJSON *json) {
    cJSON *libraries = cJSON_GetObjectItemCaseSensitive(json, "libraries");
    cJSON *lib = NULL;
    cJSON_ArrayForEach(lib, libraries) {
        cJSON *rules = cJSON_GetObjectItemCaseSensitive(lib, "rules");
        if (!is_rule_allowed(rules)) continue;
        cJSON *downloads = cJSON_GetObjectItemCaseSensitive(lib, "downloads");
        if (downloads) {
            cJSON *artifact = cJSON_GetObjectItemCaseSensitive(downloads, "artifact");
            if (artifact) {
                cJSON *url = cJSON_GetObjectItemCaseSensitive(artifact, "url");
                cJSON *path = cJSON_GetObjectItemCaseSensitive(artifact, "path");
                if (cJSON_IsString(url) && cJSON_IsString(path)) {
                    char full_path[512];
                    snprintf(full_path, sizeof(full_path), "%s/%s", LIBRARIES_DIR, path->valuestring);
                    add_to_download_queue(url->valuestring, full_path);
                }
            }
            cJSON *classifiers = cJSON_GetObjectItemCaseSensitive(downloads, "classifiers");
            cJSON *natives = cJSON_GetObjectItemCaseSensitive(lib, "natives");
            if (classifiers && natives) {
                cJSON *windows = cJSON_GetObjectItemCaseSensitive(natives, "windows");
                if (cJSON_IsString(windows)) {
                    cJSON *native_artifact = cJSON_GetObjectItemCaseSensitive(classifiers, windows->valuestring);
                    if (native_artifact) {
                        cJSON *url = cJSON_GetObjectItemCaseSensitive(native_artifact, "url");
                        cJSON *path = cJSON_GetObjectItemCaseSensitive(native_artifact, "path");
                        if (cJSON_IsString(url) && cJSON_IsString(path)) {
                            char full_path[512];
                            snprintf(full_path, sizeof(full_path), "%s/%s", LIBRARIES_DIR, path->valuestring);
                            add_to_download_queue(url->valuestring, full_path);
                        }
                    }
                }
            }
        }
    }
}

void queue_assets(cJSON *json) {
    cJSON *assetIndex = cJSON_GetObjectItemCaseSensitive(json, "assetIndex");
    if (!assetIndex) return;
    cJSON *url = cJSON_GetObjectItemCaseSensitive(assetIndex, "url");
    cJSON *id = cJSON_GetObjectItemCaseSensitive(assetIndex, "id");
    if (cJSON_IsString(url) && cJSON_IsString(id)) {
        char index_path[512];
        snprintf(index_path, sizeof(index_path), "%s/indexes/%s.json", ASSETS_DIR, id->valuestring);
        send_json("status", "Downloading asset index", 0);
        download_file_immediate(url->valuestring, index_path);
        char *index_content = read_file(index_path);
        if (index_content) {
            cJSON *index_json = cJSON_Parse(index_content);
            free(index_content);
            if (index_json) {
                cJSON *objects = cJSON_GetObjectItemCaseSensitive(index_json, "objects");
                cJSON *object = NULL;
                cJSON_ArrayForEach(object, objects) {
                    cJSON *hash = cJSON_GetObjectItemCaseSensitive(object, "hash");
                    if (cJSON_IsString(hash)) {
                        char first2[3];
                        strncpy(first2, hash->valuestring, 2);
                        first2[2] = '\0';
                        char asset_url[512];
                        snprintf(asset_url, sizeof(asset_url), "https://resources.download.minecraft.net/%s/%s", first2, hash->valuestring);
                        char asset_path[512];
                        snprintf(asset_path, sizeof(asset_path), "%s/objects/%s/%s", ASSETS_DIR, first2, hash->valuestring);
                        add_to_download_queue(asset_url, asset_path);
                    }
                }
                cJSON_Delete(index_json);
            }
        }
    }
}

void download_client_jar(const char *version_json_path, const char *version_id, const char *username) {
    char *json_content = read_file(version_json_path);
    if (!json_content) return;
    cJSON *json = cJSON_Parse(json_content);
    free(json_content); 
    if (!json) return;
    cJSON *downloads = cJSON_GetObjectItemCaseSensitive(json, "downloads");
    cJSON *client = cJSON_GetObjectItemCaseSensitive(downloads, "client");
    cJSON *url = cJSON_GetObjectItemCaseSensitive(client, "url");
    if (cJSON_IsString(url) && (url->valuestring != NULL)) {
        char jar_filename[128];
        snprintf(jar_filename, sizeof(jar_filename), "%s/%s/%s.jar", VERSIONS_DIR, version_id, version_id);
        add_to_download_queue(url->valuestring, jar_filename);
    }
    queue_libraries(json);
    queue_assets(json);
    send_json("status", "Preparing downloads", 0);
    process_download_queue();
    cJSON *libraries = cJSON_GetObjectItemCaseSensitive(json, "libraries");
    cJSON *lib = NULL;
    cJSON_ArrayForEach(lib, libraries) {
        cJSON *downloads_lib = cJSON_GetObjectItemCaseSensitive(lib, "downloads");
        if (downloads_lib) {
            cJSON *classifiers = cJSON_GetObjectItemCaseSensitive(downloads_lib, "classifiers");
            cJSON *natives = cJSON_GetObjectItemCaseSensitive(lib, "natives");
            if (classifiers && natives) {
                cJSON *windows = cJSON_GetObjectItemCaseSensitive(natives, "windows");
                if (cJSON_IsString(windows)) {
                    cJSON *native_artifact = cJSON_GetObjectItemCaseSensitive(classifiers, windows->valuestring);
                    if (native_artifact) {
                        cJSON *path = cJSON_GetObjectItemCaseSensitive(native_artifact, "path");
                        if (cJSON_IsString(path)) {
                            char full_path[512];
                            snprintf(full_path, sizeof(full_path), "%s/%s", LIBRARIES_DIR, path->valuestring);
                            extract_jar(full_path, NATIVES_DIR);
                        }
                    }
                }
            }
        }
    }
    ensure_directory_exists(NATIVES_DIR);
    launch_game(json, version_id, username);
    cJSON_Delete(json);
}

void parse_manifest(const char *json_data, const char *version_input, const char *username) {
    cJSON *json = cJSON_Parse(json_data);
    if (json == NULL) return;
    cJSON *versions = cJSON_GetObjectItemCaseSensitive(json, "versions");
    cJSON *version = NULL;
    int found = 0;
    cJSON_ArrayForEach(version, versions) {
        cJSON *id = cJSON_GetObjectItemCaseSensitive(version, "id");
        cJSON *url = cJSON_GetObjectItemCaseSensitive(version, "url");
        if (cJSON_IsString(id) && strcmp(id->valuestring, version_input) == 0) {
            if (cJSON_IsString(url)) {
                ensure_version_dirs_exist(version_input);
                char filename[128];
                snprintf(filename, sizeof(filename), "%s/%s/%s.json", VERSIONS_DIR, version_input, version_input);
                download_file_immediate(url->valuestring, filename);
                download_client_jar(filename, version_input, username);
                found = 1;
                break;
            }
        }
    }
    if (!found) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Version %s not found", version_input);
        send_json("error", err_msg, -1);
    }
    cJSON_Delete(json);
}

int main(int argc, char *argv[]) {
    char *version_to_launch = NULL;
    char *username = "Player";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) version_to_launch = argv[++i];
        else if (strcmp(argv[i], "--username") == 0 && i + 1 < argc) username = argv[++i];
    }
    if (!version_to_launch) return 1;
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, MANIFEST_URL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "curl_easy_perform() failed: %s", curl_easy_strerror(res));
            send_json("error", err_msg, -1);
        } else {
            parse_manifest(chunk.memory, version_to_launch, username);
        }
        curl_easy_cleanup(curl);
    }
    free(chunk.memory);
    curl_global_cleanup();
    return 0;
}