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

#define LTAG "LSR"

// Laser sensor command definitions
static const uint8_t CMD_OPEN_LASER[]      = {0x80, 0x06, 0x05, 0x01, 0x74};   // Open laser
static const uint8_t CMD_GET_SINGLE[]      = {0x80, 0x06, 0x02, 0x78};         // Single measurement
//static const uint8_t CMD_RANGE_10M[]       = {0xFA, 0x04, 0x09, 0x0A, 0xEF};   // Set range to 10m
static const uint8_t CMD_RANGE_80M[]       = {0xFA, 0x04, 0x09, 0x50, 0xA9};   // Set range to 80m
static const uint8_t CMD_RANGE_150M[]      = {0xFA, 0x04, 0x09, 0x96, 0x63};   // Set range to 150m
static const uint8_t CMD_RESOLUTION_1MM[]  = {0xFA, 0x04, 0x0C, 0x01, 0xF5};   // Set resolution to 1mm

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);
//static void configure_beacon(char *long_distance, char *short_distance);
static uint8_t own_addr_type;

// Declare global variables to store distance measurements
char distance_measurement_0[10] = "888888";
char distance_measurement_1[10] = "777777";

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

void configure_long_laser()
{
    // Open the laser
    ESP_LOGI(LTAG, "Turning on the long laser...");
    uart_write_bytes(UART_PORT_0, (const char*)CMD_OPEN_LASER, sizeof(CMD_OPEN_LASER));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set resolution to 1mm
    ESP_LOGI(LTAG, "Setting long laser resolution to 1mm...");
    uart_write_bytes(UART_PORT_0, (const char*)CMD_RESOLUTION_1MM, sizeof(CMD_RESOLUTION_1MM));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set range to 10m
    ESP_LOGI(LTAG, "Setting long laser range to 150m...");
    uart_write_bytes(UART_PORT_0, (const char*)CMD_RANGE_150M, sizeof(CMD_RANGE_150M));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Clear read buffer
    uint8_t dist_raw[30];
    uart_read_bytes(UART_PORT_0, dist_raw, sizeof(dist_raw), pdMS_TO_TICKS(1000));
}

void configure_short_laser()
{
    // Open the laser
    ESP_LOGI(LTAG, "Turning on the short laser...");
    uart_write_bytes(UART_PORT_1, (const char*)CMD_OPEN_LASER, sizeof(CMD_OPEN_LASER));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set resolution to 1mm
    ESP_LOGI(LTAG, "Setting short laser resolution to 1mm...");
    uart_write_bytes(UART_PORT_1, (const char*)CMD_RESOLUTION_1MM, sizeof(CMD_RESOLUTION_1MM));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set range to 10m
    ESP_LOGI(LTAG, "Setting short laser range to 10m...");
    uart_write_bytes(UART_PORT_1, (const char*)CMD_RANGE_80M, sizeof(CMD_RANGE_80M));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Clear read buffer
    uint8_t dist_raw[30];
    uart_read_bytes(UART_PORT_1, dist_raw, sizeof(dist_raw), pdMS_TO_TICKS(1000));
}

void string_to_hex(const char *str, uint8_t *hex, size_t hex_len) {
    long value = strtol(str, NULL, 10);

    for (int i = hex_len - 1; i >= 0; i--) {
        hex[i] = value & 0xFF;
        value >>= 8;
    }
}

