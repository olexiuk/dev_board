
#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

#include "BTN.h"
#include "LED.h"

#define SLEEP_MS 100

#define ACC_PWR_NODE DT_ALIAS(acc_pwr)
static const struct gpio_dt_spec acc_power = GPIO_DT_SPEC_GET(ACC_PWR_NODE, gpios);

#define I2C0_NODE DT_NODELABEL(i2c0)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C0_NODE);

#define ACC_I2C_ADR           0x68
#define REG_PWR_MGMT_1        0x6B
#define REG_DATA_START        0x3B

#define BLE_CUSTOM_SERVICE_UUID BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BLE_CUSTOM_CHARACTERISTIC_UUID BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

static const struct bt_uuid_128 accel_service_uuid = BT_UUID_INIT_128(BLE_CUSTOM_SERVICE_UUID);
static const struct bt_uuid_128 accel_characteristic_uuid = BT_UUID_INIT_128(BLE_CUSTOM_CHARACTERISTIC_UUID);

static const struct bt_data accel_advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BLE_CUSTOM_SERVICE_UUID),
};

static const struct bt_data accel_scan_response_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static uint8_t data[2] = {0, 0};

BT_GATT_SERVICE_DEFINE(
    accel_service,
    BT_GATT_PRIMARY_SERVICE(&accel_service_uuid),

    BT_GATT_CHARACTERISTIC(
        &accel_characteristic_uuid.uuid,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL
    ),
    BT_GATT_CCC(
        NULL,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    ),
);

void init_accel(const struct device *i2c_dev)
{
    uint8_t config[] = {REG_PWR_MGMT_1, 0x00};
    int ret = i2c_write(i2c_dev, config, sizeof(config), 0x68);
    if (ret != 0)
        printk("Failed to turn on accelerometer: %d\n", ret);
    else
        printk("Accelerometer on\n");
}

void read_accel(const struct device *i2c_dev)
{
    uint8_t raw_data[6]; 
    uint8_t reg = REG_DATA_START;

    if (i2c_write_read(i2c_dev, ACC_I2C_ADR, &reg, 1, raw_data, 6) == 0)
    {
        int16_t x = (raw_data[0] << 8) | raw_data[1];
        int16_t y = (raw_data[2] << 8) | raw_data[3];
        int16_t z = (raw_data[4] << 8) | raw_data[5];

        int16_t x_mapped = ((int32_t)(x + 16384) * 255) / (2 * 16384);
        int16_t y_mapped = ((int32_t)(y + 16384) * 255) / (2 * 16384);

        if (x_mapped < 0)
            x_mapped = 0;
        else if (x_mapped > 255)
            x_mapped = 255;
        if (y_mapped < 0)
            y_mapped = 0;
        else if (y_mapped > 255)
            y_mapped = 255;

        data[0] = (uint8_t) x_mapped;
        data[1] = (uint8_t) y_mapped;

        printk("X: %6d  Y: %6d  Z: %6d\n", x, y, z);
    }
    else
        printk("Failed to read\n");
}

void notify_accel_data(void)
{
    bt_gatt_notify(NULL, &accel_service.attrs[1], data, sizeof(data));
}

int start_peripheral(void)
{
    if (!gpio_is_ready_dt(&acc_power))
        return 0;
    if (gpio_pin_configure_dt(&acc_power, GPIO_OUTPUT_ACTIVE) < 0)
        return 0;

    gpio_pin_set_dt(&acc_power, 1);

    k_msleep(200);

    if (!device_is_ready(i2c_dev))
        return 0;

    init_accel(i2c_dev);

    if (bt_enable(NULL))
        return 0;

    if (bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, accel_advertising_data, ARRAY_SIZE(accel_advertising_data), accel_scan_response_data, ARRAY_SIZE(accel_scan_response_data)))
        return 0;

    while (1)
    {
        read_accel(i2c_dev);
        notify_accel_data();
        k_msleep(SLEEP_MS);
    }

    return 0;
}
