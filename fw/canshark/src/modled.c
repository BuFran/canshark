#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "modled.h"
#include "io.h"

void modled_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOD);

	io_output_high(PD2);
	io_output_high(PD4);
	io_output_high(PD10);
}