void get_distance(char *distance_measurement_1, char *distance_measurement_0)
{
    //uint8_t dist_raw[20];
    uint8_t dist_raw_0[20];
    uint8_t dist_raw_1[20];

    //int len;
    int len_0;
    int len_1;

    ESP_LOGI(LTAG, "Sending distance measurement command.\n");
    //uart_write_bytes(UART_PORT, (const char*)CMD_GET_SINGLE, sizeof(CMD_GET_SINGLE));
    uart_write_bytes(UART_PORT_0, (const char*)CMD_GET_SINGLE, sizeof(CMD_GET_SINGLE));
    uart_write_bytes(UART_PORT_1, (const char*)CMD_GET_SINGLE, sizeof(CMD_GET_SINGLE));
    vTaskDelay(pdMS_TO_TICKS(800));  // Increased delay for response

    //len = uart_read_bytes(UART_PORT, dist_raw, sizeof(dist_raw), pdMS_TO_TICKS(1000));
    len_0 = uart_read_bytes(UART_PORT_0, dist_raw_0, sizeof(dist_raw_0), pdMS_TO_TICKS(1000));
    //len = len_0;
    len_1 = uart_read_bytes(UART_PORT_1, dist_raw_1, sizeof(dist_raw_1), pdMS_TO_TICKS(1000));
    
    if (len_0 > 0 && len_1 > 0) {
        /*
        ESP_LOGI(LTAG, "long raw data: %s", dist_raw_0);
        ESP_LOGI(LTAG, "short raw data: %s", dist_raw_1);
        */

        // Extract printable ASCII characters
        int idx_0 = 0, idx_1 = 0;
        for (int i = 0; i < len_0 && idx_0 < 9; i++) { // Limit to 9 characters
            if (dist_raw_0[i] >= 48 && dist_raw_0[i] <= 57 && dist_raw_0[i] != 46) { // ASCII numbers 0-9 and not .
                distance_measurement_0[idx_0++] = dist_raw_0[i];
                //ESP_LOGI(LTAG, "debug long: %i", dist_raw_0[i]);
            }
        }
        distance_measurement_0[idx_0] = '\0';

        for (int i = 0; i < len_1 && idx_1 < 9; i++) { // Limit to 9 characters
            if (dist_raw_1[i] >= 48 && dist_raw_1[i] <= 57 && dist_raw_1[i] != 46) { // Printable ASCII
                distance_measurement_1[idx_1++] = dist_raw_1[i];
            }
        }
        distance_measurement_1[idx_1] = '\0';

        printf("Distance: X:%s-Y:%s \n", distance_measurement_0, distance_measurement_1);

    } else {
        printf("Failed to read distance.\n");
        ESP_LOGE(LTAG, "Reading failed, got %i bytes from long and %i bytes from short", len_0, len_1);
        /* clear lasers chaches */
        uint8_t dist_raw_a[40];
        uint8_t dist_raw_b[40];
        uart_read_bytes(UART_PORT_1, dist_raw_a, sizeof(dist_raw_a), pdMS_TO_TICKS(1100));
        uart_read_bytes(UART_PORT_0, dist_raw_b, sizeof(dist_raw_b), pdMS_TO_TICKS(1100));
    }
}

