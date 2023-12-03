#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "errno.h"
#include "cJSON.h"
#include "app_lora.h"
#include "app_mqtt.h"
#include "app_wifi.h"
#include "app_web_ui.h"
#include "smoke_x.h"

static const char *TAG = "app_web_ui";
#define REST_CHECK(a, str, goto_tag, ...)                         \
    do {                                                          \
        if (!(a)) {                                               \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, \
                     ##__VA_ARGS__);                              \
            goto goto_tag;                                        \
        }                                                         \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

typedef struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
} async_resp_arg_t;

#define CHECK_FILE_EXTENSION(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

static esp_err_t init_fs(void) {
    esp_vfs_spiffs_conf_t conf = {.base_path = CONFIG_WEB_MOUNT_POINT,
                                  .partition_label = NULL,
                                  .max_files = 5,
                                  .format_if_mount_failed = false};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",
                     esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

static void json_check_strncpy(cJSON *json, char **dst, char *key,
                               size_t max_len) {
    if (cJSON_HasObjectItem(json, key)) {
        size_t len;
        char *value;
        value = cJSON_GetObjectItem(json, key)->valuestring;
        // TODO: app_mqtt.c should be doing this stuff
        len = strlen(value);
        if (len <= max_len) {
            if (*dst) {
                free(*dst);
            }
            *dst = malloc(len + 1);
            strncpy(*dst, value, len + 1);
        }
    } else {
        ESP_LOGE(TAG, "Key '%s' not found in JSON", key);
    }
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req,
                                            const char *filepath) {
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html.gz")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js.gz")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css.gz")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png.gz")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico.gz")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg.gz")) {
        type = "text/xml";
    }

    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context =
        (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (!strcmp(req->uri, "/") || !strcmp(req->uri, "/wlan") ||
        !strcmp(req->uri, "/pairing") || !strcmp(req->uri, "/mqtt") ||
        !strcmp(req->uri, "/lora")) {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
        // filenames other than index.html are hashed, so cache for a long time
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=604800");
    }
    strlcat(filepath, ".gz", sizeof(filepath));
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Invalid resource requested : %s", req->uri);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s %d", filepath, errno);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete: %s", filepath);
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler for setting RF params */
static esp_err_t rf_params_set_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    app_lora_params_t *params;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    char *param_str = cJSON_Print(root);
    ESP_LOGI(TAG, "Setting RF params: \n%s", param_str);

    // TODO check that key exists cJSON_HasObjectItem
    params = calloc(1, sizeof(app_lora_params_t));
    params->tx_power = cJSON_GetObjectItem(root, "txPower")->valueint;
    params->bandwidth = cJSON_GetObjectItem(root, "bandwidth")->valueint;
    params->crc_on = cJSON_GetObjectItem(root, "enableCRC")->valueint;
    params->coding_rate = cJSON_GetObjectItem(root, "codingRate")->valueint;
    params->frequency = cJSON_GetObjectItem(root, "frequency")->valueint;
    params->implicit_hdr =
        cJSON_GetObjectItem(root, "implicitHeader")->valueint;
    params->msg_len = cJSON_GetObjectItem(root, "messageLength")->valueint;
    params->sync_word = cJSON_GetObjectItem(root, "syncWord")->valueint;
    params->spreading_factor =
        cJSON_GetObjectItem(root, "spreadingFactor")->valueint;
    params->preamble_len =
        cJSON_GetObjectItem(root, "preambleLength")->valueint;
    app_lora_set_params(params, xTaskGetCurrentTaskHandle());
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    cJSON_Delete(root);
    free(param_str);
    free(params);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

