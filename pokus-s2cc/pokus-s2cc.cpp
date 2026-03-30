// pokus-s2cc - sériák, pokus 2, verze s cout a cin
// Pokraèování v pøetìžování operátorù (cout-cin.h), pøidáme templaty.
// Navíc v nich potøebujeme constexpr, tak je v projektu zapnutý C++ 11
// což se udìlá pøidáním  -std=c++11 do Other flags v nastavení C++ toolchainu.
// Pøibylo i uartSend/cout na float a nastavení k nìmu, to taky chce min C++ 11

#include <avr/io.h>
#define F_CPU (20000000UL)
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

#include "cout-cin.h" // nový include rovnou z adresáøe projektu

#define SW_port		PORTB
#define SW_bm		(1<<4)

#define LED_port	PORTC
#define LEDTR_port	PORTB
#define LEDTR_Rbp	3
#define LEDTR_Ybp	2
#define LEDTR_Gbp	1
#define LEDTR_ABbm	(1<<0)

#define TLAC_port	PORTA
#define TLAC_bm		(1<<0)

#define F_CPUkor	(F_CPU + ((F_CPU * (int8_t)SIGROW.OSC20ERR5V) / 1024L))
#define UART_baud(br) ((float)(F_CPUkor*64 / (16*(float)br)) + 0.5)
#define UART_bd		19200

volatile uint8_t	sekSync=0, msSync=0;
uint8_t				ledJas = 20;
uint16_t			ledA, ledB; // stavy LEDek v bargrafech

void ledky (int a, int b) { // ================================================
	ledA = a;
	ledB = b;
}

void bargrafy(uint8_t a, uint8_t b) { // ======================================
	ledky(4095 >> 12-a, 4095 >> 12-b);
}


void cekej(uint16_t n) { // ===================================================
	uint16_t ms=0;
	
	do {
		if (msSync==1) {
			msSync = 0;
			
			ms++;
			uartRecvMsCnt++;
		}
		
		if (sekSync==1) {
			sekSync=0;
			
			static int tlacCas=0;
			if (!(TLAC_port.IN & TLAC_bm) && !uartSendSemafor) {
				cout << "Tlacitko tlaceno " << tlacCas << " s\n";
				tlacCas++;
			} else {
				tlacCas = 0;
			}
		}
		
	} while(ms < n);
}

// template: Pro každý typ se vytvoøí nová verze jako kdyby bylo pøetížení
template <typename T> bool testLimit(T val, T min, T max) { // ================
	if (val<min || val>max) {
		cout << "ERR: Hodnota mimo rozsah (" << val;
		cout << ") povoleno " << min << '-' << max << ')' << endl;
		return false;
	}
	return true;
}

