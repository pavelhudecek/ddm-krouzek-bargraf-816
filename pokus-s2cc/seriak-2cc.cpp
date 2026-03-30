#include <avr/io.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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
//#warning "seriak-2cc bbbbbbbbbbbbbbbbbbbbbbbbbb"
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
void uartSend(const char* text) { // ================================================
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

// i jen inicializovano / r recv chr / t timeout / e endchar, end str
// b buf. overflow / v val. overflow / n no chars / u unknown
char	uartRecvEndType = 'i';
char	strToValEndType = 'i';

const char		cNoRecv = 255;
const uint8_t	cEndCharLimit = 10;
char uartRecv(uint8_t timeout=255) { // =======================================
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
// pøijmout data, max délky dSize, až po jeden z endChars,
// neomezenì èekat na první znak, timeout u ostatních
// data==NULL: data neukládat, ale jinak všechno stejnì - ignorování dat tøeba po chybì
uint8_t uartRecv(char* data, uint8_t dSize, char* endChars=" \n", bool wait0=true, uint8_t timeout=255) {
	uint8_t n=0;
	
	for (n=0; n<dSize; n++) {
		char d = uartRecv((n==0 && wait0) ? 0 : timeout);
		//if (data!=NULL) data[n] = d;
		data[n] = d;
		for (uint8_t e=0; e<cEndCharLimit; e++) {
			if (endChars[e]==0) break;
			if (endChars[e]==1 || endChars[e]==2) {
				if (d<'0' || d>'9') {
					if (endChars[e]==1) {			// \1 akceptovat jen èíslice
						uartRecvEndType = 'e';
						return n;
					} else if (d!='+' && d!='-') { // \2 èíslice a +/-
						uartRecvEndType = 'e';
						return n;
					}
				}
			} else if (d==endChars[e]) {
				uartRecvEndType = 'e';
				return n;
			}
		}
		if (uartRecvEndType!='r') return n;
	}
	if (n==dSize) {uartRecvEndType = 'b'; return n;}
	uartRecvEndType = 'u';
	return n;
}

uint8_t valToStr(uint32_t x, char* data, uint8_t maxChars) { // ===============
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

uint8_t inStr(char* data, uint8_t dSize, const char* searched) { // ===========
	//uartSend("inStr-str: d:'"); uartSend(data); uartSend("' dSize:"); uartSend(dSize);
	//uartSend(" s:'"); uartSend(searched); uartSend("'\n");
	uint8_t n;
	for (n=0; n<dSize; n++) if (data[n]==searched[0] || data[n]==0) break;
	if (n<dSize && data[n]!=0) {
		for (uint8_t i=0; n+i < dSize; i++) {
			if (searched[i]==0) return n+i;
			if (data[n+i]!=searched[i] || data[n+i]==0) return 255;
		}
	}
	return 255;
}
uint8_t inStr(char* data, uint8_t dSize, char searched) { // ==================
	//uartSend("inStr-chr: d:'"); uartSend(data); uartSend("' dSize:"); uartSend(dSize);
	//uartSend(" s:"); uartSend(searched); uartSend('\n');	
	for (uint8_t n=0; n<dSize; n++) {
		if (data[n]==searched) return n;
		if (data[n]==0) break;
	}
	return 255;
}

char	strToValEndChar=' '; // konec textu k pøevodu, pokud není endChars
uint8_t	strToValPos=0;

// podle návratového typu pøetìžovat nejde, tak rùzný jména:
uint32_t strToVal32(char* data, uint8_t maxChars=10, char* endChars=NULL) { // =====================
	uint32_t val = 0;

	strToValEndType = 'e';
	for (strToValPos=0; strToValPos<maxChars; strToValPos++) {
		char d = data[strToValPos];
		if (endChars==NULL && d==strToValEndChar || d==0) {
			return val;
		} else if (inStr(endChars, 5, d) != 255) { // sizeof nefungovalo, tak limit 5
				return val;
		}
		if (d<'0' || d>'9') {
			uartSend("ERR: Neocekavany znak (pos:"); uartSend(strToValPos);
			uartSend(" '"); uartSend(d);
			uartSend("'="); uartSend((uint8_t)d);
			uartSend(") ocekavano 0-9\n");
			strToValEndType = 'n';
			return val;
		}
		if (val>429496729 || val==429496729 && d>'6') {
			uartSend("ERR: Max32b prekroceno (val:"); uartSend(val);
			uartSend(" pos:"); uartSend(strToValPos); uartSend(" [pos]:"); uartSend(d); uartSend(")\n");
			strToValEndType = 'v';
			return val;			
		}
		val *= 10;
		val += (d-'0');
	}
	//uartSend("val:"); uartSend(val); uartSend('\n');
	// az do maxChars - strToValPos=maxChars
	return val;
}
uint16_t strToVal16(char* data, uint8_t maxChars=5) { // ======================
	return static_cast<uint16_t>(strToVal32(data, maxChars));
}
uint8_t strToVal8(char* data, uint8_t maxChars=3) { // ========================
	return static_cast<uint8_t>(strToVal32(data, maxChars));
}
int32_t strToVal32i(char* data, uint8_t maxChars=11, char* endChars=NULL) { // =====================
	int32_t tmp;
	if (data[0]=='-') {
		tmp = static_cast<int32_t>(strToVal32(data+1, maxChars-1, endChars)) * (-1);
		strToValPos++;
		return tmp;
	} else if (data[0]=='+') {
		tmp = static_cast<int32_t>(strToVal32(data+1, maxChars-1, endChars));
		strToValPos++;
		return tmp;
	} else {
		return static_cast<int32_t>(strToVal32(data, maxChars, endChars));
	}
}
int16_t strToVal16i(char* data, uint8_t maxChars=6) { // ======================
	return static_cast<int16_t>(strToVal32i(data, maxChars));
}
int8_t strToVal8i(char* data, uint8_t maxChars=4) { // ========================
	return static_cast<int8_t>(strToVal32i(data, maxChars));
}

uint32_t strToVal32b(char* data, uint8_t bits, char* endChars=NULL) { // ======
	uint32_t val = 0;
	
	strToValEndType = 'e';
	for (strToValPos=0; strToValPos<bits; strToValPos++) {
		char d = data[strToValPos];
		if (endChars==NULL && d==strToValEndChar || d==0) {
			return val;
		} else if (inStr(endChars, sizeof(endChars), d) != 255) {
			return val;
		}
		if (d!='0' && d!='1') {
			uartSend("ERR: Neocekavany znak (pos:"); uartSend(strToValPos);
			uartSend(" '"); uartSend(d);
			uartSend("'="); uartSend((uint8_t)d);
			uartSend(") ocekavano 0/1\n");
			strToValEndType = 'n';
			return val;
		}
		val <<= 1;
		val |= d-'0';
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

float strToValF(char* data, uint8_t dSize=12, char point='.') { // ============
	uint8_t pPos;
	for (pPos=0; pPos<dSize; pPos++) {
		if (data[pPos]==0) break;
		if (data[pPos]==strToValEndChar) break;
		if (data[pPos]==point) break;
	}
	uartSend("pPos_0:"); uartSend(pPos); uartSend(" d:'"); uartSend(data[pPos]); uartSend("' ");

	if (data[pPos]!=point) {					// bez .
		return static_cast<float>(strToVal32i(data, pPos));
	} else if (data[pPos]==point) {				// nalezen symbol des. .
		int32_t a = 0;
		if (pPos>0) a = strToVal32i(data, pPos);// bylo 1.23 ne .123
		uint32_t b = 0;
		if (pPos+1 < dSize) {				// bylo nìco za .
			b = strToVal32(data+pPos+1, dSize-pPos-(pPos!=0 ? 1 : 2));
		} else {
			return static_cast<float>(a);		// za . nebylo nic
		}
		uartSend("pPos:"); uartSend(pPos); uartSend(' ');
		uartSend("strToValPos:"); uartSend(strToValPos); 
		uartSend(" e:"); uartSend(strToValEndType);
		uartSend(" a:"); uartSend(a); uartSend(" b:"); uartSend(b);
		float div = 1.0;
		if (a<0) div=-1.0; // jinak by z -123.456 bylo -122.544
		uint8_t n;
		for (n=0; n < strToValPos; n++) div *= 10.0;
		uartSend(" n:"); uartSend(n); uartSend(" div:"); uartSend(div); uartSend('\n');
		return static_cast<float>(a) + static_cast<float>(b) / div;
	} else {
		uartSend("else-point pPos:"); uartSend(pPos); uartSend(' ');
	}
	strToValEndType = 'n'; // nic nenaèteno
	return NAN; // not a number - "neèíslo"
	// C++ má normálnì i signaling_NaN a quiet_NaN ale tady nemáme <cmath> s nima, ani exceptiony
}
/*float strToValFe_(char* data, uint8_t dSize=30, char point='.') { // ===========
	uint8_t pPos = 255;		// pozice .
	uint8_t ePos = 255;		// pozice E/e
	uint8_t endPos = 255;	// pozice konce
	float tmp = NAN;
	int32_t a=0, b=0, e=0; // aaa.bbbEeee "-12.345e-24 " -> a=-12 b=345 e=-24
	uint8_t aChars=0, bChars=0, eChars=0;
	
	auto errMsg = [=](const char* msg) { // ----------------------------------
		uartSend("ERR: "); uartSend(msg);
		#ifdef DEBUG_messages
		uartSend(" (pPos:"); uartSend(pPos); uartSend(" ePos:"); uartSend(ePos); uartSend(" end:"); uartSend(endPos);
		uartSend(" aCh:"); uartSend(aChars); uartSend(" bChr:"); uartSend(bChars);
		uartSend(" eCh:"); uartSend(eChars);
		uartSend(" a:"); uartSend(a); uartSend(" b:"); uartSend(b); uartSend(" e:"); uartSend(e);
		uartSend(" strToValEndType:"); uartSend(strToValEndType);
		uartSend(" strToValPos:"); uartSend(strToValPos); uartSend(')');
		#endif //DEBUG_messages
		uartSend('\n');
	}; // --------------------------------------------------------------
	
	for (uint8_t n=0; n<dSize; n++) { // zjistit pozice . / e / konce
		if (data[n]==0)						{endPos=n; break;}
		if (data[n]==strToValEndChar)		{endPos=n; break;}
		if (data[n]==point)					{pPos=n; }
		if (data[n]=='e' || data[n]=='E')	{ePos=n; }
	}
	
	#ifdef DEBUG_messages
	uartSend(" d:'"); uartSend(data);
	uartSend("' pPos:"); uartSend(pPos); uartSend(" ePos:"); uartSend(ePos);
	uartSend(" end:"); uartSend(endPos); uartSend('\n');
	#endif //DEBUG_messages
	
	if (endPos==255) endPos = dSize; // tøeba je èíslo pøes celej buffer
	if (endPos==3) {
		if (data[0]=='N' && data[1]=='A' && data[2]=='N') {			// NAN
			strToValEndType = 'e';
			strToValPos = endPos;
			return NAN;
		} else if (data[0]=='I' && data[1]=='N' && data[2]=='F') {	// INF
			strToValEndType = 'e';
			strToValPos = endPos;
			return INFINITY;
		}
	}
	if (endPos <= ((pPos!=255)+(ePos!=255))) {
		errMsg("Nejsou cisla");
		strToValEndType = 'n'; // nic nenaèteno
		return NAN; // not a number - "neèíslo"
	}

	if (pPos!=255) { // byla .
		aChars = pPos;
		bChars = (ePos!=255) ? ePos-pPos-1 : endPos-pPos-1;
	} else {		 // nebyla .
		aChars = (ePos!=255) ? ePos-1 : endPos-1;
	}
	eChars = (ePos!=255) ? endPos-ePos-1 : 0;
	
	#ifdef DEBUG_messages
	uartSend("aCh:"); uartSend(aChars); uartSend(" bChr:"); uartSend(bChars);
	uartSend(" eCh:"); uartSend(eChars); uartSend("\n");
	#endif //DEBUG_messages

	if (aChars) {
		a = strToVal32i(data, aChars);
		if (strToValEndType!='e') {errMsg("a EndType!='e'"); return NAN;}
	}
	if (bChars) {
		b = strToVal32(data+pPos+1, bChars);
		if (strToValEndType!='e') {errMsg("b EndType!='e'"); return NAN;}
	}
	if (eChars) {
		e = strToVal32i(data+ePos+1, eChars);
		if (strToValEndType!='e') {errMsg("e EndType!='e'"); return NAN;}
	}
	#ifdef DEBUG_messages
	uartSend("a:"); uartSend(a); uartSend(" b:"); uartSend(b); uartSend(" e:"); uartSend(e);
	#endif //DEBUG_messages
	
	float div = 1.0;
	if (data[0]=='-') div=-1.0; // jinak by z -123.456 bylo -122.544 / if (a<0) nefunguje pri -0.123
	uint8_t n;
	for (n=0; n < bChars; n++) div *= 10.0;
	tmp = static_cast<float>(a) + static_cast<float>(b) / div;
	
	#ifdef DEBUG_messages
	uartSend(" n:"); uartSend(n); uartSend(" div:"); uartSend(div);
	uartSend(" tmp:"); uartSend(tmp, 0, 7); uartSend('\n');
	#endif //DEBUG_messages
	
	strToValPos = endPos;
	return tmp * pow(10.0, static_cast<float>(e));
}*/
float strToValFe(char* data, uint8_t dSize=30, char point='.') { // ===========
	uint8_t  lastPos = 0;
	uint8_t  aChars=0, bChars=0, eChars=0;
	float	 tmp = NAN;
	int32_t  a=0, e=0; // aaa.bbbEeee "-12.345e-24 " -> a=-12 b=345 e=-24
	uint32_t b=0;
	
	auto errMsg = [=](const char* msg) { // ----------------------------------
		uartSend("ERR: "); uartSend(msg);
		#ifdef DEBUG_messages
		uartSend(" (dSize:"); uartSend(dSize); uartSend(" lastPos:"); uartSend(lastPos);
		uartSend(" bChars:"); uartSend(bChars); uartSend(" eChars:"); uartSend(eChars);
		uartSend(" a:"); uartSend(a); uartSend(" b:"); uartSend(b); uartSend(" e:"); uartSend(e);
		uartSend(" strToValEndType:"); uartSend(strToValEndType);
		uartSend(" strToValPos:"); uartSend(strToValPos); uartSend(')');
		#endif //DEBUG_messages
		uartSend('\n');
	}; // --------------------------------------------------------------
	
	strToValEndType = 'e';
	if (inStr(data, dSize, "NAN")==0)  return NAN;
	if (inStr(data, dSize, "INF")==0)  return INFINITY;
	if (inStr(data, dSize, "+INF")==0) return INFINITY;
	if (inStr(data, dSize, "-INF")==0) return -INFINITY;
	
	char end1[] = ".eE ";
	end1[0] = point;
	//strToVal32i(char* data, uint8_t maxChars=11, char* endChars=NULL)
	a = strToVal32i(data, dSize, end1);
	if (strToValEndType!='e') {errMsg("a EndType!='e'"); return NAN;}
	aChars = strToValPos;
	lastPos += strToValPos;
		
	if (data[lastPos]=='.') {
		lastPos++;
		b = strToVal32(data+lastPos, dSize-lastPos, "eE ");
		if (strToValEndType!='e') {errMsg("b EndType!='e'"); return NAN;}
		bChars = strToValPos;
	}
	lastPos += strToValPos;
	if (data[lastPos]=='e' || data[lastPos]=='E') {
		lastPos++;
		e = strToVal32i(data+lastPos, dSize-lastPos);
		if (strToValEndType!='e') {errMsg("e EndType!='e'"); return NAN;}
		eChars = strToValPos;
	}
	strToValPos = lastPos+strToValPos;
	
	#ifdef DEBUG_messages
	uartSend("dSize:"); uartSend(dSize);
	uartSend(" a:"); uartSend(a); uartSend(" b:"); uartSend(b); uartSend(" e:"); uartSend(e);
	#endif //DEBUG_messages
	
	float div = 1.0;
	if (data[0]=='-') div=-1.0; // jinak by z -123.456 bylo -122.544 / if (a<0) nefunguje pri -0.123
	uint8_t n;
	for (n=0; n < bChars; n++) div *= 10.0;
	tmp = static_cast<float>(a) + static_cast<float>(b) / div;
	
	#ifdef DEBUG_messages
	uartSend(" n:"); uartSend(n); uartSend(" div:"); uartSend(div);
	uartSend(" tmp:"); uartSend(tmp, 0, 7); uartSend('\n');
	#endif //DEBUG_messages
	
	if (e<-250 || e>250) {
		errMsg("E mimo rozsah +/-250");
		strToValEndType='u';
		return NAN;
	}
	float m0 = 10.0;
	uint8_t nn = static_cast<uint8_t>(e);
	if (e<0) {
		m0 = 0.1;
		nn = static_cast<uint8_t>(-e);
	}
	float mult = 1.0;
	for (n=0; n < nn; n++) mult *= m0;

	#ifdef DEBUG_messages
	uartSend("n:"); uartSend(n); uartSend(" m0:"); uartSend(m0);
	uartSend(" mult:"); uartSend(mult);	uartSend('\n');
	#endif //DEBUG_messages

	// .000000000000001 funguje, ale o 0 víc tady spadne pøestože funguje i 1e-25
	return tmp * mult;
}