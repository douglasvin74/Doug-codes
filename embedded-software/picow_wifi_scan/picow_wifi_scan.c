#include <stdio.h>
#include <string.h>
#include "mqtt.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "hardware/vreg.h"
#include "hardware/clocks.h"

// === CONFIGURAÇÕES ===
#define WIFI_SSID "Ado"
#define WIFI_PASS "rivaldo1973"
#define MQTT_BROKER_IP "localhost"  // Use o IP da máquina local com o broker
#define MQTT_PORT 1883
#define MQTT_TOPIC "wifi/scan"

// === TAMANHOS DE BUFFER ===
#define SEND_BUF_SIZE 2048
#define RECV_BUF_SIZE 2048

// === BUFFERS MQTT ===
static uint8_t sendbuf[SEND_BUF_SIZE];
static uint8_t recvbuf[RECV_BUF_SIZE];

// === SOCKET ===
#include <lwip/sockets.h>
static int mqtt_socket = -1;

// === MQTT CLIENT ===
static struct mqtt_client client;

// === FUNÇÃO CALLBACK DO SCAN ===
static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "SSID: %s, RSSI: %d, Channel: %d, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 result->ssid, result->rssi, result->channel,
                 result->bssid[0], result->bssid[1], result->bssid[2],
                 result->bssid[3], result->bssid[4], result->bssid[5]);

        printf("%s\n", msg);

        // Publica via MQTT
        mqtt_publish(&client, MQTT_TOPIC, msg, strlen(msg), MQTT_PUBLISH_QOS_0);
    }
    return 0;
}

// === FUNÇÃO PARA CONECTAR AO BROKER MQTT ===
bool connect_mqtt() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MQTT_PORT);
    addr.sin_addr.s_addr = ipaddr_addr(MQTT_BROKER_IP);

    mqtt_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (mqtt_socket < 0) {
        printf("Erro ao criar socket MQTT\n");
        return false;
    }

    if (connect(mqtt_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Erro ao conectar ao broker MQTT\n");
        return false;
    }

    mqtt_init(&client, mqtt_socket, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), NULL);
    mqtt_connect(&client,
                 "picow-client", NULL, NULL, 0, NULL, NULL, 0, 400);

    mqtt_sync(&client); // envia o CONNECT imediatamente
    printf("Conectado ao MQTT broker!\n");

    return true;
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // tempo para abrir o terminal

    if (cyw43_arch_init()) {
        printf("Falha ao iniciar Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Erro ao conectar ao Wi-Fi\n");
        return 1;
    }

    printf("Wi-Fi conectado!\n");

    // Conecta ao MQTT broker
    if (!connect_mqtt()) {
        printf("Erro na conexão MQTT\n");
        return 1;
    }

    absolute_time_t scan_time = nil_time;
    bool scan_in_progress = false;

    while (true) {
        mqtt_sync(&client); // processa o cliente MQTT (mantém a conexão viva)

        if (absolute_time_diff_us(get_absolute_time(), scan_time) < 0) {
            if (!scan_in_progress) {
                cyw43_wifi_scan_options_t scan_options = {0};
                int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
                if (err == 0) {
                    printf("\nIniciando varredura Wi-Fi\n");
                    scan_in_progress = true;
                } else {
                    printf("Falha ao iniciar scan: %d\n", err);
                    scan_time = make_timeout_time_ms(10000);
                }
            } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
                scan_time = make_timeout_time_ms(10000);
                scan_in_progress = false;
            }
        }

#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(scan_time);
#else
        sleep_ms(500);
#endif
    }

    close(mqtt_socket);
    cyw43_arch_deinit();
    return 0;
}