int main(void) { // ###########################################################
	SW_port.DIR = SW_bm; // 0b00010000;
	
	_PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 0);		// nedìlit f CPU, bude 20 MHz
	// --------------------------------------------------------------
	PORTA_PIN0CTRL = PORT_PULLUPEN_bm;
	LED_port.DIR = 0b1111;
	LEDTR_port.DIRSET = 0b1111;
	// --------------------------------------------------------------
	TCA0.SINGLE.CTRLA = 0;
	TCA0.SINGLE.CTRLB = 0;
	TCA0.SINGLE.CTRLC = 0;
	TCA0.SINGLE.CTRLD = 0;
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
	TCA0.SINGLE.PER = F_CPU/10000UL;
	TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm;
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
	// --------------------------------------------------------------
	PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;	// RxD - PORTA.2, TxD - PORTA.1
	PORTA.DIRSET = 1<<1;
	PORTA.OUTSET = 1<<1;
	PORTA_PIN2CTRL = PORT_PULLUPEN_bm;
	
	USART0.CTRLA = 0;
	USART0.CTRLB = USART_RXEN_bm + USART_TXEN_bm;
	USART0.CTRLC = USART_CHSIZE_8BIT_gc;
	USART0.BAUD = UART_baud(UART_bd);
	USART0.STATUS = USART_DREIF_bm;
	// --------------------------------------------------------------
	__asm("sei");
	const uint8_t cDataLen = 30;
	char data[cDataLen+1];
	uartSend("\n\nstart\nbara-b010100001111 / barb-b010100001111 (bin 12 mist) // bara-7 / barb-11 (dec 0-12) // out-1 (0/1/t)\n");
	uartSend("Na konci kazdeho prikazu je mezera: 'bara-11 '\n\n");

	//cout << "test n:" << 12.3456 << endl;
	
	/*cout << "cekani na test string input... ";
	char test[] = {0,0,0,0,0};
	cin >> test;
	cout << "test ch:'" << test << "' pos:" << cin.pos << " e:" << cin.endType << endl;
	if (cin.endType!='e') {
		char ee = cin.endType;
		cin >> cin.ignore;
		cout << "ERR: Chyba cin, endType:" << ee << " ocekavano:e" << endl;
		cout << "Zbyla data zahozena: pos:" << cin.pos << " e:" << cin.endType << endl;
	}
	cout << "cekani na test uint32 input... ";
	uint32_t t32;
	cin >> t32;
	cout << "test n:'" << t32 << "' pos:" << cin.pos << " e:" << cin.endType << endl;
	if (cin.evEndType!='e') {
		char ee = cin.evEndType;
		cin >> cin.ignore;
		cout << "ERR: Chyba cin, evEndType:" << ee << " ocekavano:e" << endl;
		cout << "Zbyla data zahozena: pos:" << cin.evPos << " e:" << cin.endType << endl;
	}
	while(1) {
		cout << "cekani na test int32 input... ";
		int32_t t32i;
		cin >> t32i;
		cout << "test n:'" << t32i << "' pos:" << cin.pos << " e:" << cin.endType << endl;
		if (cin.evEndType!='e') {
			char ee = cin.evEndType;
			cin >> cin.ignore;
			cout << "ERR: Chyba cin, evEndType:" << ee << " ocekavano:e" << endl;
			cout << "Zbyla data zahozena: pos:" << cin.evPos << " e:" << cin.endType << endl;
			cin >> cin.ignore;
			cout << "Jeste jednou: pos:" << cin.evPos << " e:" << cin.endType << endl;
		}
		cout << endl;
	}*/
	
	cout.floatPrec = 8;
	cout.floatE = true;
	while(1) {
		cout << "cekani na test float input...\n";
		float ff;
		cin >> ff;
		cout << "test val:'" << ff << "' pos:" << cin.pos << " e:" << cin.endType << endl;
		if (cin.evEndType!='e' && cin.evEndType!='m') {
			cout << "ERR: Chyba cin, evEndType:" << cin.evEndType << " ocekavano: e/m" << endl;
			cout << "Zahazovani zbylych dat..." << endl;
			cin >> cin.ignore;
			cout << "Hotovo:" << cin.evPos << " e:" << cin.endType << endl;
		}

		cout << endl;
	}

	
	while (1) {
		//uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" -", bool wait0=true, uint8_t timeout=0) {
		uint8_t pos = uartRecv(data, cDataLen, " ", true, 255);
		cout << "end:" << uartRecvEndType << " pos:"<< pos <<' ';
		
		const char* prikazy[] = {"bara-b", "barb-b", "bara-", "barb-", "out-", "#"};
		uint8_t p, s=0;
		for (p=0; p<6; p++) {
			if (prikazy[p][0]=='#') break;
			for (s=0; s<7; s++) {
				if (s>=pos) break;
				if (data[s]!=prikazy[p][s]) break;
				if (prikazy[p][s]==0) break;
			}
			if (prikazy[p][s]==0) break;
		}
		cout << "p:" << p << " s:" << s << endl;
		
		uint16_t val16;
		uint8_t val8;
		bool isOK = false;
		switch(p) {
			case 0: // bara-b000... nastavení bin. hodnoty všech LEDek
				val16 = strToVal16b(data + s, 12);
				if (val16==65535) break;
				ledA = val16;
				isOK = true;
				break;
			case 1: // barb-b000... nastavení bin. hodnoty všech LEDek
				val16 = strToVal16b(data + s, 12);
				if (val16==65535) break;
				ledB = val16;
				isOK = true;
				break;
			case 2: // bara-5 nastavení zobrazené hodnoty 0-12 bargrafu
				val8 = strToVal8(data + s);
				if (!testLimit(val8, (uint8_t)0, (uint8_t)12)) break;
				ledA = 4095 >> 12-val8;
				isOK = true;
				break;
			case 3: // barb-5 nastavení zobrazené hodnoty 0-12 bargrafu
				val8 = strToVal8(data + s);
				if (!testLimit(val8, (uint8_t)0, (uint8_t)12)) break;
				ledB = 4095 >> 12-val8;
				isOK = true;
				break;
			case 4: // out-
				switch(data[4]) {
					case '0':
						SW_port.OUTCLR = SW_bm;
						break;
					case '1':
						SW_port.OUTSET = SW_bm;
						break;
					case 't':
						SW_port.OUTTGL = SW_bm;
						break;
					default:
						cout <<"ERR: Neocekavany znak (pos:4 '" << data[4];
						cout << "'=" << (uint8_t)data[4] << ") ocekavano 0/1/t\n";
				}
				if (data[4]=='0' || data[4]=='1' || data[4]=='t') {
					if (SW_port.OUT & SW_bm) uartSend("out:ON\n");
					else					 uartSend("out:OFF\n");
					isOK = true;
				}
				break;
			default: // neznamo
				uartSend("ERR: Neznamy prikaz\n");
		}
		if (isOK) uartSend("OK\n");
	}
}

ISR (TCA0_OVF_vect) { // ======================================================
	static char n = 0;
	static int	sekCnt = 0;
	static char	msCnt = 0;
	
	LED_port.OUTCLR = 0b1111;
	LEDTR_port.OUTCLR = 0b1111;
	LEDTR_port.OUTSET = 1 << (LEDTR_Gbp + (n % 3));
	
	if (n>2 && n<6) {
		LEDTR_port.OUTSET = LEDTR_ABbm;
		LED_port.OUTSET = (ledA >> (n - 3) * 4) & 0b1111;
	} else if (n<6) {
		LEDTR_port.OUTCLR = LEDTR_ABbm;
		LED_port.OUTSET = (ledB >> n * 4) & 0b1111;
	} else {
		// 6 az 6+ledJas zhasnuto
	}
	
	n++;
	if (n > 6+ledJas) n=0;
	
	if (++sekCnt>10000) {
		sekCnt = 0;
		sekSync = 1;
	}
	if (++msCnt>10) {
		msCnt = 0;
		msSync = 1;
	}
	
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}