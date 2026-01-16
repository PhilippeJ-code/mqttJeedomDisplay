#include "stdio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "mqtt_client.h"

#include "json.h"

#include "WaveShare.h"

#include "libs/lodepng/lodepng.h"

// Wifi Manager
//

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <sys/stat.h>
#include <dirent.h>

#include "wifi_manager.h"
#include "http_app.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define MAX_FILE_SIZE (400 * 1024)
#define MAX_FILE_SIZE_STR "400KB"

#define SCRATCH_BUFSIZE 8192

struct file_server_data
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
};
static struct file_server_data *server_data = NULL;

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "main";

const char *base_path = "/spiffs";
const char *dirpath = "/spiffs/";

static char current_path[FILE_PATH_MAX] = "/spiffs/";
static char file_path[FILE_PATH_MAX];
static char dir_path[FILE_PATH_MAX];

LV_FONT_DECLARE(my_font_montserrat_18);
LV_FONT_DECLARE(my_font_montserrat_24);
LV_FONT_DECLARE(my_font_montserrat_30);

LV_IMG_DECLARE(img_lvgl_logo);

esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static bool wifiIsConnected = false;
static bool mqttFirstConnected = false;
static bool mqttIsConnected = false;
static bool canContinue = false;

void snapshot_event_cb(lv_event_t *e)
{

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        lv_draw_buf_t *snapshot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);

        if (snapshot)
        {
            uint8_t *ptr = snapshot->data;
            for (int w = 0; w < snapshot->header.w; w++)
            {
                for (int h = 0; h < snapshot->header.h; h++)
                {
                    uint8_t swap = *ptr;
                    *ptr = *(ptr + 2);
                    *(ptr + 2) = swap;

                    ptr += 3;
                }
            }
            lodepng_encode24_file("A:/snapshot.png", snapshot->data, snapshot->header.w, snapshot->header.h);
            lv_draw_buf_destroy(snapshot);
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:

        mqttIsConnected = true;
        if ( mqttFirstConnected == true)
            jsonSubscribe();
        else
            mqttFirstConnected = true;

        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        break;
    case MQTT_EVENT_DISCONNECTED:

        mqttIsConnected = false;

        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        jsonDataEvent(event);

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static bool mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {};
    jsonParametersMqtt(&mqtt_cfg);

    if ((mqtt_cfg.broker.address.uri == NULL) ||
        (mqtt_cfg.credentials.username == NULL) ||
        (mqtt_cfg.credentials.authentication.password == NULL))
        return false;

    mqtt_cfg.network.timeout_ms = 3000;
    mqtt_cfg.session.keepalive = 3000;
    mqtt_cfg.task.priority = 5;

    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    return true;
}

// Compter le nombre d'occurrences d'un caractère dans une chaîne
//
uint8_t countChars(char *sString, char cChar)
{
    uint8_t nCount = 0;
    while (*sString)
    {
        if (*sString == cChar)
        {
            nCount++;
        }
        sString++;
    }
    return nCount;
}