/* Handler for getting RF params */
static esp_err_t rf_params_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    app_lora_params_t params;
    cJSON *root = cJSON_CreateObject();
    app_lora_get_params(&params);
    cJSON_AddNumberToObject(root, "txPower", params.tx_power);
    cJSON_AddNumberToObject(root, "bandwidth", params.bandwidth);
    cJSON_AddBoolToObject(root, "enableCRC", params.crc_on);
    cJSON_AddNumberToObject(root, "codingRate", params.coding_rate);
    cJSON_AddNumberToObject(root, "frequency", params.frequency);
    cJSON_AddBoolToObject(root, "implicitHeader", params.implicit_hdr);
    cJSON_AddNumberToObject(root, "messageLength", params.msg_len);
    cJSON_AddNumberToObject(root, "syncWord", params.sync_word);
    cJSON_AddNumberToObject(root, "spreadingFactor", params.spreading_factor);
    cJSON_AddNumberToObject(root, "preambleLength", params.preamble_len);
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (json_str) {
        httpd_resp_sendstr(req, json_str);
        free(json_str);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* Handler for getting pairing status */
static esp_err_t pairing_status_get_handler(httpd_req_t *req) {
    smoke_x_config_t smoke_x_config;
    smoke_x_get_config(&smoke_x_config);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "isPaired", smoke_x_is_configured());
    cJSON_AddNumberToObject(root, "currentFrequency", smoke_x_config.frequency);
    cJSON_AddStringToObject(root, "deviceId", smoke_x_config.device_id);
    cJSON_AddStringToObject(root, "deviceModel",
                            smoke_x_config.num_probes == 2 ? "X2" : "X4");
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (json_str) {
        httpd_resp_sendstr(req, json_str);
        free(json_str);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* Handler for getting data/history status */
static esp_err_t data_get_handler(httpd_req_t *req) {
    char *json_str = smoke_x_get_data_json();
    if (json_str) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json_str);
        return ESP_OK;
    }
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Unable to generate history");
    return ESP_FAIL;
}

/* Handler for getting wifi config */
static esp_err_t wifi_config_get_handler(httpd_req_t *req) {
    app_wifi_params_t app_wifi_params;
    app_wifi_get_params(&app_wifi_params);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "mode", app_wifi_params.mode);
    cJSON_AddNumberToObject(root, "authType", app_wifi_params.auth_type);
    cJSON_AddStringToObject(root, "password", "");
    cJSON_AddStringToObject(root, "ssid", app_wifi_params.ssid);
    cJSON_AddStringToObject(root, "username", app_wifi_params.username);
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (json_str) {
        httpd_resp_sendstr(req, json_str);
        free(json_str);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* Handler for setting wifi config */
static esp_err_t wifi_config_set_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    char *param_str = cJSON_Print(root);
    ESP_LOGI(TAG, "Setting wifi params: \n%s", param_str);

    app_wifi_params_t app_wifi_params;

    // TODO check that key exists cJSON_HasObjectItem
    app_wifi_params.mode = cJSON_GetObjectItem(root, "mode")->valueint;
    app_wifi_params.auth_type = cJSON_GetObjectItem(root, "authType")->valueint;
    strncpy(app_wifi_params.ssid,
            cJSON_GetObjectItem(root, "ssid")->valuestring,
            sizeof(app_wifi_params.ssid));
    strncpy(app_wifi_params.username,
            cJSON_GetObjectItem(root, "username")->valuestring,
            sizeof(app_wifi_params.username));
    strncpy(app_wifi_params.password,
            cJSON_GetObjectItem(root, "password")->valuestring,
            sizeof(app_wifi_params.password));

    cJSON_Delete(root);
    free(param_str);
    httpd_resp_sendstr(req, "Post control value successfully");
    app_wifi_set_params(&app_wifi_params);

    return ESP_OK;
}

