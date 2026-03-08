
#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include "BTN.h"
#include "LED.h"

#define SLEEP_MS 1000

#define ACC_PWR_NODE DT_ALIAS(acc_pwr)
static const struct gpio_dt_spec acc_power = GPIO_DT_SPEC_GET(ACC_PWR_NODE, gpios);

#define I2C0_NODE DT_NODELABEL(i2c0)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C0_NODE);

#define ACC_I2C_ADR           0x68
#define REG_PWR_MGMT_1        0x6B
#define REG_DATA_START        0x3B

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

        printk("X: %6d  Y: %6d  Z: %6d\n", x, y, z);
    }
    else
        printk("Failed to read\n");
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

  while (1)
  {
    read_accel(i2c_dev);
    k_msleep(SLEEP_MS);
  }

  return 0;
}