// Générer la page HTML du répertoire courant
//
static esp_err_t http_resp_dir_html(httpd_req_t *req)
{
    char entrypath[FILE_PATH_MAX];
    char dirname[FILE_PATH_MAX];
    char entrysize[16];

    struct dirent *entry;
    struct stat entry_stat;

    char *noms[100];
    char *noms_entiers[100];
    int count = 0;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    extern const unsigned char html_header_start[] asm("_binary_html_header_html_start");
    extern const unsigned char html_header_end[] asm("_binary_html_header_html_end");
    const size_t html_header_size = (html_header_end - html_header_start);
    httpd_resp_send_chunk(req, (const char *)html_header_start, html_header_size);

    // Démarrer le tableau des répertoires et fichiers
    //
    httpd_resp_sendstr_chunk(req, "<h3>Répertoire courant : ");
    httpd_resp_sendstr_chunk(req, current_path);
    httpd_resp_sendstr_chunk(req, "</h3>");
    httpd_resp_sendstr_chunk(req,
                             "<div class=\"classtable\">"
                             "<table>"
                             "<col width=\"60%\"/><col width=\"30%\"/>"
                             "<thead><tr><td>Nom</td><td>Taille (Bytes)</td><td>Options</td></tr></thead>"
                             "<tbody>");

    // Ajouter le dossier parent ..
    //
    if (strcmp(current_path, dirpath) != 0)
    {
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"/changeupdir");
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, "<img src=\"folder.svg\">");
        httpd_resp_sendstr_chunk(req, "..");
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "</td></tr>");
    }
    while ((entry = readdir(dir)) != NULL)
    {
        // Extraire le nom du dossier et du fichier
        //
        strlcpy(dir_path, dirpath, sizeof(dir_path));
        strlcat(dir_path, entry->d_name, sizeof(dir_path));
        char *ptrFile = strrchr(dir_path, '/');
        ptrFile++;
        strlcpy(file_path, ptrFile, sizeof(file_path));
        *ptrFile = 0;

        // Filtrer les dossiers
        //
        if (strcmp(dir_path, current_path) == 0)
        {
            continue;
        }

        // Filtrer les sous-dossiers
        //
        if ((countChars(current_path, '/') + 1) != countChars(dir_path, '/'))
        {
            continue;
        }

        strcpy(dirname, dir_path);
        ptrFile = strrchr(dirname, '/');
        *ptrFile = 0;
        ptrFile = strrchr(dirname, '/');
        ptrFile++;

        // Filtrer les doublons
        //
        bool bFound = false;
        for (int i = 0; i < count; i++)
        {
            if (strcmp(ptrFile, noms[i]) == 0)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            noms[count] = strdup(ptrFile);
            noms_entiers[count] = strdup(dir_path);
            count++;
        }
    }

    for (int i = 0; i < count; i++)
    {
        // Envoyer la ligne de tableau HTML pour le dossier
        //
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"/changedir");
        httpd_resp_sendstr_chunk(req, noms_entiers[i]);
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, "<img src=\"folder.svg\">");
        httpd_resp_sendstr_chunk(req, noms[i]);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
        free(noms[i]);
        free(noms_entiers[i]);
    }
    rewinddir(dir);
    while ((entry = readdir(dir)) != NULL)
    {
        strlcpy(dir_path, dirpath, sizeof(dir_path));
        strlcat(dir_path, entry->d_name, sizeof(dir_path));
        char *ptrFile = strrchr(dir_path, '/');
        ptrFile++;
        strlcpy(file_path, ptrFile, sizeof(file_path));
        *ptrFile = 0;

        // Filtrer les dossiers
        //
        if (strcmp(dir_path, current_path) != 0)
        {
            continue;
        }

        // Obtenir les informations du fichier
        //
        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1)
        {
            ESP_LOGE(TAG, "Failed to stat : %s", entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);

        // Envoyer la ligne de tableau HTML pour le fichier
        //
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"/download/");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, "<img src=\"file.svg\">");
        httpd_resp_sendstr_chunk(req, file_path);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<a href=\"/delete/");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, "<img src=\"delete.svg\">");
        httpd_resp_sendstr_chunk(req, "</a></td></tr>");
    }
    closedir(dir);

    httpd_resp_sendstr_chunk(req, "</tbody></table></div>");

    extern const unsigned char html_footer_start[] asm("_binary_html_footer_html_start");
    extern const unsigned char html_footer_end[] asm("_binary_html_footer_html_end");
    const size_t html_footer_size = (html_footer_end - html_footer_start);
    httpd_resp_send_chunk(req, (const char *)html_footer_start, html_footer_size);

    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

// Extraire le chemin complet du fichier à partir de l'URI
//
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    ESP_LOGI(TAG, "Base_path %s", base_path);

    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);
    uint16_t len = 0;

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    const char *download = strstr(uri, "/download");
    if (download)
    {
        pathlen = MIN(pathlen, download - uri - 9);
        len = 9;
    }

    const char *delete = strstr(uri, "/delete");
    if (delete)
    {
        pathlen = MIN(pathlen, delete - uri - 7);
        len = 7;
    }

    const char *changedir = strstr(uri, "/changedir");
    if (changedir)
    {
        pathlen = MIN(pathlen, changedir - uri - 10);
        len = 10;
    }

    const char *changeupdir = strstr(uri, "/changeupdir");
    if (changeupdir)
    {
        pathlen = MIN(pathlen, changeupdir - uri - 12);
        len = 12;
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        return NULL;
    }

    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri + len, pathlen + 1);

    return dest + base_pathlen;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

// Définir le type de contenu HTTP en fonction de l'extension du fichier
//
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    return httpd_resp_set_type(req, "text/plain");
}

