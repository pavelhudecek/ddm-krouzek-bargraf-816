#pragma once // ochrana proti vícenásobnému includu

// vìci z externích souborù musíme dodefinovat pøes extern.
// Pokud je jich víc, je vhodné ty externy naházet do .h souboru a ten pak includovat.
// v našem pøípadì je to tak napùl, že externy od seriak.cpp jsou pøifaøené v tomto .h co je taky potøebuje
// nebo pokud existuje seriak.h, includne se
#if __has_include("seriak.h")
	#include "seriak.h"
#else
	extern void uartSend(char b);
	extern void uartSend(char* text);
	extern void uartSend(const char* text);
	extern void uartSend(uint32_t x);
	extern void uartSend(uint16_t x);
	extern void uartSend(uint8_t x);
	extern void uartSend(int32_t x);
	extern void uartSend(int16_t x);
	extern void uartSend(int8_t x);
	extern void uartSend(bool x);
	extern void uartSend(float x, int8_t wf=0, uint8_t prec=3, bool e=false);

	extern char uartRecv(uint8_t timeout=0);
	extern uint8_t uartRecv(char* data, uint8_t dSize, char* endChars=" /n", bool wait0=true, uint8_t timeout=255);

	extern uint32_t strToVal32(char* data, uint8_t maxChars=10, char* endChars=NULL);
	extern uint16_t strToVal16(char* data, uint8_t maxChars=5);
	extern uint8_t  strToVal8(char* data, uint8_t maxChars=3);
	extern int32_t	strToVal32i(char* data, uint8_t maxChars=11, char* endChars=NULL);
	extern int16_t	strToVal16i(char* data, uint8_t maxChars=6);
	extern int8_t	strToVal8i(char* data, uint8_t maxChars=4);
	extern float	strToValF(char* data, uint8_t dSize=12, char point='.');
	extern float strToValFe(char* data, uint8_t dSize=30, char point='.');

	extern uint32_t strToVal32b(char* data, uint8_t bits);
	extern uint16_t strToVal16b(char* data, uint8_t bits);
	extern uint8_t  strToVal8b(char* data, uint8_t bits);
	
	extern char		strToValEndType; // stejnì jako uartRecvEndType
	extern uint8_t	strToValPos;
	//extern char		strToValEndChar=' '; // konec textu k pøevodu

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
	//#warning "cin-cout bbbbbbbbbbbbbbbbbbbbbbbbbb"
	#endif // config.h
	#endif // config-seriak.h
	// Co kdyby v nich bylo: COUTCIN_noStatic
#endif // seriak.h

// ############################################################################

#ifndef is_same // staré C++ nemá is_same tak si detekci shody typù udìláme sami
#warning "Tato verze C++ nema is_same"
template<typename T, typename U> struct is_same {
	static constexpr bool value = false;
};
template<typename T> struct is_same<T, T> {
	static constexpr bool value = true;
};
#endif // is_same<T, float>::value

// prázdná struktura, která má jen jméno a vytvoøená promìnná není nikde v pamìti
static const struct endl_t {} endl = {}; // dá se pro ní ale pøetížit operátor

// struct obvykle používáme na datové struktury, ale funguje i místo jednodušších class
struct tCout { // =============================================================
	tCout& operator<<(char c)	{uartSend(c);	return *this;}
	tCout& operator<<(char* s)	{uartSend(s);	return *this;}
	tCout& operator<<(const char* s)	{uartSend((char*)s);	return *this;}

	/*tCout& operator<<(uint32_t v){uartSend(v);	return *this;}
	tCout& operator<<(uint16_t v){uartSend(v);	return *this;}
	tCout& operator<<(uint8_t v) {uartSend(v);	return *this;}

	tCout& operator<<(int32_t v) {uartSend(v);	return *this;}
	tCout& operator<<(int16_t v) {uartSend(v);	return *this;}
	tCout& operator<<(int8_t v)	{uartSend(v);	return *this;}*/

