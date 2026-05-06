#ifndef MASTER_TEST_H
#define MASTER_TEST_H

#include <stdbool.h>
#include <stdint.h>

#define BLINK_DELAY_MS 500

void master_tests_init(void);
bool master_tests_run_all(void);
bool run_switch_test(void);

#endif