#ifndef _ETH_F417_H__
#define _ETH_F417_H__


void ethf417_gpio_init(void);
int8_t ethf417_output(struct netif *nif, struct pbuf *p);
void ethf417_poll(struct netif *nif);
int8_t ethf417_init(struct netif *nif);

#endif // _ETH_F417_H__
