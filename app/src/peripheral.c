
#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

#define SLEEP_MS 50

#define ACC_PWR_NODE DT_ALIAS(acc_pwr)
static const struct gpio_dt_spec acc_power = GPIO_DT_SPEC_GET(ACC_PWR_NODE, gpios);

#define I2C0_NODE DT_NODELABEL(i2c0)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C0_NODE);

#define ACC_I2C_ADR         0x68
#define REG_PWR_MGMT_1      0x6B
#define REG_DATA_START      0x3B

#define GRAVITY 16384

#define ACCEL_SERVICE_UUID BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define ACCEL_CHARACTERISTIC_UUID BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

static const struct bt_uuid_128 accel_service_uuid = BT_UUID_INIT_128(ACCEL_SERVICE_UUID);
static const struct bt_uuid_128 accel_characteristic_uuid = BT_UUID_INIT_128(ACCEL_CHARACTERISTIC_UUID);

// Fixed address for reliable and fast connection
static bt_addr_le_t static_addr = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {.val = {0x01, 0x02, 0x03, 0x04, 0x05, 0xC1}},
};

static const struct bt_data accel_advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, ACCEL_SERVICE_UUID),
};

static const struct bt_data accel_scan_response_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

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

// Holds x and y tilt values to be sent by bluetooth
static uint8_t accel_x_y[2] = {0, 0};

int init_accel(const struct device *i2c_dev)
{
    // Wake accelerometer from sleep
    uint8_t config[] = {REG_PWR_MGMT_1, 0x00};
    return i2c_write(i2c_dev, config, sizeof(config), ACC_I2C_ADR);
}

void update_accel_data(const struct device *i2c_dev)
{
    uint8_t raw_data[6]; 
    uint8_t reg = REG_DATA_START;

    // Read raw accelerometer data
    if (i2c_write_read(i2c_dev, ACC_I2C_ADR, &reg, 1, raw_data, 6) != 0)
    {
        printk("Failed to read accelerometer\n");
        return;
    }
    
    // When holding steady and just tilting, values range [-1g, +1g]. g = 16384
    int16_t x = (raw_data[0] << 8) | raw_data[1];
    int16_t y = (raw_data[2] << 8) | raw_data[3];

    // Maps [-1g, +1g] to [0, 255]
    int16_t x_mapped = ((int32_t)(x + GRAVITY) * 255) / (2 * GRAVITY);
    int16_t y_mapped = ((int32_t)(y + GRAVITY) * 255) / (2 * GRAVITY);

    // Clamp values
    accel_x_y[0] = (uint8_t) CLAMP(x_mapped, 0, 255);
    accel_x_y[1] = (uint8_t) CLAMP(y_mapped, 0, 255);

    printk("X: %3d  Y: %3d\n", x, y);
}

void notify_accel_data(void)
{
    bt_gatt_notify(NULL, &accel_service.attrs[1], accel_x_y, sizeof(accel_x_y));
}

int start_peripheral(void)
{
    int err;

    if (!gpio_is_ready_dt(&acc_power))
    {
        printk("GPIO not ready\n");
        return -1;
    }
    
    err = gpio_pin_configure_dt(&acc_power, GPIO_OUTPUT_ACTIVE);
    if (err != 0)
        return err;

    gpio_pin_set_dt(&acc_power, 1);

    // Give accelerometer time to stablise 
    k_msleep(200);

    if (!device_is_ready(i2c_dev))
    {
        printk("I2C not ready\n");
        return -1;
    }

    if (init_accel(i2c_dev) != 0)
    {
        printk("Failed to wake accelerometer\n");
        return -1;
    }

    // Use static address
    if (bt_id_create(&static_addr, NULL))
        return -1;

    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }
    printk("Bluetooth initialized\n");
    
    if (bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, accel_advertising_data, ARRAY_SIZE(accel_advertising_data), accel_scan_response_data, ARRAY_SIZE(accel_scan_response_data)))
    {
        printk("Failed to start advertising\n");
        return -1;
    }
        
    while (1)
    {
        update_accel_data(i2c_dev);
        notify_accel_data();
        k_msleep(SLEEP_MS);
    }

    return 0;
}