	int8_t	floatWF = 0;	// width: míst celkem, 0 automaticky / flags pro dtostre
	uint8_t floatPrec = 3;	// poèet míst za .
	bool floatE = false;	// 1.23E-04 místo 0.000123
	// lze pøenastavit cout.floatPrec=5;
	tCout& operator<<(float v)	{uartSend(v, floatWF, floatPrec, floatE); return *this;}
	tCout& operator<<(double v)	{uartSend(v, floatWF, floatPrec, floatE); return *this;}
	
	// všechny jednoduché tCout pro char, uint32, ... nahradil jeden template
	template<typename T> tCout& operator<<(T v) {
		uartSend(v);
		return *this;
	}
	
	tCout& operator<<(endl_t) {uartSend('\n');	return *this;}
};
// static zaøídí, že se struktura vytvoøí "sama", takže po #include se už dá používat cin/cout/endl
// Jenže pak se vytvoøí kopie pro všechny .cpp, kam bylo inckudováno.
// #ifndef zaøídí, že pokud pøed #include dáme #define COUTCIN_noStatic, k vytvoøení
// nedojde a mùžeme si je vytvoøit sami jen tam kde chceme a jinam pak dát extern.
#ifndef COUTCIN_noStatic
	static tCout cout;
#endif


struct tCin { // ==============================================================
	
//public:
	// prázdná struktura pro operátor, ale uvnitø struct: cin << cin.ignore;
	static constexpr struct ignore_t {} ignore = {}; // contsexpr, tady nejde const
	char endChars[10] = " \n";
	uint8_t pos = 255;
	char	endType = uartRecvEndType;
	char	evEndType = strToValEndType;
	uint8_t	evPos = strToValPos;
	
	// pøíjem char[] - pozor, správnì si urèí délku bufferu jen když je deklarován pøímo
	template<typename T, size_t N>	tCin& operator>>(T (&buf)[N]) {
		pos = uartRecv(buf, sizeof(buf)-1, endChars);
		endType = uartRecvEndType;
		return *this;
	}
	uint8_t ignLimit = 254;
	tCin& operator>>(ignore_t ign) { // zahazovaè zbytkù po errorech
		for (uint8_t n=0; n<ignLimit; n++) {
			char ign = uartRecv(255);
			pos++;
			if (uartRecvEndType=='t') break;
		}
		endType = uartRecvEndType;
		return *this;
	}
	tCin& operator>>(uint32_t& v) {
		char buff[11];
		//uint8_t uartRecv(char* data, uint8_t dSize, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)
		pos = uartRecv(buff, 10);
		endType = uartRecvEndType;
		if (uartRecvEndType != 'e') return *this;
		// uint32_t strToVal32(char* data, uint8_t maxChars=10)
		v = strToVal32(buff);
		evEndType = strToValEndType;
		evPos = strToValPos;
		return *this;
	}
	tCin& operator>>(int32_t& v) {
		char buff[12];
		//uint8_t uartRecv(char* data, uint8_t dSize, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)
		pos = uartRecv(buff, 11);
		endType = uartRecvEndType;
		if (uartRecvEndType != 'e') return *this;
		// uint32_t strToVal32i(char* data, uint8_t maxChars=11)
		v = strToVal32i(buff);
		evEndType = strToValEndType;
		evPos = strToValPos;
		return *this;
	}
	
