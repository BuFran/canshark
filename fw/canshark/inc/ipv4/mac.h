#ifndef MAC_H_INCLUDED
#define MAC_H_INCLUDED



#define MAC_F		"%02x%02x%02x-%02x%02x%02x"
#define MAC_FV(val)	((uint8_t *)(val))[0],		\
			((uint8_t *)(val))[1],		\
			((uint8_t *)(val))[2],		\
			((uint8_t *)(val))[3],		\
			((uint8_t *)(val))[4],		\
			((uint8_t *)(val))[5]

/* returns 0 if exactly parsed,
   returns < 0 if ip is unparsable
   returns > if ip is parsable
   value defines position of first unparsable character
   requires short format AABBCC-DDEEFF
   or linux format AA-BB-CC-DD-EE-FF
   or windows format AA:BB:CC:DD:EE:FF
   hex number inside can be ommited
   */
static int mac_aton(struct eth_addr *addr, const char *address)
{
	uint8_t eth[6];
	memset(eth, 0, 6);
	int pos = 0;
	int part;
	for (part = 0; part < 6; part++, address++, pos++) {
		// parse Hi
		if (*address == 0) {
			break;
		} else if ((*address == '-') || (*address == ':')) {
			continue;
		} else if ((*address >= '0') && (*address <= '9')) {
			eth[part] = (*address - '0');
		} else if ((*address >= 'A') && (*address <= 'F')) {
			eth[part] = (*address - 'A' + 10);
		} else if ((*address >= 'a') && (*address <= 'f')) {
			eth[part] = (*address - 'A' + 10);
		} else {
			// unknown unparsable character here
			break;
		}

		address++;
		pos++;

		// parse Lo
		if (*address == 0) {
			break;
		} else if ((*address == '-') || (*address == ':')) {
			continue;
		} else if ((*address >= '0') && (*address <= '9')) {
			eth[part] = (eth[part] << 4) | (*address - '0');
		} else if ((*address >= 'A') && (*address <= 'F')) {
			eth[part] = (eth[part] << 4) | (*address - 'A' + 10);
		} else if ((*address >= 'a') && (*address <= 'f')) {
			eth[part] = (eth[part] << 4) | (*address - 'A' + 10);
		} else {
			// unknown unparsable character here
			break;
		}
	}

	if (part < 5)
		return -pos;	// error in parsing

	memcpy(addr, eth, 6);
	return pos;		// parsed successfully
}

/* buffer size always of length 6*4+2 characters,
   returns pointer to this buffer for use inside print
   always in format AABBCC-DDEEFF
   */
static char *mac_ntoa(char *address, struct eth_addr *addr)
{
	if (addr == NULL) {
		strcpy(address,"(null)");
		return address;
	}

	char* ret = address;
	uint8_t *tmp = (uint8_t*)addr->addr;

	for (int i = 0;i < 3; i++) {
		uint8_t lval = tmp[i] & 0x0F;
		uint8_t hval = (tmp[i] >> 4) & 0x0F;
		*address++ = hval + (hval > 9) ? 'A' - 10 : '0';
		*address++ = lval + (lval > 9) ? 'A' - 10 : '0';
	}
	*address++ = '-';
	for (int i = 3;i < 6; i++) {
		uint8_t lval = tmp[i] & 0x0F;
		uint8_t hval = (tmp[i] >> 4) & 0x0F;
		*address++ = hval + (hval > 9) ? 'A' - 10 : '0';
		*address++ = lval + (lval > 9) ? 'A' - 10 : '0';
	}
	return ret;
}

static inline void mac_copy(struct eth_addr *dest, const uint8_t *src)
{
	dest->addr[0] = src[0];
	dest->addr[1] = src[1];
	dest->addr[2] = src[2];
	dest->addr[3] = src[3];
	dest->addr[4] = src[4];
	dest->addr[5] = src[5];
	dest->addr[6] = src[6];
}

#endif // MAC_H_INCLUDED
