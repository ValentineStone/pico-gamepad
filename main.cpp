#include <stdio.h>
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "bsp/board.h"

#include "tusb.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for
// information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_MOSI 19

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for
// information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

void led_blinking_task();

int main() {
  stdio_init_all();
  tusb_init();

  // SPI initialisation. This example will use SPI at 1MHz.
  spi_init(SPI_PORT, 1000 * 1000);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

  // Chip select is active-low, so we'll initialise it to a driven-high state
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_put(PIN_CS, 1);

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  puts("Hello, world!");

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  while (true) {
    tuh_task();
    led_blinking_task();
  }

  return 0;
}

#define MAX_REPORT  4

// Each HID instance can has multiple reports
static uint8_t _report_count[CFG_TUH_HID];
static tuh_hid_report_info_t _report_info_arr[CFG_TUH_HID][MAX_REPORT];

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // Interface protocol
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" }; // hid_protocol_type_t
  uint8_t const interface_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  // Parse report descriptor with built-in parser
  _report_count[instance] = tuh_hid_parse_report_descriptor(_report_info_arr[instance], MAX_REPORT, desc_report, desc_len);
  printf("HID has %u reports and interface protocol = %s\r\n", _report_count[instance], protocol_str[interface_protocol]);
}


int64_t last_page = -1;
int64_t last_usage = -1;
uint16_t last_len = 0;
uint8_t* last_report = nullptr;

bool equal(uint16_t len1, const uint8_t* arr1, uint16_t len2, const uint8_t* arr2) {
  if (len1 != len2) return false;
  for (uint16_t i = 0; i < len1; i++)
    if (arr1[i] != arr2[i]) return false;
  return true;
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  (void) dev_addr;

  uint8_t const rpt_count = _report_count[instance];
  tuh_hid_report_info_t* rpt_info_arr = _report_info_arr[instance];
  tuh_hid_report_info_t* rpt_info = NULL;

  if (rpt_count == 1 && rpt_info_arr[0].report_id == 0) {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  } else {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the arrray
    for (uint8_t i = 0; i < rpt_count; i++) {
      if (rpt_id == rpt_info_arr[i].report_id) {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info) {
    printf("Couldn't find the report info for this report !\r\n");
    return;
  }

  if (
    last_page != rpt_info->usage_page
    || last_usage != rpt_info->usage
    || !equal(last_len, last_report, len, report)
    ) {
      // printf("tuh_hid_report_received_cb\n");
      // printf("dev_addr = %u\n", dev_addr);
      // printf("instance = %u\n", instance);
      // printf("rpt_info->usage = %02x\n", rpt_info->usage);

    printf("report[%u] =", len);
    for (uint16_t i = 0; i < len; i++)
      printf(" %02x", report[i]);
    printf("\n");

    last_page = rpt_info->usage_page;
    last_usage = rpt_info->usage;
    last_len = len;
    delete[] last_report;
    last_report = new uint8_t[len];
    memcpy(last_report, report, len);
  }
}


void led_blinking_task() {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  gpio_put(PICO_DEFAULT_LED_PIN, led_state);
  led_state = 1 - led_state; // toggle
}