	// float strToValF(char* data, char point, uint8_t maxChars)
	tCin& operator>>(float& v) {
		char buff[30];
		//uint8_t uartRecv(char* data, uint8_t dSize, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)
		pos = uartRecv(buff, 30);
		cout << " ptr:" << (uint16_t)buff << " buff:'" << buff << "' pos:" << pos << ' ';
		//cout << "data:";
		//for (uint8_t n=0; n<10; n++) cout << static_cast<uint8_t>(buff[n]) << ' ';// << "'" << buff[n] << "'";}
		//cout << endl;
		endType = uartRecvEndType;
		if (uartRecvEndType != 'e') return *this;
		// float strToValF(char* data, char point='.', uint8_t maxChars=12)
		v = strToValFe(buff, 30);
		evEndType = strToValEndType;
		evPos = strToValPos;
		return *this;
	}
	
	
	/*template<typename T> tCin& operator>>(T& v) {
		uint8_t digLimit = 0;
		char buff[25];
		if (is_same<T, uint32_t>::value) {
			digLimit = 10;
		} else if (is_same<T, uint16_t>::value) {
			digLimit = 5;
		} else if (is_same<T, uint8_t>::value) {
			digLimit = 3;
		} else if (is_same<T, int32_t>::value) {
			digLimit = 10+1;
		} else if (is_same<T, int16_t>::value) {
			digLimit = 5+1;
		} else if (is_same<T, int8_t>::value) {
			digLimit = 3+1;
		} else {
			endType = 'U';
			return *this;
		}
		//uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)
		pos = uartRecv(buff, 24);
		endType = uartRecvEndType;
		if (uartRecvEndType != 'e') return *this;
		if (pos>digLimit) {
			endType = 'B';
			return *this;
		}
		if (is_same<T, uint32_t>::value) {
			// uint32_t strToVal32(char* data, uint8_t maxChars=10)
			v = strToVal32(buff);
		} else if (is_same<T, uint16_t>::value) {
			v = strToVal16(buff);
		} else if (is_same<T, uint8_t>::value) {
			v = strToVal8(buff);
		} else if (is_same<T, int32_t>::value) {
			v = strToVal32i(buff);
		} else if (is_same<T, int16_t>::value) {
			v = strToVal16i(buff);
		} else if (is_same<T, int8_t>::value) {
			v = strToVal8i(buff);
		}

		return *this;
	}*/


	/*const uint8_t cNumBuffLen = 25;
	tCin& operator>>(uint32_t& v) {
		char buff[cNumBuffLen];
		//uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)
		pos = uartRecv(buff, cNumBuffLen-1, endChars);
		endType = uartRecvEndType;
		if (uartRecvEndType != 'e') return *this;
		if (pos>10) {
			endType = 'B';
			return *this;
		}
		// uint32_t strToVal32(char* data, uint8_t maxChars=10)
		v = strToVal32(buff);
		return *this;
	}*/
};

/*
struct tCin { // ===============================================================
	tCin& operator>>(char& c) {c = uartRecv();	return *this;}

	char* endChars=" \n";
	
	static const uint8_t cBuffLen = 25;
	char buff[cBuffLen];
	tCin& operator>>(uint32_t& v) {
		//uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)
		uint8_t n = uartRecv(buff, cBuffLen, endChars);
		if (uartRecvEndType != 'e') return *this;
		


		return *this;
	}

	// naèti "slovo" do bufferu (C-string) uživatel MUSÍ poskytnout buffer + velikost
	struct cstr_ref { char* buf; uint8_t siz; };
	static cstr_ref cstr(char* b, uint8_t siz) { return { b, siz }; }
	tCin& operator>>(cstr_ref dst) {
		if (!dst.buf || dst.siz == 0) return *this;

		//uint8_t uartRecv(char* data, uint8_t limit, char* endChars=" \n", bool wait0=true, uint8_t timeout=0)

		char c;
		c = uartRecv();
		// pøeskoè whitespace
		//do {
		//c = uartRecv();
		////putchar_uart(c);
		//} while (c == ' ' || c == '\r' || c == '\n' || c == '\t');

		uint8_t i = 0;
		while (c != ' ' && c != '\r' && c != '\n' && c != '\t') {
			if (i + 1 < dst.siz) dst.buf[i++] = c;  // nech místo na '\0'
			c = uartRecv();
			//putchar_uart(c);
		}

		dst.buf[i] = '\0';
		return *this;
	}
};*/

#ifndef COUTCIN_noStatic
	static tCin cin;
#endif






