/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/services/lbs.h>

LOG_MODULE_REGISTER(spi_flash_test, LOG_LEVEL_INF);

#define LED_TEST 0
#define FLASH_WRITE_LEN 16
#define FLASH_TEST_ERASE_SIZE 4096
//#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME "OLD_TST"
//#define DEVICE_NAME "NEW_OTA"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define EXT_FLASH_NODE DT_NODELABEL(mx25r64)
#define EXT_SPI_NODE DT_BUS(EXT_FLASH_NODE)

static const struct flash_area *s_flash_area;
static const struct device *const ext_spi_dev = DEVICE_DT_GET(EXT_SPI_NODE);
static bool app_button_state;
static atomic_t bt_is_connected;
static atomic_t low_power_idle;
static struct k_work adv_work;
static struct k_work enter_idle_work;
static struct k_work exit_idle_work;
static struct bt_conn *current_conn;
K_MUTEX_DEFINE(flash_lock);
K_MUTEX_DEFINE(conn_lock);
K_MUTEX_DEFINE(power_state_lock);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback button1_cb_data;
static struct gpio_callback button2_cb_data;
#if LED_TEST
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);
#endif

static void advertising_start(void);

static int pm_action_ignore_already(const struct device *dev, enum pm_device_action action)
{
	int ret = pm_device_action_run(dev, action);

	if (ret == -EALREADY) {
		return 0;
	}

	return ret;
}

static void disconnect_current_conn(void)
{
	struct bt_conn *conn = NULL;

	k_mutex_lock(&conn_lock, K_FOREVER);
	if (current_conn) {
		conn = bt_conn_ref(current_conn);
	}
	k_mutex_unlock(&conn_lock);

	if (conn) {
		int err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

		if (err && err != -ENOTCONN) {
			printk("BLE disconnect failed (err %d)\n", err);
		}
		bt_conn_unref(conn);
	}
}

static void enter_idle_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);

	k_mutex_lock(&power_state_lock, K_FOREVER);
	if (atomic_get(&low_power_idle)) {
		k_mutex_unlock(&power_state_lock);
		return;
	}

	printk("Entering low power idle\n");
	atomic_set(&low_power_idle, 1);

	(void)k_work_cancel(&adv_work);
	ret = bt_le_adv_stop();
	if (ret && ret != -EALREADY && ret != -EINVAL) {
		printk("Advertising stop failed (err %d)\n", ret);
	}

	disconnect_current_conn();

	k_mutex_lock(&flash_lock, K_FOREVER);
	ret = pm_action_ignore_already(s_flash_area->fa_dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret) {
		printk("External flash suspend failed (err %d)\n", ret);
	}

	if (device_is_ready(ext_spi_dev)) {
		ret = pm_action_ignore_already(ext_spi_dev, PM_DEVICE_ACTION_SUSPEND);
		if (ret) {
			printk("SPI suspend failed (err %d)\n", ret);
		}
	}
	k_mutex_unlock(&flash_lock);

	printk("Low power idle entered\n");
	k_mutex_unlock(&power_state_lock);
}

static void exit_idle_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);

	k_mutex_lock(&power_state_lock, K_FOREVER);
	if (!atomic_get(&low_power_idle)) {
		k_mutex_unlock(&power_state_lock);
		return;
	}

	printk("Leaving low power idle\n");

	k_mutex_lock(&flash_lock, K_FOREVER);
	if (device_is_ready(ext_spi_dev)) {
		ret = pm_action_ignore_already(ext_spi_dev, PM_DEVICE_ACTION_RESUME);
		if (ret) {
			printk("SPI resume failed (err %d)\n", ret);
		}
	}

	ret = pm_action_ignore_already(s_flash_area->fa_dev, PM_DEVICE_ACTION_RESUME);
	if (ret) {
		printk("External flash resume failed (err %d)\n", ret);
	}
	k_mutex_unlock(&flash_lock);

	atomic_clear(&low_power_idle);
	advertising_start();
	printk("Normal operation resumed\n");
	k_mutex_unlock(&power_state_lock);
}

