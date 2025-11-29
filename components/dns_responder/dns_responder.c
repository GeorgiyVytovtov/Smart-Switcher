#include "dns_responder.h"
#include "shearch_component.h"

#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"


#define DNS_HEADER_SIZE 12
#define DNS_PORT 53

#define CAPTIVE_PORTAL_IP "192.168.4.1" 

static const char *TAG = "DNS_RESPONDER";

static TaskHandle_t dns_task_handle = NULL;
static volatile bool dns_task_running = false;

void dns_task(void *pvParameter);

bool dns_responder_is_running(void)
{
    return dns_task_handle != NULL;
}

void dns_responder_start(void)
{
    if(dns_task_handle)
    {
        ESP_LOGW(TAG, "DNS responder already running");
        return;
    }
    if(dns_task_running)
    {
        ESP_LOGW(TAG, "DNS task is already starting");
        return;
    }
    dns_task_running = true;
    ESP_LOGI(TAG, "Starting DNS task...");
    
    if(xTaskCreate(dns_task, "dns_task", 4096, NULL, 5, &dns_task_handle)!= pdPASS)
    {
        ESP_LOGE(TAG, "Could not create DNS task");
        dns_task_handle = NULL;
    }
}

void dns_responder_stop(void)
{
    if (dns_task_handle)
    {
        ESP_LOGI(TAG, "Stopping DNS task...");
        dns_task_running = false;

        while (dns_task_handle != NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void dns_task(void *pvParameter)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "DNS socket create failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DNS_PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 500 ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "DNS responder started on UDP/53");

    uint8_t rxbuf[512];
    while(dns_task_running)
    {
        struct sockaddr_in clientAddr;
        socklen_t client_len = sizeof(clientAddr);
        ssize_t recvLen = recvfrom(sock, rxbuf, sizeof(rxbuf), 0, (struct sockaddr *)&clientAddr, &client_len);
        if (recvLen < DNS_HEADER_SIZE)
            continue;

        uint8_t txbuf[512];
        memset(txbuf, 0, sizeof(txbuf));
        txbuf[0] = rxbuf[0];
        txbuf[1] = rxbuf[1];
        txbuf[2] = 0x81;
        txbuf[3] = 0x80;
        txbuf[4] = rxbuf[4];
        txbuf[5] = rxbuf[5];
        txbuf[6] = 0x00;
        txbuf[7] = 0x01;

        size_t qend = DNS_HEADER_SIZE;
        while (qend < (size_t)recvLen && rxbuf[qend] != 0)
            qend++;
        if (qend >= (size_t)recvLen)
            continue;
        qend++;
        if (qend + 4 > (size_t)recvLen)
            continue;
        size_t qlen = qend + 4 - DNS_HEADER_SIZE;
        memcpy(&txbuf[DNS_HEADER_SIZE], &rxbuf[DNS_HEADER_SIZE], qlen);
        size_t txOffset = DNS_HEADER_SIZE + qlen;

        txbuf[txOffset++] = 0xC0;
        txbuf[txOffset++] = 0x0C;
        txbuf[txOffset++] = 0x00; // TYPE A
        txbuf[txOffset++] = 0x01;
        txbuf[txOffset++] = 0x00; // CLASS IN
        txbuf[txOffset++] = 0x01;
        txbuf[txOffset++] = 0x00; // TTL 60s
        txbuf[txOffset++] = 0x00;
        txbuf[txOffset++] = 0x00;
        txbuf[txOffset++] = 0x3C;
        txbuf[txOffset++] = 0x00; // RDLENGTH = 4
        txbuf[txOffset++] = 0x04;

        ip4_addr_t ip_addr;
        ip4addr_aton(CAPTIVE_PORTAL_IP, &ip_addr);
        memcpy(&txbuf[txOffset], &ip_addr.addr, 4);
        txOffset += 4;

        sendto(sock, txbuf, txOffset, 0, (struct sockaddr *)&clientAddr, client_len);
    }
    close(sock);
    dns_task_handle = NULL;
    ESP_LOGI(TAG, "DNS responder stopped");
    vTaskDelete(NULL);
}
