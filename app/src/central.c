
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <lvgl.h>

#include "BTN.h"
#include "LED.h"
#include "lv_data_obj.h"

/* MACROS --------------------------------------------------------------------------------------- */

#define SLEEP_MS 10


static struct bt_uuid_128 ACCEL_SERVICE_UUID =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0));
static struct bt_uuid_128 ACCEL_CHARACTERISTIC_UUID =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2));

/* PROTOTYPES ----------------------------------------------------------------------------------- */

static void ble_start_scanning(void);
static uint8_t discover_func(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                             struct bt_gatt_discover_params* params);
static uint8_t notify_func(struct bt_conn* conn, struct bt_gatt_subscribe_params* params,
                           const void* data, uint16_t length);

/* VARIABLES  ----------------------------------------------------------------------------------- */

static bt_addr_le_t static_addr = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {.val = {0x01, 0x02, 0x03, 0x04, 0x05, 0xC1}},
};

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_obj_t *screen = NULL;

static struct bt_conn* my_connection;
static struct bt_uuid_16 discover_uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

const uint16_t width = 320;
const uint16_t height = 240;
const uint8_t diameter = 30;
const float bouncyness = 0.6f;

lv_obj_t *circle;

float x = 50;
float y = 50;
float vel_x = 0;
float vel_y = 0;
float max_accel = 1;

/* FUNCTIONS ------------------------------------------------------------------------------------ */

static bool ble_get_adv_device_name_cb(struct bt_data* data, void* user_data) {
  char* name = user_data;

  if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
    /* Copy the name to the user data buffer */
    memcpy(name, data->data, data->data_len);
    name[data->data_len] = '\\0'; /* Null-terminate the string */
    return false;                 /* Stop parsing after finding the name */
  }

  return true; /* Continue parsing */
}

static void ble_on_advertisement_received(const bt_addr_le_t* addr, int8_t rssi, uint8_t type,
                                          struct net_buf_simple* ad) {
  char addr_str[BT_ADDR_LE_STR_LEN];
  int err;

  if (my_connection) {
    return;
  }

  if (bt_addr_le_cmp(addr, &static_addr) != 0) {
    return;
  }

  /* We're only interested in connectable events */
  if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
    return;
  }

  char name[32] = {0};
  bt_data_parse(ad, ble_get_adv_device_name_cb, name);
  bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
  printk("Device found: %s (RSSI %d) - '%s'\n", addr_str, rssi, name);

  /* connect only to devices in close proximity */
  if (rssi < -80) {
    return;
  }

  if (bt_le_scan_stop()) {
    return;
  }

  err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &my_connection);
  if (err) {
    printk("Create conn to %s failed (%d)\n", addr_str, err);
    ble_start_scanning();
  }
}

static void ble_start_scanning(void) {
  int err;

  /* This demo doesn't require active scan */
  err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, ble_on_advertisement_received);
  if (err) {
    printk("Scanning failed to start (err %d)\n", err);
    return;
  }

  printk("Scanning successfully started\n");
}

static void ble_on_device_connected(struct bt_conn* conn, uint8_t err) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (err) {
    printk("Failed to connect to %s %u %s\n", addr, err, bt_hci_err_to_str(err));

    bt_conn_unref(my_connection);
    my_connection = NULL;

    ble_start_scanning();
    return;
  }

  if (conn != my_connection) {
    return;
  }

  printk("Connected: %s\n", addr);

  discover_params.uuid = &ACCEL_SERVICE_UUID.uuid;
  discover_params.func = discover_func;
  discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
  discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
  discover_params.type = BT_GATT_DISCOVER_PRIMARY;

  err = bt_gatt_discover(my_connection, &discover_params);
  if (err) {
    printk("Discover failed(err %d)\n", err);
    return;
  }
}

static void ble_on_device_disconnected(struct bt_conn* conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];

  if (conn != my_connection) {
    return;
  }

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

  bt_conn_unref(my_connection);
  my_connection = NULL;

  ble_start_scanning();
}

