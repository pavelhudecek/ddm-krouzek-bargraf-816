// pokus-s2c - sériák, pokus 2, odvozená verze s cout
// Tady si zkusíme další C++ vychytávku: pøetìžování operátorù (cout.h),
// takže k vysílání použijeme cout << "a:" << a << " b:" << b << endl;
// A taky C #ifdef/#ifndef na detekci zda nìco bylo definováno nebo ne.
// Pøidáme ještì #include podmínìný existencí souboru pøes __has_include("config.h")
// Navíc se trochu zmìnil zpùsob èekání, takže cekej používá jen msSync.
// Tím se eliminují možná potíže s atomicitou ms10, ale zároveò se tím zhorší pøesnost,
// protože záleží na tom, kolik na zaèátku zbývá do dalšího msSync.
// Èekání navíc pøi držení tlaèítka každou s vysílá kolik s už je stisknuté.

// úkol: Doplnit pøíkaz "out-pwm-95 ", který zpùsobí, že pøerušení od èasovaèe
// na výstupu udìlá PWM 0-100 %. Použití pùvodního out- musí PWM vypnout.

#include <avr/io.h>
#define F_CPU (20000000UL)
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

#include "cout.h" // nový include rovnou z adresáøe projektu

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

void ledky(uint16_t a, uint16_t b) { // =======================================
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

	while (1) {
		//uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" -", bool wait0=true, uint8_t timeout=0) {
		uint8_t pos = uartRecv(data, cDataLen, " ", true, 255);
		cout << "end:" << uartRecvEndType << " pos:"<< pos <<' ';
		
		const char* prikazy[] = {"bara-b", "barb-b", "bara-", "barb-", "out-", "#"};
		uint8_t p, s;
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
				if (val8==255) break;
				ledA = 4095 >> 12-val8;
				isOK = true;
				break;
			case 3: // barb-5 nastavení zobrazené hodnoty 0-12 bargrafu
				val8 = strToVal8(data + s);
				if (val8==255) break;
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