// Gestionnaire pour télécharger un fichier hébergé sur le serveur
//
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, server_data->base_path,
                                             req->uri, sizeof(filepath));

    ESP_LOGI(TAG, "File : %s", filepath);

    if (!filename)
    {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = server_data->scratch;
    size_t chunksize;
    do
    {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Changer de répertoire
//
static esp_err_t change_dir_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    const char *filename = get_path_from_uri(filepath, "",
                                             req->uri, sizeof(filepath));

    ESP_LOGI(TAG, "Directory : %s", filepath);

    strcpy(current_path, filepath);

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/files");
    httpd_resp_sendstr(req, "Change directory ok");

    return ESP_OK;
}

// Envoyer folder.svg
//
static esp_err_t folder_svg_get_handler(httpd_req_t *req)
{
    extern const unsigned char folder_svg_start[] asm("_binary_folder_svg_start");
    extern const unsigned char folder_svg_end[] asm("_binary_folder_svg_end");
    const size_t folder_svg_size = (folder_svg_end - folder_svg_start);

    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_send_chunk(req, (const char *)folder_svg_start, folder_svg_size);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

// Envoyer file.svg
//
static esp_err_t file_svg_get_handler(httpd_req_t *req)
{
    extern const unsigned char file_svg_start[] asm("_binary_file_svg_start");
    extern const unsigned char file_svg_end[] asm("_binary_file_svg_end");
    const size_t file_svg_size = (file_svg_end - file_svg_start);

    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_send_chunk(req, (const char *)file_svg_start, file_svg_size);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

// Envoyer delete.svg
//
static esp_err_t delete_svg_get_handler(httpd_req_t *req)
{
    extern const unsigned char delete_svg_start[] asm("_binary_delete_svg_start");
    extern const unsigned char delete_svg_end[] asm("_binary_delete_svg_end");
    const size_t delete_svg_size = (delete_svg_end - delete_svg_start);

    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_send_chunk(req, (const char *)delete_svg_start, delete_svg_size);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

// Remonter d'un niveau dans l'arborescence
//
static esp_err_t change_up_dir_get_handler(httpd_req_t *req)
{
    // Retirer le dernier niveau dans current_path
    //
    ESP_LOGI(TAG, "Current Directory before up: %s", current_path);
    char *ptrFile = strrchr(current_path, '/');
    if (ptrFile != NULL && ptrFile != current_path)
    {
        *ptrFile = 0;
        ptrFile = strrchr(current_path, '/');
        ESP_LOGI(TAG, "Current Directory after first up: %s", current_path);
        if (ptrFile != NULL && ptrFile != current_path)
        {
            ptrFile++;
            *ptrFile = 0;
        }
        else
        {
            strcpy(current_path, "/spiffs/");
        }
        ESP_LOGI(TAG, "Current Directory after up: %s", current_path);
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/files");
    httpd_resp_sendstr(req, "Change up directory ok");

    return ESP_OK;
}

// Gestionnaire pour téléverser un fichier vers le serveur
//
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, current_path,
                                             req->uri + sizeof("/upload/") - 1, sizeof(filepath));
    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/')
    {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0)
    {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    if (req->content_len > MAX_FILE_SIZE)
    {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than " MAX_FILE_SIZE_STR "!");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    char *buf = server_data->scratch;
    int received;

    int remaining = req->content_len;

    while (remaining > 0)
    {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd)))
        {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
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
    httpd_resp_set_hdr(req, "Location", "/files");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

// Gestionnaire pour supprimer un fichier hébergé sur le serveur
static esp_err_t delete_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, server_data->base_path,
                                             req->uri + sizeof("/delete") - 1, sizeof(filepath));

    ESP_LOGI(TAG, "File to delete: %s", filepath);

    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/')
    {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1)
    {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    unlink(filepath);

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/files");
    httpd_resp_sendstr(req, "File deleted successfully");

    return ESP_OK;
}

// Gestionnaire HTTP personnalisé pour les requêtes GET
//
static esp_err_t my_get_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/files") == 0)
    {
        http_resp_dir_html(req);
    }
    else if (strncmp(req->uri, "/download/", 10) == 0)
    {
        download_get_handler(req);
    }
    else if (strncmp(req->uri, "/changedir/", 10) == 0)
    {
        change_dir_get_handler(req);
    }
    else if (strncmp(req->uri, "/changeupdir/", 10) == 0)
    {
        change_up_dir_get_handler(req);
    }
    else if (strncmp(req->uri, "/delete/", 8) == 0)
    {
        delete_get_handler(req);
    }
    else if (strncmp(req->uri, "/folder.svg", 10) == 0)
    {
        folder_svg_get_handler(req);
    }
    else if (strncmp(req->uri, "/file.svg", 10) == 0)
    {
        file_svg_get_handler(req);
    }
    else if (strncmp(req->uri, "/delete.svg", 11) == 0)
    {
        delete_svg_get_handler(req);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown URI: %s", req->uri);
        httpd_resp_send_404(req);
    }

    return ESP_OK;
}

// Gestionnaire HTTP personnalisé pour les requêtes POST
//
static esp_err_t my_post_handler(httpd_req_t *req)
{
    if (strncmp(req->uri, "/upload/", 8) == 0)
    {
        upload_post_handler(req);
    }
    else
    {
        httpd_resp_send_404(req);
    }
    return ESP_OK;
}

static void WifiConnected()
{
    ESP_LOGI(TAG, "Wifi Connected, Got IP");
    wifiIsConnected = true;
}

static void continue_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        canContinue = true;
    }
}

static void wifi_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "Wifi_event_handler");
        nvsWrite("boot", 1);
        canContinue = true;
    }
}