static int init_flash_area(void)
{
	int ret = flash_area_open(FIXED_PARTITION_ID(ido_storage_partition), &s_flash_area);

	if (ret) {
		printk("Error: flash area open failed, err = %d\n", ret);
		return ret;
	}

	if (!device_is_ready(s_flash_area->fa_dev)) {
		printk("External flash device not ready\n");
		return -ENODEV;
	}

	printk("flash test area: offset = 0x%lx, size = %u Byte\n",
	       (long)s_flash_area->fa_off, (unsigned int)s_flash_area->fa_size);

	return 0;
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_LBS_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void adv_work_handler(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);

	if (atomic_get(&low_power_idle)) {
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err == -EALREADY) {
		return;
	}

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void advertising_start(void)
{
	k_work_submit(&adv_work);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
		return;
	}

	printk("Connected\n");
	atomic_set(&bt_is_connected, 1);

	k_mutex_lock(&conn_lock, K_FOREVER);
	if (current_conn) {
		bt_conn_unref(current_conn);
	}
	current_conn = bt_conn_ref(conn);
	k_mutex_unlock(&conn_lock);

	if (atomic_get(&low_power_idle)) {
		(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);

	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
	atomic_clear(&bt_is_connected);

	k_mutex_lock(&conn_lock, K_FOREVER);
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	k_mutex_unlock(&conn_lock);

	if (!atomic_get(&low_power_idle)) {
		advertising_start();
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void app_led_cb(bool led_state)
{
	printk("LBS LED state: %s\n", led_state ? "on" : "off");
}

static bool app_button_cb(void)
{
	return app_button_state;
}

static struct bt_lbs_cb lbs_callbacks = {
	.led_cb = app_led_cb,
	.button_cb = app_button_cb,
};

static void button1_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_work_submit(&enter_idle_work);
}

static void button2_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_work_submit(&exit_idle_work);
}

static int buttons_init(void)
{
	int ret;

	k_work_init(&enter_idle_work, enter_idle_work_handler);
	k_work_init(&exit_idle_work, exit_idle_work_handler);

	if (!gpio_is_ready_dt(&button1) || !gpio_is_ready_dt(&button2)) {
		printk("Button GPIO device not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret) {
		printk("Button1 configure failed (err %d)\n", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
	if (ret) {
		printk("Button2 configure failed (err %d)\n", ret);
		return ret;
	}

	gpio_init_callback(&button1_cb_data, button1_pressed, BIT(button1.pin));
	ret = gpio_add_callback(button1.port, &button1_cb_data);
	if (ret) {
		printk("Button1 callback add failed (err %d)\n", ret);
		return ret;
	}

	gpio_init_callback(&button2_cb_data, button2_pressed, BIT(button2.pin));
	ret = gpio_add_callback(button2.port, &button2_cb_data);
	if (ret) {
		printk("Button2 callback add failed (err %d)\n", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		printk("Button1 interrupt configure failed (err %d)\n", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		printk("Button2 interrupt configure failed (err %d)\n", ret);
		return ret;
	}

	return 0;
}

static void idle_aware_sleep(int32_t ms)
{
	int32_t remaining = ms;

	while ((remaining > 0) && !atomic_get(&low_power_idle)) {
		int32_t delay = (remaining > 100) ? 100 : remaining;

		k_msleep(delay);
		remaining -= delay;
	}
}

static int bluetooth_init(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	err = bt_lbs_init(&lbs_callbacks);
	if (err) {
		printk("LBS init failed (err %d)\n", err);
		return err;
	}

	k_work_init(&adv_work, adv_work_handler);
	advertising_start();

	return 0;
}

int main(void)
{
	printk("%s, %s\n", CONFIG_BOARD_TARGET, CONFIG_BOARD);

	static uint32_t cnt = 0;
	static off_t offset = 0;
	bool flash_test_paused_logged = false;
	int ret = 0;

	ret = init_flash_area();
	if (ret) {
		return 0;
	}

	ret = bluetooth_init();
	if (ret) {
		return 0;
	}

	ret = buttons_init();
	if (ret) {
		return 0;
	}

#if LED_TEST
	if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2) || !gpio_is_ready_dt(&led3)) {
		printk("led device not ready\n");
		return 0;
	}
	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Failed to configure LED0\n");
		return 0;
	}
	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Failed to configure LED1\n");
		return 0;
	}
	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Failed to configure LED2\n");
		return 0;
	}
	ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Failed to configure LED3\n");
		return 0;
	}
#endif

	while(1)
	{
		if (atomic_get(&low_power_idle)) {
			k_msleep(100);
			continue;
		}

		if (atomic_get(&bt_is_connected)) {
			if (!flash_test_paused_logged) {
				printk("Flash test paused while BLE is connected\n");
				flash_test_paused_logged = true;
			}
			idle_aware_sleep(1000);
			continue;
		}
		flash_test_paused_logged = false;

		cnt++;
		printk("==== Loop %u | partition offset = 0x%lx, flash offset = 0x%lx (%ldK) ====\n",
		       cnt, (long)offset, (long)(s_flash_area->fa_off + offset),
		       (long)((s_flash_area->fa_off + offset) / 1024));

		uint8_t buf_r[FLASH_WRITE_LEN] = {0};
		uint8_t buf_w[FLASH_WRITE_LEN] = {0};

		/* 准备写入数据 */
		printk("buf_w: ");
		for (uint8_t i = 0; i < FLASH_WRITE_LEN; i++)
		{
			buf_w[i] = (uint8_t)(rand() >> 24) + i;
			printk("%02X ", buf_w[i]);
		}
		printk("\n");

		/* 1. Erase */
		k_mutex_lock(&flash_lock, K_FOREVER);
		ret = flash_area_erase(s_flash_area, offset, FLASH_TEST_ERASE_SIZE);
		k_mutex_unlock(&flash_lock);
		if (ret != 0)
		{
			printk("[%d]Error: Erase failed, err = %d\n", __LINE__, ret);
		}
		else
		{
			printk("Erase OK\n");
		}
		idle_aware_sleep(1000);
		if (atomic_get(&low_power_idle)) {
			continue;
		}

		/* 2. Write */
		k_mutex_lock(&flash_lock, K_FOREVER);
		ret = flash_area_write(s_flash_area, offset, buf_w, FLASH_WRITE_LEN);
		k_mutex_unlock(&flash_lock);
		if (ret != 0)
		{
			printk("[%d]Error: Write failed, err = %d\n", __LINE__, ret);
		}
		else
		{
			printk("Write OK\n");
		}
		idle_aware_sleep(1000);
		if (atomic_get(&low_power_idle)) {
			continue;
		}

		/* 3. Read & verify */
		k_mutex_lock(&flash_lock, K_FOREVER);
		ret = flash_area_read(s_flash_area, offset, buf_r, FLASH_WRITE_LEN);
		k_mutex_unlock(&flash_lock);
		if (ret != 0)
		{
			printk("[%d]Error: Read failed, err = %d\n", __LINE__, ret);
		}
		else
		{
			printk("buf_r: ");
			for (uint8_t i = 0; i < FLASH_WRITE_LEN; i++)
			{
				printk("%02X ", buf_r[i]);
			}
			printk("\n");
			printk("Match: %s\n", (memcmp(buf_w, buf_r, FLASH_WRITE_LEN) == 0) ? "PASS" : "FAIL");
		}
		idle_aware_sleep(1000);
		if (atomic_get(&low_power_idle)) {
			continue;
		}

		/* 推进到下一个扇区（4K 对齐） */
		offset += FLASH_TEST_ERASE_SIZE;
		if ((offset + FLASH_TEST_ERASE_SIZE) > s_flash_area->fa_size)
		{
			offset = 0;
		}
		printk("=============================================================\n");

		/* 本轮操作结束，休息 5 秒再循环 */
		idle_aware_sleep(5000);
	}

	return 0;
}
