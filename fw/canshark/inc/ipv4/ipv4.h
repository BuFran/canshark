#ifndef IPV4_H_INCLUDED
#define IPV4_H_INCLUDED

#define IPV4_ADDR(a, b, c, d) \
	(((uint32_t)((d) & 0xff) << 24) | \
	 ((uint32_t)((c) & 0xff) << 16) | \
	 ((uint32_t)((b) & 0xff) << 8)  | \
	  (uint32_t)((a) & 0xff))

#define IPV4_MASK(a)	\
	(uint32_t)((1 << (a)) - 1)


#define IPV4_F		"%u.%u.%u.%u"
#define IPV4_FV(val)	(((uint8_t *)(val))[0]), (((uint8_t *)(val))[1]), \
			(((uint8_t *)(val))[2]), (((uint8_t *)(val))[3])

static inline void ipv4_copy(struct ip_addr *dest, const struct ip_addr *src)
{
	if ((dest != NULL) && (src != NULL))
		dest->addr = src->addr;
}

static uint8_t ipv4_mask_bits(struct ip_addr *mask)
{
	uint8_t nbits = 0;
	uint32_t mskval = mask->addr;

	for (int i = 0; i < 32; i++, mskval >>= 1)
	{
		if (mskval & 0x01)
			nbits++;
		else
			break;
	}
	return nbits;
}

static inline bool ipv4_mask_valid(struct ip_addr *mask)
{
	if (mask == NULL)
		return false;
	return (mask->addr & (mask->addr - 1)) == 0;
}

static inline uint32_t ipv4_id_network(struct ip_addr *ip, struct ip_addr *mask)
{
	return ip->addr & ~mask->addr;
}

static inline uint32_t ipv4_id_host(struct ip_addr *ip, struct ip_addr *mask)
{
	return ip->addr & mask->addr;
}

/* returns 0 if exactly parsed,
   returns < 0 if ip is unparsable
   returns > if ip is parsable
   value defines position of first unparsable character
   */
static int ipv4_aton(struct ip_addr *addr, const char *address)
{
	int parts[4];
	int part = 0;
	int inpart = 0;
	int pos;
	for (pos = 0;*address;address++, pos++) {
		if ((*address >= '0') && (*address <= '9')) {
			parts[part] = parts[part] * 10 + (*address - '0');
			inpart++;
		} else if (*address == '.') {
			part++;
			inpart = 0;
			if (part == 5) {
				break;
			}
		} else {
			// unparsable character
			if ((part == 4) && (inpart > 0)) {
				break;
			} else {
				// string was not sufficient to decode 4 numbers
				// eg "0.0.0." or "3.14159x"
				return -pos;
			}
		}
	}

	addr->addr = IPV4_ADDR(parts[0], parts[1], parts[2], parts[3]);
	return (*address) ? pos : 0;
}

/* buffer size always of length 4*4+1 characters,
   returns pointer to this buffer for use inside print */
static char *ipv4_ntoa(char *address, struct ip_addr* addr)
{
	char* ret = address;
	if (addr == NULL) {
		strcpy(address,"(null)");
		return address;
	}
	uint8_t *ptr = (uint8_t *)&addr->addr;
	for (int i = 3;i >= 0; i--) {
		uint8_t val = ptr[i];
		if (val > 99) {
			*address++ = (val / 100) + '0';
		}
		val %= 100;
		if (val > 9) {
			*address++ = (val / 10) + '0';
		}
		val %= 10;
		*address++ = val + '0';
		if (i > 0) {
			*address++ = '.';
		}
	}
	return ret;
}

#endif // IPV4_H_INCLUDED