void app_main(void)
{

    nvsInit();

    int8_t bootType = nvsRead("boot");
    if (bootType == 0)
    {
        WaveShareInit();
        reset_panel();
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (bootType == 0)
    {
        if (ret != ESP_OK)
        {
            if (ret == ESP_FAIL)
            {
                ESP_LOGE(TAG, "Failed to mount or format filesystem");
            }
            else if (ret == ESP_ERR_NOT_FOUND)
            {
                ESP_LOGE(TAG, "Failed to find SPIFFS partition");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            }
            return;
        }

        lv_obj_t *liste = NULL;

        if (hardLvglLock(-1))
        {
            lv_obj_set_style_text_font(lv_screen_active(), &my_font_montserrat_18, 0);

            liste = lv_list_create(lv_screen_active());
            lv_obj_set_size(liste, 700, 200);
            lv_obj_center(liste);

            lv_list_add_text(liste, "Démarrage du Wifi");
            hardLvglUnlock();
        }

        wifi_manager_start();
        wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, WifiConnected);
        while (wifiIsConnected == false)
        {
            vTaskDelay(10);
        }

        if (hardLvglLock(-1))
        {
            lv_list_add_text(liste, "Démarrage MQTT");
            hardLvglUnlock();
        }
        if (mqtt_app_start() == true)
        {
            while (mqttIsConnected == false)
            {
                vTaskDelay(10);
            }
        }
        else
        {
            if (hardLvglLock(-1))
            {
                lv_list_add_text(liste, "Erreur de lancement MQTT");
                hardLvglUnlock();
            }
        }

        lv_obj_t *btn1 = NULL;
        lv_obj_t *btn2 = NULL;
        lv_obj_t *label1 = NULL;
        lv_obj_t *label2 = NULL;

        if (hardLvglLock(-1))
        {
            btn1 = lv_button_create(lv_screen_active());
            lv_obj_add_event_cb(btn1, continue_event_handler, LV_EVENT_ALL, NULL);
            lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 20, -20);
            lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);

            label1 = lv_label_create(btn1);
            lv_label_set_text(label1, "Continuer");
            lv_obj_center(label1);

            btn2 = lv_button_create(lv_screen_active());
            lv_obj_add_event_cb(btn2, wifi_event_handler, LV_EVENT_ALL, NULL);
            lv_obj_align(btn2, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
            lv_obj_remove_flag(btn2, LV_OBJ_FLAG_PRESS_LOCK);

            label2 = lv_label_create(btn2);
            lv_label_set_text(label2, "Wifi");
            lv_obj_center(label2);

            hardLvglUnlock();
        }

        while (canContinue == false)
        {
            vTaskDelay(10);
        }

        if (hardLvglLock(-1))
        {
            lv_obj_del(label2);
            lv_obj_del(btn2);
            lv_obj_del(label1);
            lv_obj_del(btn1);
            lv_obj_del(liste);
            hardLvglUnlock();
        }

        jsonInit();
        jsonSubscribe();

        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            jsonUpdate();
        }
    }
    else
    {
        server_data = calloc(1, sizeof(struct file_server_data));
        if (!server_data)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for server data");
        }
        else
        {
            strlcpy(server_data->base_path, base_path,
                    sizeof(server_data->base_path));
        }

        nvsWrite("boot", 0);
        wifi_manager_start();

        http_app_set_handler_hook(HTTP_GET, &my_get_handler);
        http_app_set_handler_hook(HTTP_POST, &my_post_handler);
    }
}