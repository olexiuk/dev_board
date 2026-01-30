/*
 * main.c
 */

#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include "BTN.h"
#include "LED.h"

#define SLEEP_MS 100

#define ARDUINO_SPI_NODE DT_NODELABEL(arduino_spi)
#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#define CMD_SOFTWARE_RESET 0x01
#define CMD_SLEEP_OUT 0x11
#define CMD_DISPLAY_ON 0x29
#define CMD_COLUMN_ADDRESS_SET 0x2A
#define CMD_ROW_ADDRESS_SET 0x2B
#define CMD_MEMORY_WRITE 0x2C

#define ARDUINO_I2C_NODE DT_NODELABEL(arduino_i2c)
static const struct device* dev1 = DEVICE_DT_GET(ARDUINO_I2C_NODE);

#define ADDR 0x38
#define TD_STATUS 0x02
#define P1_XH 0x03
#define P1_XL 0x04
#define P1_YH 0x05
#define P1_YL 0x06

#define TOUCH_EVEHT_MASK 0xC0
#define TOUCH_EVENT_SHIFT 6

#define TOUCH_POS_MSB_MASK 0x0F

typedef enum {
  TOUCH_EVENT_PRESS_DOWN = 0b00u,
  TOUCH_EVENT_LIFT_UP = 0b01u,
  TOUCH_EVENT_CONTACT = 0b10u,
  TOUCH_EVENT_NO_EVENT = 0b11u
} touch_event_t;

static const struct gpio_dt_spec dcx_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, dcx_gpios);
static const struct spi_cs_control cs_ctrl = (struct spi_cs_control)
{
  .gpio = GPIO_DT_SPEC_GET(ARDUINO_SPI_NODE, cs_gpios),
  .delay = 1u,
};

static const struct device* dev0 = DEVICE_DT_GET(ARDUINO_SPI_NODE);
static const struct spi_config spi_cfg = {
  .frequency = 1000000,
  .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
  .slave = 0,
  .cs = cs_ctrl
};

static void lcd_cmd(uint8_t cmd, struct spi_buf * data)
{
  struct spi_buf cmd_buf[1] = {{&cmd, 1}};
  struct spi_buf_set cmd_set = {cmd_buf, 1};
  gpio_pin_set_dt(&dcx_gpio, 0);
  spi_write(dev0, &spi_cfg, &cmd_set);
  if (data != NULL)
  {
    struct spi_buf_set data_set = {data, 1};
    gpio_pin_set_dt(&dcx_gpio, 1);
    spi_write(dev0, &spi_cfg, &data_set);
  }
}

void touch_control_cmd_rsp(uint8_t cmd, uint8_t* rsp)
{
  struct i2c_msg cmd_rsp_msg[2] = {
    {&cmd, 1, I2C_MSG_WRITE},
    {rsp, 1, I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP}
  };
  i2c_transfer(dev1, cmd_rsp_msg, 2, ADDR);
}

int main(void)
{
  if (!gpio_is_ready_dt(&dcx_gpio))
    return 0;
  if (gpio_pin_configure_dt(&dcx_gpio, GPIO_OUTPUT_LOW))
    return 0;
  if (!device_is_ready(dev1))
    return 0;
  if (0 > i2c_configure(dev1, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER))
    return 0;
  if (0 > BTN_init())
    return 0;
  if (0 > LED_init())
    return 0;

  lcd_cmd(CMD_SOFTWARE_RESET, NULL);

  k_msleep(120);

  lcd_cmd(CMD_SLEEP_OUT, NULL);
  lcd_cmd(CMD_DISPLAY_ON, NULL);

  uint8_t column_data[] = {0x00, 0x73, 0x00, 0x7c};
  uint8_t row_data[] = {0x00, 0x9b, 0x00, 0xa4};
  
  uint8_t colour_data[300];
  for (int i = 0; i < 300; i += 3)
  {
    colour_data[i] = 0;
    colour_data[i + 1] = 0;
    colour_data[i + 2] = 0xfc;
  }

  struct spi_buf column_data_buf = {column_data, 4};
  struct spi_buf row_data_buf = {row_data, 4};
  struct spi_buf colour_data_buf = {colour_data, 300};

  lcd_cmd(CMD_COLUMN_ADDRESS_SET, &column_data_buf);
  lcd_cmd(CMD_ROW_ADDRESS_SET, &row_data_buf);
  lcd_cmd(CMD_MEMORY_WRITE, &colour_data_buf);
  printk("HERE");
  while(1)
  {
    uint8_t touch_status;
    touch_control_cmd_rsp(TD_STATUS, &touch_status);
    if (touch_status == 1)
    {
      uint8_t x_pos_h;
      uint8_t x_pos_l;
      uint8_t y_pos_h;
      uint8_t y_pos_l;

      touch_control_cmd_rsp(P1_XH, &x_pos_h);
      touch_control_cmd_rsp(P1_XL, &x_pos_l);
      touch_control_cmd_rsp(P1_YH, &y_pos_h);
      touch_control_cmd_rsp(P1_YL, &y_pos_l);

      uint16_t x_pos = ((x_pos_h & TOUCH_POS_MSB_MASK) << 8) + x_pos_l;
      uint16_t y_pos = ((y_pos_h & TOUCH_POS_MSB_MASK) << 8) + y_pos_l;

      printk("Touch at %u, %u\n", x_pos, y_pos);
    }
    k_msleep(SLEEP_MS);
  }
  return 0;
}
