/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "led.h"


/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  0xFFFE

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

static const char *TAG = "file_server";


// index.html
static esp_err_t index_html_get_handler_actual(httpd_req_t *req)
{
	extern const unsigned char f_start[] asm("_binary_index_html_start");
	extern const unsigned char f_end[]   asm("_binary_index_html_end");
	const size_t f_size = (f_end - f_start);
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)f_start, f_size);
    return ESP_OK;
}

// code.js
static esp_err_t download_code(httpd_req_t *req)
{
	extern const unsigned char f_j_start[] asm("_binary_code_js_start");
	extern const unsigned char f_j_end[]   asm("_binary_code_js_end");
	const size_t f_j_size = (f_j_end - f_j_start);
	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)f_j_start, f_j_size);
    return ESP_OK;
}

// styles.css
static esp_err_t styles_css_handler(httpd_req_t *req)
{
	extern const unsigned char f_s_start[] asm("_binary_styles_css_start");
	extern const unsigned char f_s_end[]   asm("_binary_styles_css_end");
	const size_t f_s_size = (f_s_end - f_s_start);
	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)f_s_start, f_s_size);
    return ESP_OK;
}

// rgb=100_100_100
static esp_err_t led_handler(httpd_req_t *req)
{
	//ESP_LOGI(TAG, "Got a RGB LED Request");
	//ESP_LOGI(TAG,"(%s)" ,(const char *)req->uri);
	const char * rgbStr = req->uri + sizeof("/rgb=") - 1;
	parse_rgb(rgbStr);
    httpd_resp_set_status(req, "200");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}


static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;

    /* Validate file storage base path */
    if (!base_path || strcmp(base_path, "/spiffs") != 0) {
        ESP_LOGE(TAG, "File server presently supports only '/spiffs' as base path");
        return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = tskIDLE_PRIORITY + 1;
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    /* Register handler for index.html which should redirect to / */
    httpd_uri_t index_html = {
        .uri       = "/index.html",
        .method    = HTTP_GET,
        .handler   = index_html_get_handler_actual,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &index_html);

    httpd_uri_t styles_css = {
        .uri       = "/styles.css",
        .method    = HTTP_GET,
        .handler   = styles_css_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &styles_css);

    /* Handler for URI used by browsers to get website icon */
    httpd_uri_t favicon_ico = {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = favicon_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &favicon_ico);


    httpd_uri_t rgb_get = {
        .uri       = "/rgb*",
        .method    = HTTP_GET,
        .handler   = led_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &rgb_get);


    /* URI handler for getting uploaded files */
    httpd_uri_t code_js = {
        .uri       = "/code.js",  // Match all URIs of type /path/to/file (except index.html)
        .method    = HTTP_GET,
        .handler   = download_code,
        .user_ctx  = NULL    // Pass server data as context
    };
    httpd_register_uri_handler(server, &code_js);

    return ESP_OK;
}
