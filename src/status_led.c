#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// デバイスツリーの status_led ノードを取得
#define LED_NODE DT_NODELABEL(status_led)
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

// LEDの状態管理変数
static bool led_state = false;
static int blink_count = 0;

// タイマー定義
static void led_timer_handler(struct k_timer *timer);
static void led_work_handler(struct k_work *work);
K_TIMER_DEFINE(led_timer, led_timer_handler, NULL);
K_WORK_DEFINE(led_work, led_work_handler);

// LEDのON/OFF切り替え用関数
static void toggle_led(bool state) {
  if (device_is_ready(status_led.port)) {
    gpio_pin_set_dt(&status_led, state ? 1 : 0);
  }
}



// 点滅タイマーの処理
static void led_work_handler(struct k_work *work) {
  led_state = !led_state;
  toggle_led(led_state);

  if (led_state) {
    k_timer_start(&led_timer, K_MSEC(200), K_NO_WAIT);
    blink_count++;
  } else {
    k_timer_start(&led_timer, K_MSEC(1800), K_NO_WAIT);
  }

  // ONが10回したら強制消灯して終了
  if (blink_count >= 10) {
    k_timer_stop(&led_timer);
    toggle_led(false);
  }
}

static void led_timer_handler(struct k_timer *timer) {
  k_work_submit(&led_work);
}

// Bluetooth接続成功時のコールバック
static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    return;
  }

  // 点滅中なら止める
  k_timer_stop(&led_timer);
  toggle_led(false);
}

// Bluetooth切断時のコールバック
static void disconnected(struct bt_conn *conn, uint8_t reason) {
  // 新たに10秒間の点滅開始 (0.2秒ON / 1.8秒OFF の2秒間隔)
  blink_count = 0;
  led_state = false;
  k_work_submit(&led_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// システム起動時の初期化処理
static int status_led_init() {
  if (!device_is_ready(status_led.port)) {
    return -ENODEV;
  }

  gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);

  // 起動直後は接続待ち状態として点滅開始
  blink_count = 0;
  led_state = false;
  k_work_submit(&led_work);

  return 0;
}

// APPLICATION レベルで初期化を実行する
SYS_INIT(status_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
