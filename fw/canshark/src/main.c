#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/dwt.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/can.h>

#include <libopencm3/ethernet/mac.h>

#include <netif/etharp.h>
#include <lwip/udp.h>
#include <lwip/init.h>

#include "modled.h"
#include "stick.h"
#include "eth_f417.h"
#include "modcan.h"

#include "serial.h"

#include "can_canopen.h"
#include "ipv4/ipv4.h"
#include "ipv4/mac.h"



/* 168MHz */
const clock_scale_t myclock168 = {
	.pllm = 25,  // for 8MHz xtal =8
	.plln = 336,
	.pllp = 2,
	.pllq = 7,
	.hpre = RCC_CFGR_HPRE_DIV_NONE,
	.ppre1 = RCC_CFGR_PPRE_DIV_4,
	.ppre2 = RCC_CFGR_PPRE_DIV_2,
	.flash_config = FLASH_ACR_ICE | FLASH_ACR_DCE | FLASH_ACR_LATENCY_5WS,
	.apb1_frequency = 42000000,
	.apb2_frequency = 84000000,
};

struct netif netif;

void sys_tick_handler(void)
{
	stick_update();

	canopen_sync(CAN1);
}

uint64_t arp_tmr;
uint64_t led_tmr;

#define BENCHMARK_START(a)	a = dwt_read_cycle_counter()
#define BENCHMARK_END(a)	a = dwt_read_cycle_counter() - a;

struct can_message modcan_buffer[8];
const char s_boot_start[] = "\n\n"
  "**************************************************************************\n"
  "    CanShark v 2.0 booting ...\n\n";

const char s_boot_finish[] =
  "Booting finished. Running application\n"
  "**************************************************************************\n";

struct ip_addr ip_bind = { IPADDR_ANY };
struct ip_addr ip_dest = { IPADDR_BROADCAST };

const uint8_t mac[] = {0xE6, 0x00, 0x00, 0x00, 0x00, 0x01};

struct udp_pcb udp;

static void print_config(void)
{
	serial_printf("MAC: Address " MAC_F ", Type %s\n",
		MAC_FV(netif.hwaddr), "Micrel KSZ8051MLL");

	serial_printf("IPv4: Address " IPV4_F "/%d gateway " IPV4_F "\n",
		IPV4_FV(&netif.ip_addr), ipv4_mask_bits(&netif.netmask), IPV4_FV(&netif.gw) );

	serial_printf("UDP: Bound to " IPV4_F ":%d, sending to "IPV4_F ":%d\n",
		IPV4_FV(&ip_bind), 6000, IPV4_FV(&ip_dest), 6000);
}

static void ethernet_linkchg(struct netif *nif)
{
	if (netif_is_link_up(nif))
		serial_print("eth: Link up\n");
	else
		serial_print("eth: Link down\n");
}

static void protocol_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *remaddr, uint16_t remport)
{
	(void)arg;
	(void)pcb;
	(void)remaddr;
	(void)remport;
	pbuf_free(p);
}

static void protocol_poll(void)
{
	int n = 0;
	while (modcan_get(&modcan_buffer[n]) && (n < 8)) {
		n++;
	}

	if (n == 0)
		return;

	struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, n * sizeof(struct can_message), PBUF_RAM);
	if (p == NULL)
		return;		/* packets are lost ! */

	// allocated is always single pbuf in PBUF_RAM, read the buffer into pbuf
	memcpy(p->payload, modcan_buffer, n * sizeof(struct can_message));
	p->len = n * sizeof(struct can_message);
	udp_sendto(&udp, p, &ip_dest, 6000);

	pbuf_free(p);
	serial_print(".");
}

static void print_arp_table(void)
{
	const char *states[] = {"EMPTY", "PENDING", "ACTIVE", "REREQUESTING", "STATIC"};
	struct ip_addr ip;
	struct eth_addr amac;
	int8_t state;
	uint8_t ctime;
	int i=0;

	serial_print("\n");
	while ((state = etharp_get_entry(i, &ip, &amac, &ctime)) >= 0)
	{
		serial_printf(MAC_F " at " IPV4_F " %s age=%d\n",
			MAC_FV(&amac), IPV4_FV(&ip), states[state], ctime);
		i++;
	}
}

int main(void)
{
	rcc_clock_setup_hse_3v3(&myclock168);

	rcc_periph_clock_enable(RCC_GPIOA); /* MDIO */
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC); /* MDC */
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOE);
	rcc_periph_clock_enable(RCC_GPIOH);

	dwt_enable_cycle_counter();

	serial_init();
	serial_print(s_boot_start);

	stick_init(STICK_HZ);
	modled_init();
	modcan_init();

	lwip_init();

	mac_copy((struct eth_addr*)netif.hwaddr, mac);

	netif.ip_addr.addr = IPV4_ADDR(10, 0, 1, 56);
	netif.netmask.addr = IPV4_MASK(16);
	netif.gw.addr = IPV4_ADDR(10, 0, 0, 1);

	netif_add(&netif, &ethf417_init, &ethernet_input);
	netif_set_default(&netif);
	netif_set_up(&netif);
	netif_set_link_callback(&netif, &ethernet_linkchg);

	stick_prepare(&arp_tmr, ARP_TMR_INTERVAL * STICK_HZ / 1000);
	stick_prepare(&led_tmr, STICK_HZ);

	udp_init_pcb(&udp);
	udp.so_options |= SOF_BROADCAST;

	udp_bind(&udp, &ip_bind, 6000);
	udp_recv(&udp, &protocol_recv, &udp);

	print_config();
	serial_print(s_boot_finish);

	while (1) {
		ethf417_poll(&netif);

		protocol_poll();

		if (stick_fire(&led_tmr, STICK_HZ)) {
			LED_TGL(LED0);

			canmsg_get();
		}

		if (stick_fire(&arp_tmr, ARP_TMR_INTERVAL * STICK_HZ / 1000)) {
			print_arp_table();
			etharp_tmr();
		}
	}

	return 0;
}


