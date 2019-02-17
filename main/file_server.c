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


/* Index HTML handler */

static esp_err_t index_html_get_handler_actual(httpd_req_t *req)
{
	extern const unsigned char f_start[] asm("_binary_index_html_start");
	extern const unsigned char f_end[]   asm("_binary_index_html_end");
	const size_t f_size = (f_end - f_start);
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)f_start, f_size);
    return ESP_OK;
}

static esp_err_t download_code(httpd_req_t *req)
{
	extern const unsigned char f_j_start[] asm("_binary_code_js_start");
	extern const unsigned char f_j_end[]   asm("_binary_code_js_end");
	const size_t f_j_size = (f_j_end - f_j_start);
	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)f_j_start, f_j_size);
    return ESP_OK;
}


static esp_err_t styles_css_handler(httpd_req_t *req)
{
	extern const unsigned char f_s_start[] asm("_binary_styles_css_start");
	extern const unsigned char f_s_end[]   asm("_binary_styles_css_end");
	const size_t f_s_size = (f_s_end - f_s_start);
	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)f_s_start, f_s_size);
    return ESP_OK;
}


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


#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req)
{
    if (IS_FILE_EXT(req->uri, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(req->uri, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(req->uri, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t http_resp_file(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(filepath, ((struct file_server_data *)req->user_ctx)->base_path);

    /* Concatenate the requested file path */
    strcat(filepath, req->uri);
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* If file doesn't exist respond with 404 Not Found */
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* If file exists but unable to open respond with 500 Server Error */
        httpd_resp_set_status(req, "500 Server Error");
        httpd_resp_sendstr(req, "Failed to read existing file!");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filepath, file_stat.st_size);
    set_content_type_from_file(req);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        /* Send the buffer contents as HTTP response chunk */
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
            fclose(fd);
            ESP_LOGE(TAG, "File sending failed!");
            /* Abort sending file */
            httpd_resp_sendstr_chunk(req, NULL);
            /* Send error message with status code */
            httpd_resp_set_status(req, "500 Server Error");
            httpd_resp_sendstr(req, "Failed to send file!");
            return ESP_OK;
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    // Check if the target is a directory
    if (req->uri[strlen(req->uri) - 1] == '/') {
        // In so, send an html with directory listing
    	index_html_get_handler_actual(req);
    } else {
        // Else send the file
        http_resp_file(req);
    }
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = req->uri + sizeof("/upload") - 1;

    /* Filename cannot be empty or have a trailing '/' */
    if (strlen(filename) == 0 || filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid file name : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_set_status(req, "400 Bad Request");
        /* Send failure reason */
        httpd_resp_sendstr(req, "Invalid file name!");
        return ESP_OK;
    }

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(filepath, ((struct file_server_data *)req->user_ctx)->base_path);

    /* Concatenate the requested file path */
    strcat(filepath, filename);
    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* If file exists respond with 400 Bad Request */
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "File already exists!");
        return ESP_OK;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "File size must be less than "
                           MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* If file creation failed, respond with 500 Server Error */
        httpd_resp_set_status(req, "500 Server Error");
        httpd_resp_sendstr(req, "Failed to create file!");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Return failure reason with status code */
            httpd_resp_set_status(req, "500 Server Error");
            httpd_resp_sendstr(req, "Failed to receive file!");
            return ESP_OK;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            httpd_resp_set_status(req, "500 Server Error");
            httpd_resp_sendstr(req, "Failed to write file to storage!");
            return ESP_OK;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = req->uri + sizeof("/upload") - 1;

    /* Filename cannot be empty or have a trailing '/' */
    if (strlen(filename) == 0 || filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid file name : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_set_status(req, "400 Bad Request");
        /* Send failure reason */
        httpd_resp_sendstr(req, "Invalid file name!");
        return ESP_OK;
    }

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(filepath, ((struct file_server_data *)req->user_ctx)->base_path);

    /* Concatenate the requested file path */
    strcat(filepath, filename);
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* If file does not exist respond with 400 Bad Request */
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "File does not exist!");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico */
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


    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file (except index.html)
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);


    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

    return ESP_OK;
}
