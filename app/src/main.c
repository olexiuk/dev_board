/**
 * @file main.c
 */

#include <zephyr/kernel.h>

int start_central(void);
int start_peripheral(void);

int main(void)
{
#if CONFIG_ROLE_CENTRAL
start_central();
#elif CONFIG_ROLE_PERIPHERAL
start_peripheral();
#endif

return 0;
}