static uint8_t discover_func(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                             struct bt_gatt_discover_params* params) {
  int err;

  if (!attr) {
    printk("Discover complete\n");
    (void)memset(params, 0, sizeof(*params));
    return BT_GATT_ITER_STOP;
  }

  printk("[ATTRIBUTE] handle %u\n", attr->handle);

  if (!bt_uuid_cmp(discover_params.uuid, &ACCEL_SERVICE_UUID.uuid)) {
    discover_params.uuid = &ACCEL_CHARACTERISTIC_UUID.uuid;
    discover_params.start_handle = attr->handle + 1;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
      printk("Discover failed (err %d)\n", err);
    }
  } else if (!bt_uuid_cmp(discover_params.uuid, &ACCEL_CHARACTERISTIC_UUID.uuid)) {
    memcpy(&discover_uuid, BT_UUID_GATT_CCC, sizeof(discover_uuid));
    discover_params.uuid = &discover_uuid.uuid;
    discover_params.start_handle = attr->handle + 2;
    discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
    subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
      printk("Discover failed (err %d)\n", err);
    }
  } else {
    subscribe_params.notify = notify_func;
    subscribe_params.value = BT_GATT_CCC_NOTIFY;
    subscribe_params.ccc_handle = attr->handle;

    err = bt_gatt_subscribe(conn, &subscribe_params);
    if (err && err != -EALREADY) {
      printk("Subscribe failed (err %d)\n", err);
    } else {
      printk("[SUBSCRIBED]\n");
    }

    return BT_GATT_ITER_STOP;
  }

  return BT_GATT_ITER_STOP;
}

static uint8_t notify_func(struct bt_conn* conn, struct bt_gatt_subscribe_params* params,
                           const void* data, uint16_t length) {
  if (!data) {
    printk("[UNSUBSCRIBED]\n");
    params->value_handle = 0U;
    return BT_GATT_ITER_STOP;
  }

  if (length >= 2) {
        uint8_t* vals = (uint8_t*) data;
        vel_x += max_accel * ((vals[1] / 128.0f) - 1);
        vel_y += max_accel * ((vals[0] / 128.0f) - 1);

        printk("[NOTIFICATION] x=%d, y=%d\n", vals[0], vals[1]);
    } else {
        printk("[NOTIFICATION] data too short, length=%u\n", length);
    }

  return BT_GATT_ITER_CONTINUE;
}

/* CALLBACK CONFIG ------------------------------------------------------------------------------ */

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = ble_on_device_connected,
    .disconnected = ble_on_device_disconnected,
};

/* MAIN ----------------------------------------------------------------------------------------- */

int start_central(void)
{
  if (0 > BTN_init())
    return 0;
  if (0 > LED_init())
    return 0;

  if (!device_is_ready(display_dev))
    return 0;

  screen = lv_screen_active();
  if (screen == NULL)
    return 0;

  int err = bt_enable(NULL);
  if (err)
  {
    printk("Bluetooth init failed (err %d)\n", err);
    return 0;
  } 
  else
    printk("Bluetooth initialized\n");

  ble_start_scanning();

  circle = lv_obj_create(screen);
  lv_obj_set_size(circle, diameter, diameter);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle, lv_color_hex(0xFF0000), LV_PART_MAIN);
  lv_obj_set_pos(circle, x, y);

  display_blanking_off(display_dev);
  while (1) {
    lv_task_handler();
    k_msleep(SLEEP_MS);
    x += vel_x;
    y += vel_y;
    if (x + diameter > width)
    {
      x = width - diameter - (x - (width - diameter));
      vel_x *= -bouncyness;
    }
    else if (x < 0)
    {
      x *= -1;
      vel_x *= -bouncyness;
    }
    if (y + diameter > height)
    {
      y = height - diameter - (y - (height - diameter));
      vel_y *= -bouncyness;
    }
    else if (y < 0)
    {
      y *= -1;
      vel_y *= -bouncyness;
    }

    // printf("x: %.2f, y: %.2f, width: %d, height: %d\n", x, y, width, height);
    lv_obj_set_pos(circle, round(x), round(y));
  }

  // To disconnect: bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

  return 0;
}