#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

// tady máme separovaný vìci k sériáku, to se pak dá snadno pøidat do jiných projektù

/*
#define F_CPUkor	(F_CPU + ((F_CPU * (int8_t)SIGROW.OSC20ERR5V) / 1024L))
#define UART_baud(br) ((float)(F_CPUkor*64 / (16*(float)br)) + 0.5)
#define UART_bd		19200
// RxD - PORTA.2, TxD - PORTA.1
PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
PORTA.DIRSET = 1<<1;
PORTA.OUTSET = 1<<1;
PORTA_PIN2CTRL = PORT_PULLUPEN_bm;
USART0.CTRLA = 0;
USART0.CTRLB = USART_RXEN_bm + USART_TXEN_bm;
USART0.CTRLC = USART_CHSIZE_8BIT_gc;
USART0.BAUD = UART_baud(UART_bd);
USART0.STATUS = USART_DREIF_bm;
*/

extern void cekej(uint16_t n);

uint8_t	uartRecvMsCnt = 0;		// timeout v uartRecv
bool	uartSendSemafor = false;// semafor aby se nezacyklilo pøi volání z cekej

void uartSend(char b) { // ====================================================
	uartSendSemafor = true;
	while ((USART0.STATUS & USART_DREIF_bm) == 0) cekej(0);
	USART0.TXDATAL=b;
	uartSendSemafor = false;
}
void uartSend(char* text) { // ================================================
	for (uint8_t n=0; n<255; n++) {
		if (text[n]==0) break;
		uartSend(text[n]);
	}
}
void uartSend(uint32_t x) { // ================================================
	uint32_t xx = x;
	char text[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint8_t n;
	
	for (n=0; n<10; n++) {
		text[n] = xx % 10 + '0';
		xx /= 10;
		if (xx==0) break;
	}
	for (; n<10; n--) {
		uartSend(text[n]);
	}
}
void uartSend(uint16_t x) { // ================================================
	uartSend(static_cast<uint32_t>(x));
}
void uartSend(uint8_t x) { // =================================================
	uartSend(static_cast<uint32_t>(x));
}
void uartSend(int32_t x) { // =================================================
	if (x<0) {
		uartSend('-');
		uartSend(static_cast<uint32_t>(-x));
	} else {
		uartSend(static_cast<uint32_t>(x));
	}
}
void uartSend(int16_t x) { // ================================================
	uartSend(static_cast<int32_t>(x));
}
void uartSend(int8_t x) { // =================================================
	uartSend(static_cast<int32_t>(x));
}
void uartSend(bool x) { // ===================================================
	if (x) uartSend('1'); else uartSend('0');
}

const char		cNoRecv = 255;
const uint8_t	cEndCharLimit = 10;
// i jen inicializovano / r recv chr / t timeout / e endchar / b buf. overflow / u unknown
char			recvEndType = 'i';
char uartRecv(uint8_t timeout=0) { // =========================================
	uartRecvMsCnt = 0;
	while ((USART0.STATUS & USART_RXCIF_bm) == 0) {
		cekej(0);
		if (uartRecvMsCnt >= timeout && timeout != 0) {
			recvEndType = 't';
			return cNoRecv;
		}
	}
	recvEndType = 'r';
	return USART0.RXDATAL;
}
uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" -", bool wait0=true, uint8_t timeout=255) {
	uint8_t n=0;
	
	if (wait0) { // neomezenì èekat na první znak
		data[n] = uartRecv(0);
		n++;
	}
	for (; n<limit; n++) {
		data[n] = uartRecv(timeout);
		for (uint8_t e=0; e<cEndCharLimit; e++) {
			if (endChars[e]==0) break;
			if (data[n]==endChars[e]) {
				recvEndType = 'e';
				return n;
			}
		}
		if (recvEndType!='r') return n;
	}
	if (n==limit) {recvEndType = 'b'; return n;}
	recvEndType = 'u';
	return n;
}

uint8_t strToVal(char* data) { // =============================================
	uint8_t val = 0;
	for (uint8_t n=0; n<2; n++) {
		if (data[n]==' ') break;
		if (data[n]<'0' || data[n]>'9') {
			uartSend("ERR: Neocekavany znak (pos:"); uartSend(n);
			uartSend(" '"); uartSend(data[n]);
			uartSend("'="); uartSend((uint8_t)data[n]);
			uartSend(") ocekavano 0-9\n");
			return 255;
		}
		val *= 10;
		val += (data[n]-'0');
		if (val>12) {
			uartSend("ERR: Hodnota mimo rozsah ("); uartSend(val);
			uartSend(") povoleno 0-12");
			return 255;
		}
	}
	uartSend("val:"); uartSend(val); uartSend('\n');
	return val;
}

uint16_t strToValB(char* data) { // ===========================================
	uint16_t val = 0;
	for (uint8_t n=0; n<12; n++) {
		if (data[n]!='0' && data[n]!='1') {
			uartSend("ERR: Neocekavany znak (pos:"); uartSend(n);
			uartSend(" '"); uartSend(data[n]);
			uartSend("'="); uartSend((uint8_t)data[n]);
			uartSend(") ocekavano 0/1\n");
			return 65535;
		}
		val |= (data[n]-'0') << n;
	}
	uartSend("val:"); uartSend(static_cast<uint32_t>(val)); uartSend('\n');
	return val;
}