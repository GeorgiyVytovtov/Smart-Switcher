#include "captive_portal.h"
#include "wifi_manager.h"
#include "html_pages.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"

static const char *TAG = "CAPTIVE";

static httpd_handle_t http_server = NULL;

bool captive_portal_is_running(void)
{
    return http_server != NULL;
}

void captive_portal_stop(void)
{
    if (http_server)
    {
        ESP_LOGI(TAG, "Stopping HTTP server...");
        httpd_stop(http_server);
        http_server = NULL;
    }
}

static void url_decode_inplace(char *str)
{
    char *read_ptr = str, *write_ptr = str;
    while (*read_ptr)
    {
        if (*read_ptr == '+')
        {
            *write_ptr++ = ' ';
            read_ptr++;
        }
        else if (*read_ptr == '%' && read_ptr[1] && read_ptr[2])
        {
            char hex_buf[3] = {read_ptr[1], read_ptr[2], 0};
            *write_ptr++ = (char)strtol(hex_buf, NULL, 16);
            read_ptr += 3;
        }
        else
        {
            *write_ptr++ = *read_ptr++;
        }
    }
    *write_ptr = '\0';
}

static void parse_form_urlencoded(char *buf, char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len)
{
    out_ssid[0] = 0;
    out_pass[0] = 0;
    char *ssid_ptr = strstr(buf, "ssid=");
    if (ssid_ptr)
    {
        ssid_ptr += 5;
        size_t i = 0;
        while (*ssid_ptr && *ssid_ptr != '&' && i + 1 < ssid_len)
            out_ssid[i++] = *ssid_ptr++;
        out_ssid[i] = 0;
        url_decode_inplace(out_ssid);
    }

    char *pass_ptr = strstr(buf, "pass=");
    if (!pass_ptr)
        pass_ptr = strstr(buf, "password=");
    if (pass_ptr)
    {
        if (strncmp(pass_ptr, "password=", 9) == 0)
            pass_ptr += 9;
        else
            pass_ptr += 5;
        size_t i = 0;
        while (*pass_ptr && *pass_ptr != '&' && i + 1 < pass_len)
            out_pass[i++] = *pass_ptr++;
        out_pass[i] = 0;
        url_decode_inplace(out_pass);
    }
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, CP_HTML_FORM, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_send_404(req);
    return ESP_OK;
}

static esp_err_t post_connect_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 1024)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }
    char *buf = malloc(content_len + 1);
    if (!buf)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    int ret = 0, received = 0;
    while (received < content_len)
    {
        ret = httpd_req_recv(req, buf + received, content_len - received);
        if (ret <= 0)
        {
            free(buf);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = 0;

    wifi_credentials_t cred;
    parse_form_urlencoded(buf, cred.ssid, sizeof(cred.ssid), cred.pass, sizeof(cred.pass));
    free(buf);

    ESP_LOGI(TAG, "Form: ssid='%s'", cred.ssid);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, RESP, HTTPD_RESP_USE_STRLEN);

    change_wifi_mode(STA_MODE, &cred);

    return ESP_OK;
}

static esp_err_t wildcard_get_handler(httpd_req_t *req)
{
    return root_get_handler(req);
}

void captive_portal_start(void)
{
    if (http_server)
        return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 8;
    config.max_open_sockets = 4;
    config.server_port = 80;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.lru_purge_enable = true;

    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&http_server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(http_server, &root_uri);

    httpd_uri_t connect_uri = {.uri = "/connect", .method = HTTP_POST, .handler = post_connect_handler, .user_ctx = NULL};
    httpd_register_uri_handler(http_server, &connect_uri);

    httpd_uri_t favicon_uri = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler, .user_ctx = NULL};
    httpd_register_uri_handler(http_server, &favicon_uri);

    httpd_uri_t wildcard_uri = {.uri = "/*", .method = HTTP_GET, .handler = wildcard_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(http_server, &wildcard_uri);

    ESP_LOGI(TAG, "HTTP server started");
}
