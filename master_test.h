#ifndef MASTER_TEST_H
#define MASTER_TEST_H

#include <stdbool.h>
#include <stdint.h>

#define BLINK_DELAY_MS 500

typedef enum {
  MASTER_TEST_RESULT_OK = 0,
  MASTER_TEST_RESULT_SWITCH_FAIL,
  MASTER_TEST_RESULT_RELAY_FAIL,
  MASTER_TEST_RESULT_ADC_FAIL,
  MASTER_TEST_RESULT_PWM_ADC_FAIL,
  MASTER_TEST_RESULT_I2C_FAIL,
} master_test_result_t;

void master_tests_init(void);
bool master_tests_run_all(void);
master_test_result_t master_tests_run_all_with_result(void);
bool run_switch_test(void);

#endif
