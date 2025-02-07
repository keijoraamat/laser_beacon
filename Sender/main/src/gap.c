/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gap.h"
#include "common.h"
#include <time.h>

/* Private function declarations */
inline static void format_addr(char *addr_str, uint8_t addr[]);
static void start_advertising(void);

/* Private variables */
static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};

/* Private functions */
inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

static void start_advertising(void) {
    ESP_LOGI(TAG, "start_advertising() started");
    const char *device_name = "LB";
    uint32_t number1 = 888887;  // Example value
    uint32_t number2 = 777778; // Example value

    uint8_t manufacturer_data[12];  // 4 bytes for timestamp, 6 bytes for numbers
    uint32_t timestamp = (uint32_t)time(NULL);  // Example: UNIX time

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
        adv_data[adv_data_len++] = 0xFFFF; // AD Type: Manufacturer Specific Data (0x02E5 Espressif Systems (Shanghai) Co., Ltd)
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

/* Public functions */
void adv_init(void) {
    /* Local variables */
    int rc = 0;
    char addr_str[12] = {0};

    /* Make sure we have proper BT identity address set */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    /* Figure out BT address to use while advertising */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    /* Copy device address to addr_val */
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    /* Start advertising. */
    start_advertising();
}

int gap_init(void) {
    /* Local variables */
    int rc = 0;

    /* Initialize GAP service */
    ble_svc_gap_init();

    /* Set GAP device name */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return rc;
    }

    /* Set GAP device appearance */
    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
        return rc;
    }
    return rc;
}