/* Handler for getting mqtt config */
static esp_err_t mqtt_config_get_handler(httpd_req_t *req) {
    app_mqtt_params_t app_mqtt_params;
    app_mqtt_get_params(&app_mqtt_params);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, APP_MQTT_URI,
                            app_mqtt_params.uri ? app_mqtt_params.uri : "");
    cJSON_AddStringToObject(
        root, APP_MQTT_IDENTITY,
        app_mqtt_params.identity ? app_mqtt_params.identity : "");
    cJSON_AddStringToObject(
        root, APP_MQTT_USERNAME,
        app_mqtt_params.username ? app_mqtt_params.username : "");
    cJSON_AddStringToObject(
        root, APP_MQTT_PASSWORD,
        app_mqtt_params.password ? app_mqtt_params.password : "");
    cJSON_AddStringToObject(
        root, APP_MQTT_CA_CERT,
        app_mqtt_params.ca_cert ? app_mqtt_params.ca_cert : "");
    cJSON_AddBoolToObject(root, APP_MQTT_ENABLED, app_mqtt_params.enabled);
    cJSON_AddBoolToObject(root, APP_MQTT_HA_DISCOVERY,
                          app_mqtt_params.ha_discovery);
    cJSON_AddStringToObject(
        root, APP_MQTT_HA_BASE_TOPIC,
        app_mqtt_params.ha_base_topic ? app_mqtt_params.ha_base_topic : "");
    cJSON_AddStringToObject(
        root, APP_MQTT_HA_STATUS_TOPIC,
        app_mqtt_params.ha_status_topic ? app_mqtt_params.ha_status_topic : "");
    cJSON_AddStringToObject(root, APP_MQTT_HA_BIRTH_PAYLOAD,
                            app_mqtt_params.ha_birth_payload
                                ? app_mqtt_params.ha_birth_payload
                                : "");
    cJSON_AddStringToObject(
        root, APP_MQTT_STATE_TOPIC,
        app_mqtt_params.state_topic ? app_mqtt_params.state_topic : "");
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (json_str) {
        httpd_resp_sendstr(req, json_str);
        free(json_str);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* Handler for setting mqtt config */
static esp_err_t mqtt_config_set_handler(httpd_req_t *req) {
    // TODO: Factor out boilerplate for getters
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    char *param_str = cJSON_Print(root);
    ESP_LOGI(TAG, "Setting mqtt params: \n%s", param_str);

    app_mqtt_params_t app_mqtt_params = {0};

    json_check_strncpy(root, &app_mqtt_params.uri, APP_MQTT_URI,
                       APP_MQTT_MAX_URI_LEN);
    json_check_strncpy(root, &app_mqtt_params.identity, APP_MQTT_IDENTITY,
                       APP_MQTT_MAX_IDENTITY_LEN);
    json_check_strncpy(root, &app_mqtt_params.username, APP_MQTT_USERNAME,
                       APP_MQTT_MAX_USERNAME_LEN);
    json_check_strncpy(root, &app_mqtt_params.password, APP_MQTT_PASSWORD,
                       APP_MQTT_MAX_PASSWORD_LEN);
    json_check_strncpy(root, &app_mqtt_params.ca_cert, APP_MQTT_CA_CERT,
                       APP_MQTT_MAX_CERT_LEN);
    app_mqtt_params.enabled =
        cJSON_GetObjectItem(root, APP_MQTT_ENABLED)->valueint;
    app_mqtt_params.ha_discovery =
        cJSON_GetObjectItem(root, APP_MQTT_HA_DISCOVERY)->valueint;
    json_check_strncpy(root, &app_mqtt_params.ha_base_topic,
                       APP_MQTT_HA_BASE_TOPIC, APP_MQTT_MAX_TOPIC_LEN);
    json_check_strncpy(root, &app_mqtt_params.ha_status_topic,
                       APP_MQTT_HA_STATUS_TOPIC, APP_MQTT_MAX_TOPIC_LEN);
    json_check_strncpy(root, &app_mqtt_params.ha_birth_payload,
                       APP_MQTT_HA_BIRTH_PAYLOAD, APP_MQTT_MAX_TOPIC_LEN);
    json_check_strncpy(root, &app_mqtt_params.state_topic, APP_MQTT_STATE_TOPIC,
                       APP_MQTT_MAX_TOPIC_LEN);

    cJSON_Delete(root);
    free(param_str);
    httpd_resp_sendstr(req, "Post control value successfully");

    return app_mqtt_set_params(&app_mqtt_params);
}

/* Handler for commands */
static esp_err_t cmd_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to post command");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    char *cmd;
    char *param_str = cJSON_Print(root);
    ESP_LOGI(TAG, "Applying command: \n%s", param_str);
    free(param_str);

    if (cJSON_HasObjectItem(root, "command")) {
        cmd = cJSON_GetObjectItem(root, "command")->valuestring;
        if (strcmp(cmd, "startTx") == 0) {
            app_lora_tx_msg_t task_arg;
            task_arg.repeat_interval_ms = 0;
            task_arg.sending_task = xTaskGetCurrentTaskHandle();
            if (cJSON_HasObjectItem(root, "message")) {
                task_arg.msg =
                    cJSON_GetObjectItem(root, "message")->valuestring;
            }
            if (cJSON_HasObjectItem(root, "repeatInterval")) {
                task_arg.repeat_interval_ms =
                    cJSON_GetObjectItem(root, "repeatInterval")->valueint;
            }
            app_lora_start_tx(&task_arg);
            // wait for the transmit task to copy the message
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        } else if (strcmp(cmd, "stopTx") == 0) {
            app_lora_stop_tx(0, NULL);
        } else if (strcmp(cmd, "startRx") == 0) {
            app_lora_start_rx(NULL);
        } else if (strcmp(cmd, "stopRx") == 0) {
            app_lora_stop_rx(NULL);
        } else if (strcmp(cmd, "unpair") == 0) {
            smoke_x_sync();
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                "Unknown command received");
            return ESP_FAIL;
        }
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Command successfully applied");
    return ESP_OK;
}

