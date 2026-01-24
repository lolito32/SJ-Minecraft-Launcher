#include "launcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIBRARIES_DIR "libraries"
#define ASSETS_DIR "assets"
#define NATIVES_DIR "natives"
#define VERSIONS_DIR "versions"

extern void send_json(const char *type, const char *message, int progress);
extern int is_rule_allowed(cJSON *rules);

void build_classpath(cJSON *json, char *buffer, size_t size, const char *version_id) {
    buffer[0] = '\0';
    cJSON *libraries = cJSON_GetObjectItemCaseSensitive(json, "libraries");
    cJSON *lib = NULL;

    cJSON_ArrayForEach(lib, libraries) {
        cJSON *rules = cJSON_GetObjectItemCaseSensitive(lib, "rules");
        if (!is_rule_allowed(rules)) continue;

        cJSON *downloads = cJSON_GetObjectItemCaseSensitive(lib, "downloads");
        if (downloads) {
            cJSON *artifact = cJSON_GetObjectItemCaseSensitive(downloads, "artifact");
            if (artifact) {
                cJSON *path = cJSON_GetObjectItemCaseSensitive(artifact, "path");
                if (cJSON_IsString(path)) {
                    strncat(buffer, LIBRARIES_DIR, size - strlen(buffer) - 1);
                    strncat(buffer, "/", size - strlen(buffer) - 1);
                    strncat(buffer, path->valuestring, size - strlen(buffer) - 1);
                    strncat(buffer, ";", size - strlen(buffer) - 1);
                }
            }
        }
    }

    char client_jar[256];
    snprintf(client_jar, sizeof(client_jar), "%s/%s/%s.jar", VERSIONS_DIR, version_id, version_id);
    strncat(buffer, client_jar, size - strlen(buffer) - 1);
}

void launch_game(cJSON *json, const char *version_id, const char *username) {
    char classpath[32768];
    build_classpath(json, classpath, sizeof(classpath), version_id);

    cJSON *assetIndex = cJSON_GetObjectItemCaseSensitive(json, "assetIndex");
    cJSON *assetId = cJSON_GetObjectItemCaseSensitive(assetIndex, "id");
    const char *assets_index_name = assetId ? assetId->valuestring : "legacy";

    cJSON *mainClass = cJSON_GetObjectItemCaseSensitive(json, "mainClass");
    const char *main_class = mainClass ? mainClass->valuestring : "net.minecraft.client.main.Main";

    char command[65536];
    snprintf(command, sizeof(command), 
        "java -Xmx8G -Xms8G "
        "-XX:+UnlockExperimentalVMOptions -XX:+UseG1GC -XX:G1NewSizePercent=20 -XX:G1ReservePercent=20 -XX:MaxGCPauseMillis=50 -XX:G1HeapRegionSize=32M "
        "-Djava.library.path=%s -Dlwjgl.librarypath=%s "
        "--add-opens java.base/java.lang=ALL-UNNAMED "
        "--add-opens java.base/java.util=ALL-UNNAMED "
        "--add-opens java.base/java.io=ALL-UNNAMED "
        "--add-opens java.base/java.nio=ALL-UNNAMED "
        "--add-opens java.base/sun.nio.ch=ALL-UNNAMED "
        "--add-opens java.base/jdk.internal.loader=ALL-UNNAMED "
        "-Dlwjgl.util.NoChecks=true "
        "-cp \"%s\" %s "
        "--username %s --version %s --gameDir . --assetsDir %s --assetIndex %s --uuid 00000000-0000-0000-0000-000000000000 --accessToken null --userType mojang --versionType release",
        NATIVES_DIR, NATIVES_DIR, classpath, main_class, username, version_id, ASSETS_DIR, assets_index_name);

    send_json("status", "Launching game", 100);
    system(command);
}