void update_advertising_data(char *distance_measurement_1, char *distance_measurement_0)
{
    uint8_t manufacturer_data[12];  // 4 bytes for timestamp, 6 bytes for numbers
    uint32_t timestamp = (uint32_t)time(NULL);  // Start time when ESP32 starts
    //ESP_LOGI(LTAG, "timestamp (hex): %lx", timestamp);

    const char *device_name = "LB";

    uint32_t number1 = (uint32_t)atoi(distance_measurement_0);
    uint32_t number2 = (uint32_t)atoi(distance_measurement_1);
    
    /*
    //Debuging outputs
    ESP_LOGI(LTAG, "d_measure0 %s: %lx", distance_measurement_0, number1);
    ESP_LOGI(LTAG, "d_measure1 %s: %lx", distance_measurement_1, number2);

    uint8_t hex_number1[3];
    uint8_t hex_number2[3];
    string_to_hex(distance_measurement_0, hex_number1, sizeof(hex_number1));
    string_to_hex(distance_measurement_1, hex_number2, sizeof(hex_number2));
    ESP_LOGI(LTAG, "dist 1 (hex):");
    ESP_LOG_BUFFER_HEX(LTAG, hex_number1, sizeof(hex_number1));

    ESP_LOGI(LTAG, "dist 2 (hex):");
    ESP_LOG_BUFFER_HEX(LTAG, hex_number2, sizeof(hex_number2));
    */

    // Encode timestamp (4 bytes)
    manufacturer_data[0] = (timestamp >> 24) & 0xFF;
    manufacturer_data[1] = (timestamp >> 16) & 0xFF;
    manufacturer_data[2] = (timestamp >> 8) & 0xFF;
    manufacturer_data[3] = timestamp & 0xFF;

    // Encode numbers as fixed-length uint24 (3 bytes each)
    manufacturer_data[4] = (number1 >> 16) & 0xFF;  // High 8 bits of number1
    manufacturer_data[5] = (number1 >> 8) & 0xFF;   // Middle 8 bits of number1
    manufacturer_data[6] = number1 & 0xFF;          // Low 8 bits of number1

    manufacturer_data[7] = (number2 >> 16) & 0xFF;  // High 8 bits of number2
    manufacturer_data[8] = (number2 >> 8) & 0xFF;   // Middle 8 bits of number2
    manufacturer_data[9] = number2 & 0xFF;          // Low 8 bits of number2

    /*
    // Log the manufacturer data for debugging
    ESP_LOGI(LTAG, "Manufacturer Data (hex):");
    ESP_LOG_BUFFER_HEX(LTAG, manufacturer_data, sizeof(manufacturer_data));
    */

    // Set up advertising data
    uint8_t adv_data[31];
    uint8_t adv_data_len = 0;

    // Build Flags field (Mandatory)
    adv_data[adv_data_len++] = 2; // Length of Flags field (Type + Data)
    adv_data[adv_data_len++] = BLE_HS_ADV_TYPE_FLAGS; // AD Type: Flags (0x01)
    adv_data[adv_data_len++] = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP; // Flags data

    // Include Complete Local Name
    uint8_t device_name_len = strlen(device_name);
    if (device_name_len + 2 <= (31 - adv_data_len)) { // +2 for Length and Type bytes
        adv_data[adv_data_len++] = device_name_len + 1; // Length of this field (Type + Data)
        adv_data[adv_data_len++] = BLE_HS_ADV_TYPE_COMP_NAME; // AD Type: Complete Local Name (0x09)
        memcpy(&adv_data[adv_data_len], device_name, device_name_len);
        adv_data_len += device_name_len;
    }

    // Include Manufacturer Specific Data
    uint8_t remaining_space = 31 - adv_data_len;
    if (sizeof(manufacturer_data) + 2 <= remaining_space) { // +2 for Length and Type bytes
        adv_data[adv_data_len++] = sizeof(manufacturer_data) + 1; // Length of this field (Type + Data)
    adv_data[adv_data_len++] = 0xFFFF; // AD Type: Manufacturer Specific Data (0xFF)
        memcpy(&adv_data[adv_data_len], manufacturer_data, sizeof(manufacturer_data));
        adv_data_len += sizeof(manufacturer_data);
    }

    // Stop current advertising
    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising, error code: %d", rc);
    }

    // Update the advertising data
    rc = ble_gap_adv_set_data(adv_data, adv_data_len);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data, error code: %d", rc);
        return;
    }

    // Restart advertising
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON; // Non-connectable advertising
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // General discoverable mode

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to restart advertising, error code: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising restarted with updated data");
    }
}


void save_readings(void *pvParameters)
{
    int interval_seconds = 1;
    //int num_readings = 1000;

    while (true)
    {
        //printf("\nReading %d of %d \n", i + 1, num_readings);
        get_distance(distance_measurement_0, distance_measurement_1);
        //get_distance(UART_PORT_1, distance_measurement_1);

        // Update advertising data with new measurements
        update_advertising_data(distance_measurement_0, distance_measurement_1);

        vTaskDelay(pdMS_TO_TICKS(interval_seconds * 500));
    }
    
    /*
    for (int i = 0; i < num_readings; i++) {
        printf("\nReading %d of %d \n", i + 1, num_readings);
        get_distance(distance_measurement_0, distance_measurement_1);
        //get_distance(UART_PORT_1, distance_measurement_1);

        // Update advertising data with new measurements
        update_advertising_data(distance_measurement_0, distance_measurement_1);

        vTaskDelay(pdMS_TO_TICKS(interval_seconds * 1000));
    }
    */
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
    configure_long_laser();
    configure_short_laser();
    
    xTaskCreate(save_readings, "save_readings", 4096, NULL, 5, NULL);

    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    return;
}