esp_err_t app_web_ui_start() {
#if APP_DEBUG > 0
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    init_fs();
    REST_CHECK(CONFIG_WEB_MOUNT_POINT, "wrong base path", err);
    rest_server_context_t *rest_context =
        calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, CONFIG_WEB_MOUNT_POINT,
            sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;

    ESP_LOGI(TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed",
               err_start);

    /* URI handler for status getter */
    httpd_uri_t data_get_uri = {.uri = "/data",
                                .method = HTTP_GET,
                                .handler = data_get_handler,
                                .user_ctx = rest_context};
    httpd_register_uri_handler(server, &data_get_uri);

    /* URI handler for pairing status getter */
    httpd_uri_t pairing_status_get_uri = {.uri = "/pairing-status",
                                          .method = HTTP_GET,
                                          .handler = pairing_status_get_handler,
                                          .user_ctx = rest_context};
    httpd_register_uri_handler(server, &pairing_status_get_uri);

    /* URI handler for wifi config getter */
    httpd_uri_t wifi_config_get_uri = {.uri = "/wlan-config",
                                       .method = HTTP_GET,
                                       .handler = wifi_config_get_handler,
                                       .user_ctx = rest_context};
    httpd_register_uri_handler(server, &wifi_config_get_uri);

    /* URI handler for wifi config setter */
    httpd_uri_t wifi_config_set_post_uri = {.uri = "/wlan-config",
                                            .method = HTTP_POST,
                                            .handler = wifi_config_set_handler,
                                            .user_ctx = rest_context};
    httpd_register_uri_handler(server, &wifi_config_set_post_uri);

    /* URI handler for RF params getter */
    httpd_uri_t rf_params_get_uri = {.uri = "/rf-params",
                                     .method = HTTP_GET,
                                     .handler = rf_params_get_handler,
                                     .user_ctx = rest_context};
    httpd_register_uri_handler(server, &rf_params_get_uri);

    /* URI handler for RF params setter */
    httpd_uri_t rf_params_set_post_uri = {.uri = "/rf-params",
                                          .method = HTTP_POST,
                                          .handler = rf_params_set_handler,
                                          .user_ctx = rest_context};
    httpd_register_uri_handler(server, &rf_params_set_post_uri);

    /* URI handler for commands */
    httpd_uri_t cmd_uri = {.uri = "/cmd",
                           .method = HTTP_POST,
                           .handler = cmd_handler,
                           .user_ctx = rest_context};
    httpd_register_uri_handler(server, &cmd_uri);

    /* URI handler for mqtt config getter */
    httpd_uri_t mqtt_config_get_uri = {.uri = "/mqtt-config",
                                       .method = HTTP_GET,
                                       .handler = mqtt_config_get_handler,
                                       .user_ctx = rest_context};
    httpd_register_uri_handler(server, &mqtt_config_get_uri);

    /* URI handler for mqtt config setter */
    httpd_uri_t mqtt_config_set_post_uri = {.uri = "/mqtt-config",
                                            .method = HTTP_POST,
                                            .handler = mqtt_config_set_handler,
                                            .user_ctx = rest_context};
    httpd_register_uri_handler(server, &mqtt_config_set_post_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {.uri = "/*",
                                  .method = HTTP_GET,
                                  .handler = rest_common_get_handler,
                                  .user_ctx = rest_context};
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
