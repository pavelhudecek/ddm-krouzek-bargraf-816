#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

// tady máme separovaný vìci k sériáku, to se pak dá snadno pøidat do jiných projektù

// doplnìn float a jsou k nìmu nastavení: SERIAK_dtostrf_dtostre, SERIAK_dtostre
// default je že jde jen e=false a výstup je ve stylu 123.456				(+1,7 kB)
// #define SERIAK_dtostrf_dtostre - podle e je buï 123.456 nebo 1.234E+02	(+2 kB)
// #define SERIAK_dtostre - jde jen e=true, výstup je ve stylu 1.234E+02	(+1,3 kB)
// nastavení mùžou bejt buï v config-seriak.h nebo v config.h
#if __has_include("config-seriak.h")
#include "config-seriak.h"
#else
#if __has_include("config.h")
#include "config.h"
#endif
#endif
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

uint8_t	uartRecvMsCnt = 0;		// timeout v uartRecv (nutno inkrementovat po ms)
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
	for (; n<10; n--) uartSend(text[n]);
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
// x: hodnota, wf: míst celkem 0=auto, prec: míst za ., e: použít formát 1.23E+15
// pøi e je wf: flags od dtostre místo width od dtostrf
// flags: DTOSTRE_ALWAYS_SIGN: +/-, DTOSTRE_PLUS_SIGN: mezera/-, DTOSTRE_UPPERCASE: E
void uartSend(float x, int8_t wf=0, uint8_t prec=3, bool e=false) { // =======
	char buf[20];
#ifdef SERIAK_dtostrf_dtostre					// 2 kB
	if (!e) {
		//https://onlinedocs.microchip.com/oxy/GUID-317042D4-BCCE-4065-BB05-AC4312DBC2C4-en-US-2/GUID-38167418-D330-4F8E-A500-401B562D86F3.html
		//char *dtostrf(double val, signed char width, unsigned char prec, char *s);
		dtostrf(x, wf, prec, buf);
	} else {
		//https://onlinedocs.microchip.com/oxy/GUID-317042D4-BCCE-4065-BB05-AC4312DBC2C4-en-US-2/GUID-E8A5F506-EC5B-41AA-A4EC-3149E7D1B721.html
		//char *dtostre(double val, char *s, unsigned char prec, unsigned char flags);
		dtostre(x, buf, prec, wf);
	}
#else
#ifdef SERIAK_dtostre							// 1,2 kB
	if (!e)	{uartSend("(disabled)"); return;}
	else	{dtostre(x, buf, prec, wf);}
#else											// 1,7 kB
	if (e)	{uartSend("(disabled)"); return;}
	else	{dtostrf(x, wf, prec, buf);}
#endif
#endif // SERIAK_dtostrf_dtostre
	uartSend(buf);
}

const char		cNoRecv = 255;
const uint8_t	cEndCharLimit = 10;
// i jen inicializovano / r recv chr / t timeout / e endchar / b buf. overflow / u unknown
char			uartRecvEndType = 'i';
char uartRecv(uint8_t timeout=0) { // =========================================
	uartRecvMsCnt = 0;
	while ((USART0.STATUS & USART_RXCIF_bm) == 0) {
		cekej(0);
		if (uartRecvMsCnt >= timeout && timeout != 0) {
			uartRecvEndType = 't';
			return cNoRecv;
		}
	}
	uartRecvEndType = 'r';
	return USART0.RXDATAL;
}
// pøijmout char* data, max délky limit, až po endChar, èekat na první znak, timeout u ostatních
uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" \n", bool wait0=true, uint8_t timeout=0) {
	uint8_t n=0;
	
	if (wait0) { // neomezenì èekat na první znak
		data[n] = uartRecv(0);
		n++;
	}
	for (; n<limit; n++) {
		data[n] = uartRecv(timeout);
		for (uint8_t e=0; e<cEndCharLimit; e++) {
			if (endChars[e]==0) break;
			if (endChars[e]==1 || endChars[e]==2) {
				if (data[n]<'0' || data[n]>'9') {
					if (endChars[e]==1) {				// \1 akceptovat jen èíslice
						uartRecvEndType = 'e';
						return n;
					} else if (data[n]!='+' && data[n]!='-') { // \2 èíslice a +/-
						uartRecvEndType = 'e';
						return n;
					}
				}
			} else if (data[n]==endChars[e]) {
				uartRecvEndType = 'e';
				return n;
			}
		}
		if (uartRecvEndType!='r') return n;
	}
	if (n==limit) {uartRecvEndType = 'b'; return n;}
	uartRecvEndType = 'u';
	return n;
}

uint8_t valToStr(uint32_t x, char* data, uint8_t maxChars) { // ==============
	uint32_t xx = x;
	uint8_t n;
	
	for (n=0; n<10; n++) {
		if (n>maxChars) return 255; // pøi pøeteèení je v data všechno co se vešlo
		data[maxChars-n] = xx % 10 + '0';
		xx /= 10;
		if (xx==0) break;
	}
	uint8_t pos = n;
	if (n < maxChars) {
		for (n=0; n<=pos; n++) data[n] = data[n + (maxChars-pos)];
	}
	return pos;
}

// podle návratového typu pøetìžovat nejde, tak rùzný jména:
uint32_t strToVal32(char* data, uint8_t maxChars=10) { // =====================
	uint32_t val = 0;
	for (uint8_t n=0; n<maxChars; n++) {
		if (data[n]==' ') break;
		if (data[n]<'0' || data[n]>'9') {
			uartSend("ERR: Neocekavany znak (pos:"); uartSend(n);
			uartSend(" '"); uartSend(data[n]);
			uartSend("'="); uartSend((uint8_t)data[n]);
			uartSend(") ocekavano 0-9\n");
			val = 0;
			return val-1;
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
uint16_t strToVal16(char* data, uint8_t maxChars=5) { // ======================
	return static_cast<uint16_t>(strToVal32(data, maxChars));
}
uint8_t strToVal8(char* data, uint8_t maxChars=3) { // ========================
	return static_cast<uint8_t>(strToVal32(data, maxChars));
}

uint32_t strToVal32b(char* data, uint8_t bits) { // ===========================
	uint32_t val = 0;
	for (uint8_t n=0; n<bits; n++) {
		if (data[n]!='0' && data[n]!='1') {
			uartSend("ERR: Neocekavany znak (pos:"); uartSend(n);
			uartSend(" '"); uartSend(data[n]);
			uartSend("'="); uartSend((uint8_t)data[n]);
			uartSend(") ocekavano 0/1\n");
			val = 0;
			return val-1;
		}
		val |= (data[n]-'0') << n;
	}
	uartSend("val:"); uartSend(val); uartSend('\n');
	return val;
}
uint16_t strToVal16b(char* data, uint8_t bits) { // ===========================
	return static_cast<uint16_t>(strToVal32b(data, bits));
}
uint16_t strToVal8b(char* data, uint8_t bits) { // ============================
	return static_cast<uint8_t>(strToVal32b(data, bits));
}
