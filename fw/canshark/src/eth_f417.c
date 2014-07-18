#include <errno.h>
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/ethernet/mac.h>
#include <libopencm3/ethernet/phy_ksz8051mll.h>

#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"

#include "netif/etharp.h"

#include "eth_f417.h"
#include "io.h"
#include "board.h"
#include "serial.h"

#include "ipv4/mac.h"

#define ETH_RX_BUF_SIZE    1536 /* buffer size for receive */
#define ETH_TX_BUF_SIZE    1536 /* buffer size for transmit */
#define ETH_RXBUFNB        4   /* 20 Rx buffers of size ETH_RX_BUF_SIZE */
#define ETH_TXBUFNB        2    /* 5  Tx buffers of size ETH_TX_BUF_SIZE */

uint8_t eth_buffer[
	ETH_RXBUFNB * ETH_RX_BUF_SIZE +
	ETH_TXBUFNB * ETH_TX_BUF_SIZE +
	(ETH_RXBUFNB + ETH_TXBUFNB) * ETH_DES_EXT_SIZE
	];


void ethf417_gpio_init(void)
{
	rcc_periph_clock_enable(RCC_ETHMAC); /* MDIO */
	rcc_periph_clock_enable(RCC_ETHMACRX);
	rcc_periph_clock_enable(RCC_ETHMACTX);

	const uint32_t mii_pins[] = {
		MII_PIN_RXC, MII_PIN_RXER, MII_PIN_TXC, MII_PIN_TXEN,
		MII_PIN_RXD0, MII_PIN_RXD1, MII_PIN_RXD2, MII_PIN_RXD3,
		MII_PIN_TXD0, MII_PIN_TXD1, MII_PIN_TXD2, MII_PIN_TXD3,
		MII_PIN_MDC, MII_PIN_MDIO, MII_PIN_CRS, MII_PIN_COL,
		MII_PIN_RXDV
	};

	for (uint32_t i = 0; i < sizeof(mii_pins)/sizeof(uint32_t); i++)
		io_af(mii_pins[i], MII_AF);

	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO2);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO12 | GPIO13 | GPIO11);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO1 | GPIO2);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO2);
	gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO2);

	io_input_pullup(MII_PIN_INTRP);
	io_output_high(MII_PIN_RST);

	// wait at least 10ms
	for (int i = 0; i < 1680000; i++)
		asm volatile ("nop");

}

static uint8_t pkt[1600];
static uint8_t pkr[1600];

int8_t ethf417_output(struct netif *nif, struct pbuf *p)
{
	(void)nif;

	pbuf_copy_partial(p, pkt, p->tot_len, 0);

	if (!eth_tx(pkt, p->tot_len))
		serial_print("TX\n");

	return -ELOK;
}

#define PHY_REG_ICSR_SLINKUP	(1 << 0)
#define PHY_REG_ICSR_SREMFLT	(1 << 1)
#define PHY_REG_ICSR_SLINKDN	(1 << 2)
#define PHY_REG_ICSR_SLPACK	(1 << 3)
#define PHY_REG_ICSR_SPDFLT	(1 << 4)
#define PHY_REG_ICSR_SPAGEREC	(1 << 5)
#define PHY_REG_ICSR_SRECERR	(1 << 6)
#define PHY_REG_ICSR_SJABBER	(1 << 7)
#define PHY_REG_ICSR_FLINKUP	(1 << 8)
#define PHY_REG_ICSR_FREMFLT	(1 << 9)
#define PHY_REG_ICSR_FLINKDN	(1 << 10)
#define PHY_REG_ICSR_FLPACK	(1 << 11)
#define PHY_REG_ICSR_FPDFLT	(1 << 12)
#define PHY_REG_ICSR_FPAGEREC	(1 << 13)
#define PHY_REG_ICSR_FRECERR	(1 << 14)
#define PHY_REG_ICSR_FJABBER	(1 << 15)


void ethf417_poll(struct netif *nif)
{
	// INTRP is low, MAC wants interrupt us ?
	if (!io_is_set(MII_PIN_INTRP)) {

		uint16_t irq = eth_smi_read(1, PHY_REG_ICSR);

		if (irq & PHY_REG_ICSR_SLINKUP) {
			netif_set_link_up(nif);
		}
		if (irq & PHY_REG_ICSR_SLINKDN) {
			netif_set_link_down(nif);
		}
	}

	// frame received and waiting for read
	if (eth_irq_is_pending(ETH_DMASR_RS)) {
		uint32_t len = 0;

		while (eth_rx(pkr, &len, 1600)) {
			struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
			if (p != NULL) {
				pbuf_take(p, pkr, len);

				if (nif->input(p, nif) < 0) {
					pbuf_free(p);
				}
			}
		}
	}
}

int8_t ethf417_init(struct netif *nif)
{
	nif->output = etharp_output;
	nif->linkoutput = ethf417_output;

	nif->mtu = 1500;
	nif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

	ethf417_gpio_init();
	rcc_periph_reset_pulse(RST_ETHMAC);
	eth_init(ETH_CLK_150_168MHZ);

	eth_set_mac(nif->hwaddr);
	eth_desc_init(eth_buffer, ETH_TXBUFNB, ETH_RXBUFNB, ETH_TX_BUF_SIZE, ETH_RX_BUF_SIZE, true);
	eth_enable_checksum_offload();

	// generate link up/down interrupts on INTRP pin
	eth_smi_write(1, PHY_REG_ICSR, PHY_REG_ICSR_FLINKUP | PHY_REG_ICSR_FLINKDN);


	/*eth_irq_enable(ETH_DMAIER_NISE | ETH_DMAIER_AISE | ETH_DMAIER_RIE);
	nvic_enable_irq(NVIC_ETH_IRQ);
*/
	eth_start();

	return -ELOK;
}
