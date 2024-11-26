#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"

#include "common.h"
#include "gap.h"

// UART configuration
#define UART_PORT_0      UART_NUM_0
#define UART_PORT_1      UART_NUM_1
#define UART0_TX_PIN        (21)
#define UART0_RX_PIN        (20)
#define UART1_TX_PIN        (1)
#define UART1_RX_PIN        (0)
#define UART_BAUD_RATE     9600
#define UART_BUF_SIZE      1024

// Laser sensor command definitions
static const uint8_t CMD_OPEN_LASER[]      = {0x80, 0x06, 0x05, 0x01, 0x74};   // Open laser
static const uint8_t CMD_GET_SINGLE[]      = {0x80, 0x06, 0x02, 0x78};         // Single measurement
static const uint8_t CMD_RANGE_10M[]       = {0xFA, 0x04, 0x09, 0x0A, 0xEF};   // Set range to 10m
static const uint8_t CMD_RESOLUTION_1MM[]  = {0xFA, 0x04, 0x0C, 0x01, 0xF5};   // Set resolution to 1mm

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);
void update_advertising_data(void);
static uint8_t own_addr_type;

// Declare global variables to store distance measurements
char distance_measurement_0[10] = "888.888";
char distance_measurement_1[10] = "888.888";

void init_uart(int UART_PORT, int TX_PIN, int RX_PIN)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Install UART driver
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
}

void configure_sensor(int UART_PORT)
{
    // Open the laser
    printf("Turning on the laser...");
    uart_write_bytes(UART_PORT, (const char*)CMD_OPEN_LASER, sizeof(CMD_OPEN_LASER));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set resolution to 1mm
    printf("Setting resolution to 1mm...");
    uart_write_bytes(UART_PORT, (const char*)CMD_RESOLUTION_1MM, sizeof(CMD_RESOLUTION_1MM));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set range to 10m
    printf("Setting range to 10m...");
    uart_write_bytes(UART_PORT, (const char*)CMD_RANGE_10M, sizeof(CMD_RANGE_10M));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Clear read buffer
    uint8_t dist_raw[30];
    uart_read_bytes(UART_PORT, dist_raw, sizeof(dist_raw), pdMS_TO_TICKS(1000));
}

void get_distance(int UART_PORT, char *distance_measurement)
{
    uint8_t dist_raw[20];
    int len;

    printf("Sending distance measurement command.\n");
    uart_write_bytes(UART_PORT, (const char*)CMD_GET_SINGLE, sizeof(CMD_GET_SINGLE));
    vTaskDelay(pdMS_TO_TICKS(500));  // Increased delay for response

    len = uart_read_bytes(UART_PORT, dist_raw, sizeof(dist_raw), pdMS_TO_TICKS(1000));
    if (len > 0) {
        printf("Raw Data: ");
        for (int i = 0; i < len; i++) {
            printf("%02X ", dist_raw[i]);
        }
        printf("\n");

        // Extract printable ASCII characters
        int idx = 0;
        for (int i = 0; i < len && idx < 9; i++) { // limit to 9 characters to fit in 10-byte buffer
            if ((dist_raw[i] >= 32) && (dist_raw[i] <= 126)) { // printable ASCII
                distance_measurement[idx++] = dist_raw[i];
            }
        }
        distance_measurement[idx] = '\0';

        ESP_LOGI(TAG, "dist_str length: %i", sizeof(distance_measurement));

        printf("Distance: %s \n", distance_measurement);

    } else {
        printf("Failed to read distance.\n");
        strcpy(distance_measurement, "999.999");
    }
}

void save_readings(void *pvParameters)
{
    int interval_seconds = 10;
    int num_readings = 100;

    configure_sensor(UART_PORT_0);
    configure_sensor(UART_PORT_1);
    for (int i = 0; i < num_readings; i++) {
        printf("\nReading %d of %d \n", i + 1, num_readings);
        get_distance(UART_PORT_0, distance_measurement_0);
        get_distance(UART_PORT_1, distance_measurement_1);

        // Update advertising data with new measurements
        update_advertising_data();

        vTaskDelay(pdMS_TO_TICKS(interval_seconds * 1000));
    }
    vTaskDelete(NULL);
}

static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void) {
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void) {
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

void update_advertising_data()
{
    int rc;
    uint8_t adv_data[31];
    uint8_t adv_data_len = 0;

    // Build Flags field (Mandatory)
    adv_data[adv_data_len++] = 2; // Length of Flags field (Type + Data)
    adv_data[adv_data_len++] = BLE_HS_ADV_TYPE_FLAGS; // AD Type: Flags (0x01)
    adv_data[adv_data_len++] = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP; // Flags data

    // Include Complete Local Name
    const char *device_name = "LBcn";
    uint8_t device_name_len = strlen(device_name);

    // Ensure the device name fits in the remaining advertising data space
    int remaining_space = 31 - adv_data_len;
    if (device_name_len + 2 <= remaining_space) { // +2 for Length and Type bytes
        adv_data[adv_data_len++] = device_name_len + 1; // Length of this field (Type + Data)
        adv_data[adv_data_len++] = BLE_HS_ADV_TYPE_COMP_NAME; // AD Type: Complete Local Name (0x09)

        memcpy(&adv_data[adv_data_len], device_name, device_name_len);
        adv_data_len += device_name_len;
    } else {
        ESP_LOGE(TAG, "Device name is too long to fit in advertising data");
        return;
    }

    // Prepare Measurements as String Data
    char measurements_str[23]; // 9 bytes each + separator + null terminator
    snprintf(measurements_str, sizeof(measurements_str), "%s:%s", distance_measurement_0, distance_measurement_1);
    ESP_LOGI(TAG, "Advertising measurements: %s", measurements_str);

    uint8_t custom_data_len = strlen(measurements_str);

    remaining_space = 31 - adv_data_len;
    if (custom_data_len + 2 <= remaining_space) { // +2 for Length and Type bytes
        adv_data[adv_data_len++] = custom_data_len + 1; // Length of this field (Type + Data)
        adv_data[adv_data_len++] = 0xFF; // AD Type: Manufacturer Specific Data (0xFF), can be used for custom data

        // Include measurements
        memcpy(&adv_data[adv_data_len], measurements_str, custom_data_len);
        adv_data_len += custom_data_len;
    } else {
        ESP_LOGE(TAG, "Not enough space for measurements in advertising data");
        return;
    }

    // Stop current advertising
    rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising, error code: %d", rc);
    }

    // Update the advertising data
    rc = ble_gap_adv_set_data(adv_data, adv_data_len);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data, error code: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising data updated with new measurements: %s", measurements_str);
    }

    // Restart advertising
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON; // Non-connectable advertising
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // General discoverable mode

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    ESP_LOGI(TAG, adv_params);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to restart advertising, error code: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising restarted with updated data");
    }
}

void app_main()
{

        /* Local variables */
    int rc = 0;
    esp_err_t ret = ESP_OK;

    /* NVS flash initialization */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* NimBLE host stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    init_uart(UART_PORT_0, UART0_TX_PIN, UART0_RX_PIN);
    init_uart(UART_PORT_1, UART1_TX_PIN, UART1_RX_PIN);
    
    xTaskCreate(save_readings, "save_readings", 4096, NULL, 5, NULL);

    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    return;
}
