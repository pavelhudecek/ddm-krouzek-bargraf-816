#ifndef COUT_H_ // tohle je include guard - ochrana proti vícenásobnému includu
#define COUT_H_ // na konci souboru je ještì #endif
// Modernìjší øešení je #pragma once, ale mùže zlobit ve složitìjších projektech

// vìci z externích souborù musíme dodefinovat pøes extern.
// Pokud je jich víc, je vhodné ty externy naházet do .h souboru a ten pak includovat.
// v našem pøípadì je to tak napùl, že externy od seriak.cpp jsou pøifaøené v tomto .h co je taky potøebuje
// nebo pokud existuje seriak.h, includne se
#if __has_include("seriak.h")
	#include "seriak.h"
#else
	extern void uartSend (char b);
	extern void uartSend(char* text);
	extern void uartSend(uint32_t x);
	extern void uartSend(uint16_t x);
	extern void uartSend(uint8_t x);
	extern void uartSend(int32_t x);
	extern void uartSend(int16_t x);
	extern void uartSend(int8_t x);
	extern void uartSend(bool x);
	extern void uartSend(float x, int8_t wf=0, uint8_t prec=3, bool e=false);

	extern char uartRecv(uint8_t timeout=0);
	extern uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" -", bool wait0=true, uint8_t timeout=0);

	extern uint32_t strToVal32(char* data, uint8_t maxChars=10);
	extern uint16_t strToVal16(char* data, uint8_t maxChars=5);
	extern uint8_t  strToVal8(char* data, uint8_t maxChars=3);

	extern uint32_t strToVal32b(char* data, uint8_t bits);
	extern uint16_t strToVal16b(char* data, uint8_t bits);
	extern uint8_t  strToVal8b(char* data, uint8_t bits);

	extern uint8_t	uartRecvMsCnt;	// timeout v uartRecv (nutno inkrementovat po ms)
	extern bool		uartSendSemafor;// semafor aby se nezacyklilo pøi volání z cekej
	extern char		uartRecvEndType;// i jen inicializovano / r recv chr / t timeout
									// e endchar / b buf. overflow / u unknown

	// pøi neexistenci seriak.h se navíc zkoušej includovat configy:
	#if __has_include("config-seriak.h")
	#include "config-seriak.h"
	#else
	#if __has_include("config.h")
	#include "config.h"
	#endif // config.h
	#endif // config-seriak.h
	// Co kdyby v nich bylo: COUTCIN_noStatic
#endif // seriak.h

// ############################################################################

// static zaøídí, že se struktura vytvoøí "sama", takže po #include se už dá používat cin/cout/endl
// Jenže pak se vytvoøí kopie pro všechny .cpp, kam bylo inckudováno.
// #ifndef zaøídí, že pokud pøed #include dáme #define COUTCIN_noStatic, k vytvoøení
// nedojde a mùžeme si je vytvoøit sami jen tam kde chceme a jinam pak dát extern.
#ifndef COUTCIN_noStatic
	 static const struct endl_t {} endl = {};
#endif

// struct obvykle používáme na datové struktury, ale funguje i místo jednodušších class
struct tCout { // =============================================================
	tCout& operator<<(char c)	{uartSend(c);	return *this;}
	tCout& operator<<(char* s)	{uartSend(s);	return *this;}

	tCout& operator<<(uint32_t v){uartSend(v);	return *this;}
	tCout& operator<<(uint16_t v){uartSend(v);	return *this;}
	tCout& operator<<(uint8_t v) {uartSend(v);	return *this;}

	tCout& operator<<(int32_t v) {uartSend(v);	return *this;}
	tCout& operator<<(int16_t v) {uartSend(v);	return *this;}
	tCout& operator<<(int8_t v)	{uartSend(v);	return *this;}
	
	tCout& operator<<(endl_t) {uartSend('\n');	return *this;}
};
#ifndef COUTCIN_noStatic
	static tCout cout;
#endif


#endif // COUT-CIN_H_