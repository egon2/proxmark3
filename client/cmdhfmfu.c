//-----------------------------------------------------------------------------
// Ultralight Code (c) 2013,2014 Midnitesnake & Andy Davies of Pentura
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// High frequency MIFARE ULTRALIGHT (C) commands
//-----------------------------------------------------------------------------
#include "loclass/des.h"
#include "cmdhfmfu.h"
#include "cmdhfmf.h"
#include "cmdhf14a.h"
#include "mifare.h"
#include "util.h"
#include "protocols.h"
#include "data.h"

#define MAX_UL_BLOCKS     0x0f
#define MAX_ULC_BLOCKS    0x2b
#define MAX_ULEV1a_BLOCKS 0x13
#define MAX_ULEV1b_BLOCKS 0x28
#define MAX_NTAG_203      0x29
#define MAX_NTAG_210      0x13
#define MAX_NTAG_212      0x28
#define MAX_NTAG_213      0x2c
#define MAX_NTAG_215      0x86
#define MAX_NTAG_216      0xe6
#define MAX_MY_D_NFC       0xff
#define MAX_MY_D_MOVE      0x25
#define MAX_MY_D_MOVE_LEAN 0x0f

#define KEYS_3DES_COUNT 7
uint8_t default_3des_keys[KEYS_3DES_COUNT][16] = {
		{ 0x42,0x52,0x45,0x41,0x4b,0x4d,0x45,0x49,0x46,0x59,0x4f,0x55,0x43,0x41,0x4e,0x21 },// 3des std key
		{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },// all zeroes
		{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f },// 0x00-0x0F
		{ 0x49,0x45,0x4D,0x4B,0x41,0x45,0x52,0x42,0x21,0x4E,0x41,0x43,0x55,0x4F,0x59,0x46 },// NFC-key
		{ 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01 },// all ones
		{ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF },// all FF
		{ 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF }	// 11 22 33
};

#define KEYS_PWD_COUNT 1
uint8_t default_pwd_pack[KEYS_PWD_COUNT][4] = {
	{0xFF,0xFF,0xFF,0xFF}, // PACK 0x00,0x00 -- factory default
};

#define MAX_UL_TYPES 18
uint32_t UL_TYPES_ARRAY[MAX_UL_TYPES] = {
		UNKNOWN, UL, UL_C, 
		UL_EV1_48, UL_EV1_128, NTAG,
		NTAG_203, NTAG_210, NTAG_212,
		NTAG_213, NTAG_215, NTAG_216,
		MY_D, MY_D_NFC, MY_D_MOVE,
		MY_D_MOVE_NFC, MY_D_MOVE_LEAN, FUDAN_UL};

uint8_t UL_MEMORY_ARRAY[MAX_UL_TYPES] = {
		MAX_UL_BLOCKS, MAX_UL_BLOCKS, MAX_ULC_BLOCKS,
		MAX_ULEV1a_BLOCKS, MAX_ULEV1b_BLOCKS, MAX_NTAG_203,
		MAX_NTAG_203, MAX_NTAG_210, MAX_NTAG_212,
		MAX_NTAG_213, MAX_NTAG_215, MAX_NTAG_216, 
		MAX_UL_BLOCKS, MAX_MY_D_NFC, MAX_MY_D_MOVE,
		MAX_MY_D_MOVE, MAX_MY_D_MOVE_LEAN, MAX_UL_BLOCKS};

// Certain pwd generation algo nickname A.
uint32_t ul_ev1_pwdgenA(uint8_t* uid) { 

	uint8_t pos = (uid[3] ^ uid[4] ^ uid[5] ^ uid[6]) % 32;
	
	uint32_t xortable[] = {
						0x4f2711c1, 0x07D7BB83, 0x9636EF07, 0xB5F4460E, 0xF271141C, 0x7D7BB038, 0x636EF871, 0x5F4468E3,
						0x271149C7, 0xD7BB0B8F, 0x36EF8F1E, 0xF446863D, 0x7114947A, 0x7BB0B0F5, 0x6EF8F9EB, 0x44686BD7,
						0x11494fAF, 0xBB0B075F, 0xEF8F96BE, 0x4686B57C, 0x1494F2F9, 0xB0B07DF3, 0xF8F963E6, 0x686B5FCC,
						0x494F2799, 0x0B07D733, 0x8F963667, 0x86B5F4CE, 0x94F2719C, 0xB07D7B38, 0xF9636E70, 0x6B5F44E0
						};

	uint8_t entry[] = {0x00,0x00,0x00,0x00};
	uint8_t pwd[] = {0x00,0x00,0x00,0x00};
	
	num_to_bytes( xortable[pos], 4, entry);

	pwd[0] = entry[0] ^ uid[1] ^ uid[2] ^ uid[3];
	pwd[1] = entry[1] ^ uid[0] ^ uid[2] ^ uid[4];
	pwd[2] = entry[2] ^ uid[0] ^ uid[1] ^ uid[5];
	pwd[3] = entry[3] ^ uid[6];

	return (uint32_t)bytes_to_num(pwd, 4);
}

// Certain pwd generation algo nickname B. (very simple)
uint32_t ul_ev1_pwdgenB(uint8_t* uid) {

	uint8_t pwd[] = {0x00,0x00,0x00,0x00};
	
	pwd[0] = uid[1] ^ uid[3] ^ 0xAA;
	pwd[1] = uid[2] ^ uid[4] ^ 0x55;
	pwd[2] = uid[3] ^ uid[5] ^ 0xAA;
	pwd[3] = uid[4] ^ uid[6] ^ 0x55;
	return (uint32_t)bytes_to_num(pwd, 4);
}

// Certain pwd generation algo nickname C.
uint32_t ul_ev1_pwdgenC(uint8_t* uid){
	uint32_t pwd = 0;
	uint8_t base[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28,
		0x63, 0x29, 0x20, 0x43, 0x6f, 0x70, 0x79, 0x72,
		0x69, 0x67, 0x68, 0x74, 0x20, 0x4c, 0x45, 0x47,
		0x4f, 0x20, 0x32, 0x30, 0x31, 0x34, 0xaa, 0xaa
	};

	memcpy(base, uid, 7);

	for (int i = 0; i < 32; i += 4) {
		uint32_t b = *(uint32_t *)(base + i);
		pwd = b + ROTR(pwd, 25) + ROTR(pwd, 10) - pwd;
	}
	return BSWAP_32(pwd);
}

// pack generation for algo 1-3
uint16_t ul_ev1_packgenA(uint8_t* uid){
	uint16_t pack = (uid[0] ^ uid[1] ^ uid[2]) << 8 | (uid[2] ^ 8);
	return pack;
}
uint16_t ul_ev1_packgenB(uint8_t* uid){
	return 0x8080;
}
uint16_t ul_ev1_packgenC(uint8_t* uid){
	return 0xaa55;
}


void ul_ev1_pwdgen_selftest(){
	
	uint8_t uid1[] = {0x04,0x11,0x12,0x11,0x12,0x11,0x10};
	uint32_t pwd1 = ul_ev1_pwdgenA(uid1);
	PrintAndLog("UID | %s | %08X | %s", sprint_hex(uid1,7), pwd1, (pwd1 == 0x8432EB17)?"OK":"->8432EB17<-");

	uint8_t uid2[] = {0x04,0x1f,0x98,0xea,0x1e,0x3e,0x81};		
	uint32_t pwd2 = ul_ev1_pwdgenB(uid2);
	PrintAndLog("UID | %s | %08X | %s", sprint_hex(uid2,7), pwd2, (pwd2 == 0x5fd37eca)?"OK":"->5fd37eca<--");

	uint8_t uid3[] = {0x04,0x62, 0xB6, 0x8A, 0xB4, 0x42, 0x80};
	uint32_t pwd3 = ul_ev1_pwdgenC(uid3);
	PrintAndLog("UID | %s | %08X | %s", sprint_hex(uid3,7), pwd3, (pwd3 == 0x5a349515)?"OK":"->5a349515<--");
	return;
}

static int CmdHelp(const char *Cmd);

// get version nxp product type 
char *getProductTypeStr( uint8_t id){

	static char buf[20];
	char *retStr = buf;

	switch(id) {
		case 3: sprintf(retStr, "%02X, Ultralight", id); break;
		case 4:	sprintf(retStr, "%02X, NTAG", id); break;
		default: sprintf(retStr, "%02X, unknown", id); break;
	}
	return buf;
}

/*
  The 7 MSBits (=n) code the storage size itself based on 2^n, 
  the LSBit is set to '0' if the size is exactly 2^n
  and set to '1' if the storage size is between 2^n and 2^(n+1). 
*/
char *getUlev1CardSizeStr( uint8_t fsize ){

	static char buf[40];
	char *retStr = buf;
	memset(buf, 0, sizeof(buf));

	uint16_t usize = 1 << ((fsize >>1) + 1);
	uint16_t lsize = 1 << (fsize >>1);

	// is  LSB set?
	if (  fsize & 1 )
		sprintf(retStr, "%02X, (%u <-> %u bytes)",fsize, usize, lsize);
	else 
		sprintf(retStr, "%02X, (%u bytes)", fsize, lsize);		
	return buf;
}

static void ul_switch_on_field(void) {
	UsbCommand c = {CMD_READER_ISO_14443a, {ISO14A_CONNECT | ISO14A_NO_DISCONNECT, 0, 0}};
	clearCommandBuffer();
	SendCommand(&c);
}

void ul_switch_off_field(void) {
	UsbCommand c = {CMD_READER_ISO_14443a, {0, 0, 0}};
	clearCommandBuffer();
	SendCommand(&c);
}

static int ul_send_cmd_raw( uint8_t *cmd, uint8_t cmdlen, uint8_t *response, uint16_t responseLength ) {
	UsbCommand c = {CMD_READER_ISO_14443a, {ISO14A_RAW | ISO14A_NO_DISCONNECT | ISO14A_APPEND_CRC, cmdlen, 0}};
	memcpy(c.d.asBytes, cmd, cmdlen);
	clearCommandBuffer();
	SendCommand(&c);
	UsbCommand resp;
	if (!WaitForResponseTimeout(CMD_ACK, &resp, 1500)) return -1;
	if (!resp.arg[0] && responseLength) return -1;

	uint16_t resplen = (resp.arg[0] < responseLength) ? resp.arg[0] : responseLength;
	memcpy(response, resp.d.asBytes, resplen);
	return resplen;
}

static int ul_select( iso14a_card_select_t *card ){

	ul_switch_on_field();

	UsbCommand resp;
	bool ans = false;
	ans = WaitForResponseTimeout(CMD_ACK, &resp, 1500);
	if (!ans || resp.arg[0] < 1) {
		PrintAndLog("iso14443a card select failed");
		ul_switch_off_field();
		return 0;
	}

	memcpy(card, resp.d.asBytes, sizeof(iso14a_card_select_t));
	return 1;
}

// This read command will at least return 16bytes.
static int ul_read( uint8_t page, uint8_t *response, uint16_t responseLength ){

	uint8_t cmd[] = {ISO14443A_CMD_READBLOCK, page};
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), response, responseLength);
	return len;
}

static int ul_comp_write( uint8_t page, uint8_t *data, uint8_t datalen ){

	uint8_t cmd[18];
	memset(cmd, 0x00, sizeof(cmd));
	datalen = ( datalen > 16) ? 16 : datalen;

	cmd[0] = ISO14443A_CMD_WRITEBLOCK;
	cmd[1] = page;
	memcpy(cmd+2, data, datalen);

	uint8_t response[1] = {0xff};
	ul_send_cmd_raw(cmd, 2+datalen, response, sizeof(response));
	// ACK
	if ( response[0] == 0x0a ) return 0;
	// NACK
	return -1;
}

static int ulc_requestAuthentication( uint8_t *nonce, uint16_t nonceLength ){

	uint8_t cmd[] = {MIFARE_ULC_AUTH_1, 0x00};
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), nonce, nonceLength);
	return len;
}

static int ulc_authentication( uint8_t *key, bool switch_off_field ){

	UsbCommand c = {CMD_MIFAREUC_AUTH, {switch_off_field}};
	memcpy(c.d.asBytes, key, 16);
	clearCommandBuffer();
	SendCommand(&c);
	UsbCommand resp;
	if ( !WaitForResponseTimeout(CMD_ACK, &resp, 1500) ) return 0;
	if ( resp.arg[0] == 1 ) return 1;

	return 0;
}

static int ulev1_requestAuthentication( uint8_t *pwd, uint8_t *pack, uint16_t packLength ){

	uint8_t cmd[] = {MIFARE_ULEV1_AUTH, pwd[0], pwd[1], pwd[2], pwd[3]};
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), pack, packLength);
	return len;
}

static int ul_auth_select( iso14a_card_select_t *card, TagTypeUL_t tagtype, bool hasAuthKey, uint8_t *authenticationkey, uint8_t *pack, uint8_t packSize){
	if ( hasAuthKey && (tagtype & UL_C)) {
		//will select card automatically and close connection on error
		if (!ulc_authentication(authenticationkey, false)) {
			PrintAndLog("Error: Authentication Failed UL-C");
			return 0;
		}
	} else {
		if ( !ul_select(card) ) return 0;

		if (hasAuthKey) {
			if (ulev1_requestAuthentication(authenticationkey, pack, packSize) < 2) {
				ul_switch_off_field();
				PrintAndLog("Error: Authentication Failed UL-EV1/NTAG");
				return 0;
			}
		}
	}
	return 1;
}

static int ulev1_getVersion( uint8_t *response, uint16_t responseLength ){

	uint8_t cmd[] = {MIFARE_ULEV1_VERSION};	
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), response, responseLength);
	return len;
}

static int ulev1_readCounter( uint8_t counter, uint8_t *response, uint16_t responseLength ){

	uint8_t cmd[] = {MIFARE_ULEV1_READ_CNT, counter};
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), response, responseLength);
	return len;
}

static int ulev1_readTearing( uint8_t counter, uint8_t *response, uint16_t responseLength ){

	uint8_t cmd[] = {MIFARE_ULEV1_CHECKTEAR, counter};
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), response, responseLength);
	return len;
}

static int ulev1_readSignature( uint8_t *response, uint16_t responseLength ){

	uint8_t cmd[] = {MIFARE_ULEV1_READSIG, 0x00};
	int len = ul_send_cmd_raw(cmd, sizeof(cmd), response, responseLength);
	return len;
}

// Fudan check checks for which error is given for a command with incorrect crc
// NXP UL chip responds with 01, fudan 00.
// other possible checks:
//  send a0 + crc 
//  UL responds with 00, fudan doesn't respond
//  or
//  send a200 + crc
//  UL doesn't respond, fudan responds with 00
//  or
//  send 300000 + crc (read with extra byte(s))
//  UL responds with read of page 0, fudan doesn't respond.
//
// make sure field is off before calling this function
static int ul_fudan_check( void ){
	iso14a_card_select_t card;
	if ( !ul_select(&card) ) 
		return UL_ERROR;

	UsbCommand c = {CMD_READER_ISO_14443a, {ISO14A_RAW | ISO14A_NO_DISCONNECT, 4, 0}};

	uint8_t cmd[4] = {0x30,0x00,0x02,0xa7}; //wrong crc on purpose  should be 0xa8
	memcpy(c.d.asBytes, cmd, 4);
	clearCommandBuffer();
	SendCommand(&c);
	UsbCommand resp;
	if (!WaitForResponseTimeout(CMD_ACK, &resp, 1500)) return UL_ERROR;
	if (resp.arg[0] != 1) return UL_ERROR;

	return (!resp.d.asBytes[0]) ? FUDAN_UL : UL; //if response == 0x00 then Fudan, else Genuine NXP
}

static int ul_print_default( uint8_t *data){

	uint8_t uid[7];
	uid[0] = data[0];
	uid[1] = data[1];
	uid[2] = data[2];
	uid[3] = data[4];
	uid[4] = data[5];
	uid[5] = data[6];
	uid[6] = data[7];

	PrintAndLog("       UID : %s ", sprint_hex(uid, 7));
	PrintAndLog("    UID[0] : %02X, %s",  uid[0], getTagInfo(uid[0]) );
	if ( uid[0] == 0x05 && ((uid[1] & 0xf0) >> 4) == 2 ) { // is infineon and 66RxxP
		uint8_t chip = (data[8] & 0xC7); // 11000111  mask, bit 3,4,5 RFU
		switch (chip){
			case 0xc2: PrintAndLog("   IC type : SLE 66R04P 770 Bytes"); break; //77 pages
			case 0xc4: PrintAndLog("   IC type : SLE 66R16P 2560 Bytes"); break; //256 pages
			case 0xc6: PrintAndLog("   IC type : SLE 66R32P 5120 Bytes"); break; //512 pages /2 sectors
		}
	}
	// CT (cascade tag byte) 0x88 xor SN0 xor SN1 xor SN2 
	int crc0 = 0x88 ^ data[0] ^ data[1] ^data[2];
	if ( data[3] == crc0 )
		PrintAndLog("      BCC0 : %02X, Ok", data[3]);
	else
		PrintAndLog("      BCC0 : %02X, crc should be %02X", data[3], crc0);

	int crc1 = data[4] ^ data[5] ^ data[6] ^data[7];
	if ( data[8] == crc1 )
		PrintAndLog("      BCC1 : %02X, Ok", data[8]);
	else
		PrintAndLog("      BCC1 : %02X, crc should be %02X", data[8], crc1 );

	PrintAndLog("  Internal : %02X, %sdefault", data[9], (data[9]==0x48)?"":"not " );

	PrintAndLog("      Lock : %s - %s",
				sprint_hex(data+10, 2),
				sprint_bin(data+10, 2)
		);

	PrintAndLog("OneTimePad : %s - %s\n",
				sprint_hex(data + 12, 4),
				sprint_bin(data+12, 4)
		);

	return 0;
}

static int ndef_print_CC(uint8_t *data) {
	// no NDEF message
	if(data[0] != 0xe1)
		return -1;

	PrintAndLog("--- NDEF Message");
	PrintAndLog("Capability Container: %s", sprint_hex(data,4) );
	PrintAndLog("  %02X : NDEF Magic Number", data[0]); 
	PrintAndLog("  %02X : version %d.%d supported by tag", data[1], (data[1] & 0xF0) >> 4, data[1] & 0x0f);
	PrintAndLog("  %02X : Physical Memory Size: %d bytes", data[2], (data[2] + 1) * 8);
	if ( data[2] == 0x96 )
		PrintAndLog("  %02X : NDEF Memory Size: %d bytes", data[2], 48);
	else if ( data[2] == 0x12 )
		PrintAndLog("  %02X : NDEF Memory Size: %d bytes", data[2], 144);
	else if ( data[2] == 0x3e )
		PrintAndLog("  %02X : NDEF Memory Size: %d bytes", data[2], 496);
	else if ( data[2] == 0x6d )
		PrintAndLog("  %02X : NDEF Memory Size: %d bytes", data[2], 872);

	PrintAndLog("  %02X : %s / %s", data[3], 
				(data[3] & 0xF0) ? "(RFU)" : "Read access granted without any security", 
				(data[3] & 0x0F)==0 ? "Write access granted without any security" : (data[3] & 0x0F)==0x0F ? "No write access granted at all" : "(RFU)");
	return 0;
}

int ul_print_type(uint32_t tagtype, uint8_t spaces){
	char spc[11] = "          ";
	spc[10]=0x00;
	char *spacer = spc + (10-spaces);

	if ( tagtype & UL )	
		PrintAndLog("%sTYPE : MIFARE Ultralight (MF0ICU1) %s", spacer, (tagtype & MAGIC) ? "<magic>" : "" );
	else if ( tagtype & UL_C)
		PrintAndLog("%sTYPE : MIFARE Ultralight C (MF0ULC) %s", spacer, (tagtype & MAGIC) ? "<magic>" : "" );
	else if ( tagtype & UL_EV1_48)
		PrintAndLog("%sTYPE : MIFARE Ultralight EV1 48bytes (MF0UL1101)", spacer); 
	else if ( tagtype & UL_EV1_128)	
		PrintAndLog("%sTYPE : MIFARE Ultralight EV1 128bytes (MF0UL2101)", spacer);
	else if ( tagtype & NTAG )
		PrintAndLog("%sTYPE : NTAG UNKNOWN", spacer);
	else if ( tagtype & NTAG_203 )
		PrintAndLog("%sTYPE : NTAG 203 144bytes (NT2H0301F0DT)", spacer);
	else if ( tagtype & NTAG_210 )
		PrintAndLog("%sTYPE : NTAG 210 48bytes (NT2L1011G0DU)", spacer);
	else if ( tagtype & NTAG_212 )
		PrintAndLog("%sTYPE : NTAG 212 128bytes (NT2L1211G0DU)", spacer);
	else if ( tagtype & NTAG_213 )
		PrintAndLog("%sTYPE : NTAG 213 144bytes (NT2H1311G0DU)", spacer);
	else if ( tagtype & NTAG_215 )
		PrintAndLog("%sTYPE : NTAG 215 504bytes (NT2H1511G0DU)", spacer);
	else if ( tagtype & NTAG_216 )
		PrintAndLog("%sTYPE : NTAG 216 888bytes (NT2H1611G0DU)", spacer);
	else if ( tagtype & NTAG_I2C_1K )
		PrintAndLog("%sTYPE : NTAG I%sC 888bytes (NT3H1101FHK)", spacer, "\xFD");
	else if ( tagtype & NTAG_I2C_2K )	
		PrintAndLog("%sTYPE : NTAG I%sC 1904bytes (NT3H1201FHK)", spacer, "\xFD");
	else if ( tagtype & MY_D )
		PrintAndLog("%sTYPE : INFINEON my-d\x99 (SLE 66RxxS)", spacer);
	else if ( tagtype & MY_D_NFC )
		PrintAndLog("%sTYPE : INFINEON my-d\x99 NFC (SLE 66RxxP)", spacer);
	else if ( tagtype & MY_D_MOVE )
		PrintAndLog("%sTYPE : INFINEON my-d\x99 move (SLE 66R01P)", spacer);
	else if ( tagtype & MY_D_MOVE_NFC )
		PrintAndLog("%sTYPE : INFINEON my-d\x99 move NFC (SLE 66R01P)", spacer);
	else if ( tagtype & MY_D_MOVE_LEAN )
		PrintAndLog("%sTYPE : INFINEON my-d\x99 move lean (SLE 66R01L)", spacer);
	else if ( tagtype & FUDAN_UL )
		PrintAndLog("%sTYPE : FUDAN Ultralight Compatible (or other compatible) %s", spacer, (tagtype & MAGIC) ? "<magic>" : "" );
	else
		PrintAndLog("%sTYPE : Unknown %06x", spacer, tagtype);
	return 0;
}

static int ulc_print_3deskey( uint8_t *data){
	PrintAndLog("         deskey1 [44/0x2C] : %s [s]", sprint_hex(data   ,4), sprint_ascii(data,4) );
	PrintAndLog("         deskey1 [45/0x2D] : %s [s]", sprint_hex(data+4 ,4), sprint_ascii(data+4,4));
	PrintAndLog("         deskey2 [46/0x2E] : %s [s]", sprint_hex(data+8 ,4), sprint_ascii(data+8,4));
	PrintAndLog("         deskey2 [47/0x2F] : %s [s]", sprint_hex(data+12,4), sprint_ascii(data+12,4));
	PrintAndLog("\n 3des key : %s", sprint_hex(SwapEndian64(data, 16, 8), 16));
	return 0;
}

static int ulc_print_configuration( uint8_t *data){

	PrintAndLog("--- UL-C Configuration");
	PrintAndLog(" Higher Lockbits [40/0x28] : %s - %s", sprint_hex(data, 4), sprint_bin(data, 2));
	PrintAndLog("         Counter [41/0x29] : %s - %s", sprint_hex(data+4, 4), sprint_bin(data+4, 2));

	bool validAuth = (data[8] >= 0x03 && data[8] <= 0x30);
	if ( validAuth )
		PrintAndLog("           Auth0 [42/0x2A] : %s page %d/0x%02X and above need authentication", sprint_hex(data+8, 4), data[8],data[8] );
	else{
		if ( data[8] == 0){
			PrintAndLog("           Auth0 [42/0x2A] : %s default", sprint_hex(data+8, 4) );
		} else {
			PrintAndLog("           Auth0 [42/0x2A] : %s auth byte is out-of-range", sprint_hex(data+8, 4) );
		}
	}
	PrintAndLog("           Auth1 [43/0x2B] : %s %s",
			sprint_hex(data+12, 4),
			(data[12] & 1) ? "write access restricted": "read and write access restricted"
			);
	return 0;
}

static int ulev1_print_configuration( uint8_t *data, uint8_t startPage){

	PrintAndLog("\n--- Tag Configuration");

	bool strg_mod_en = (data[0] & 2);
	uint8_t authlim = (data[4] & 0x07);
	bool nfc_cnf_en = (data[4] & 0x08);
	bool nfc_cnf_prot_pwd = (data[4] & 0x10);
	bool cfglck = (data[4] & 0x40);
	bool prot = (data[4] & 0x80);
	uint8_t vctid = data[5];

	PrintAndLog("  cfg0 [%u/0x%02X] : %s", startPage, startPage, sprint_hex(data, 4));
	if ( data[3] < 0xff )
		PrintAndLog("                    - page %d and above need authentication",data[3]);
	else 
		PrintAndLog("                    - pages don't need authentication");
	PrintAndLog("                    - strong modulation mode %s", (strg_mod_en) ? "enabled":"disabled");
	PrintAndLog("  cfg1 [%u/0x%02X] : %s", startPage + 1, startPage + 1,  sprint_hex(data+4, 4) );
	if ( authlim == 0)
		PrintAndLog("                    - Unlimited password attempts");
	else
		PrintAndLog("                    - Max number of password attempts is %d", authlim);
	
	PrintAndLog("                    - NFC counter %s", (nfc_cnf_en) ? "enabled":"disabled");
	PrintAndLog("                    - NFC counter %s", (nfc_cnf_prot_pwd) ? "not protected":"password protection enabled");
	
	PrintAndLog("                    - user configuration %s", cfglck ? "permanently locked":"writeable");
	PrintAndLog("                    - %s access is protected with password", prot ? "read and write":"write");
	PrintAndLog("                    - %02X, Virtual Card Type Identifier is %s default", vctid, (vctid==0x05)? "":"not");
	PrintAndLog("  PWD  [%u/0x%02X] : %s- (cannot be read)", startPage + 2, startPage + 2,  sprint_hex(data+8, 4));
	PrintAndLog("  PACK [%u/0x%02X] : %s      - (cannot be read)", startPage + 3, startPage + 3,  sprint_hex(data+12, 2));
	PrintAndLog("  RFU  [%u/0x%02X] :       %s- (cannot be read)", startPage + 3, startPage + 3,  sprint_hex(data+12, 2));
	return 0;
}

static int ulev1_print_counters(){
	PrintAndLog("--- Tag Counters");
	uint8_t tear[1] = {0};
	uint8_t counter[3] = {0,0,0};
	uint16_t len = 0;
	for ( uint8_t i = 0; i<3; ++i) {
		ulev1_readTearing(i,tear,sizeof(tear));
		len = ulev1_readCounter(i,counter, sizeof(counter) );
		if (len == 3) {
			PrintAndLog("       [%0d] : %s", i, sprint_hex(counter,3));
			PrintAndLog("                    - %02X tearing %s", tear[0], ( tear[0]==0xBD)?"Ok":"failure");
		}
	}
	return len;
}

static int ulev1_print_signature( uint8_t *data, uint8_t len){
	PrintAndLog("\n--- Tag Signature");	
	//PrintAndLog("IC signature public key name  : NXP NTAG21x 2013"); // don't know if there is other NXP public keys.. :(
	PrintAndLog("IC signature public key value : 04494e1a386d3d3cfe3dc10e5de68a499b1c202db5b132393e89ed19fe5be8bc61");
	PrintAndLog("    Elliptic curve parameters : secp128r1");
	PrintAndLog("            Tag ECC Signature : %s", sprint_hex(data, len));
	//to do:  verify if signature is valid
	//PrintAndLog("IC signature status: %s valid", (iseccvalid() )?"":"not");
	return 0;
}

static int ulev1_print_version(uint8_t *data){
	PrintAndLog("\n--- Tag Version");
	PrintAndLog("       Raw bytes : %s",sprint_hex(data, 8) );
	PrintAndLog("       Vendor ID : %02X, %s", data[1], getTagInfo(data[1]));
	PrintAndLog("    Product type : %s", getProductTypeStr(data[2]));
	PrintAndLog(" Product subtype : %02X, %s", data[3], (data[3]==1) ?"17 pF":"50pF");
	PrintAndLog("   Major version : %02X", data[4]);
	PrintAndLog("   Minor version : %02X", data[5]);
	PrintAndLog("            Size : %s", getUlev1CardSizeStr(data[6]));
	PrintAndLog("   Protocol type : %02X", data[7]);
	return 0;
}

/*
static int ulc_magic_test(){
	// Magic Ultralight test
		// Magic UL-C, by observation,
	// 1) it seems to have a static nonce response to 0x1A command.
	// 2) the deskey bytes is not-zero:d out on as datasheet states.
	// 3) UID - changeable, not only, but pages 0-1-2-3.
	// 4) use the ul_magic_test !  magic tags answers specially!
	int returnValue = UL_ERROR;
	iso14a_card_select_t card;
	uint8_t nonce1[11] = {0x00};
	uint8_t nonce2[11] = {0x00};
	int status = ul_select(&card);
	if ( !status ){
		return UL_ERROR;
	}
	status = ulc_requestAuthentication(nonce1, sizeof(nonce1));
	if ( status > 0 ) {
		status = ulc_requestAuthentication(nonce2, sizeof(nonce2));
		returnValue =  ( !memcmp(nonce1, nonce2, 11) ) ? UL_C_MAGIC : UL_C;
	} else {
		returnValue = UL;
	}	
	ul_switch_off_field();
	return returnValue;
}
*/
static int ul_magic_test(){

	// Magic Ultralight tests
	// 1) take present UID, and try to write it back. OBSOLETE 
	// 2) make a wrong length write to page0, and see if tag answers with ACK/NACK:
	iso14a_card_select_t card;
	if ( !ul_select(&card) ) 
		return UL_ERROR;
	int status = ul_comp_write(0, NULL, 0);
	ul_switch_off_field();
	if ( status == 0 ) 
		return MAGIC;
	return 0;
}

uint32_t GetHF14AMfU_Type(void){

	TagTypeUL_t tagtype = UNKNOWN;
	iso14a_card_select_t card;
	uint8_t version[10] = {0x00};
	int status = 0;
	int len;

	if (!ul_select(&card)) return UL_ERROR;

	// Ultralight - ATQA / SAK 
	if ( card.atqa[1] != 0x00 || card.atqa[0] != 0x44 || card.sak != 0x00 ) {
		//PrintAndLog("Tag is not Ultralight | NTAG | MY-D  [ATQA: %02X %02X SAK: %02X]\n", card.atqa[1], card.atqa[0], card.sak);
		ul_switch_off_field();
		return UL_ERROR;
	}

	if ( card.uid[0] != 0x05) {

		len  = ulev1_getVersion(version, sizeof(version));
		ul_switch_off_field();

		switch (len) {
			case 0x0A: {

				if ( version[2] == 0x03 && version[6] == 0x0B )
					tagtype = UL_EV1_48;
				else if ( version[2] == 0x03 && version[6] != 0x0B )
					tagtype = UL_EV1_128;
				else if ( version[2] == 0x04 && version[3] == 0x01 && version[6] == 0x0B )
					tagtype = NTAG_210;
				else if ( version[2] == 0x04 && version[3] == 0x01 && version[6] == 0x0E )
					tagtype = NTAG_212;
				else if ( version[2] == 0x04 && version[3] == 0x02 && version[6] == 0x0F )
					tagtype = NTAG_213;
				else if ( version[2] == 0x04 && version[3] == 0x02 && version[6] == 0x11 )
					tagtype = NTAG_215;
				else if ( version[2] == 0x04 && version[3] == 0x02 && version[6] == 0x13 )
					tagtype = NTAG_216;
				else if ( version[2] == 0x04 && version[3] == 0x05 && version[6] == 0x13 )
					tagtype = NTAG_I2C_1K;
				else if ( version[2] == 0x04 && version[3] == 0x05 && version[6] == 0x15 )
					tagtype = NTAG_I2C_2K;
				else if ( version[2] == 0x04 )
					tagtype = NTAG;

				break;
			}
			case 0x01: tagtype = UL_C; break;
			case 0x00: tagtype = UL; break;
			case -1  : tagtype = (UL | UL_C | NTAG_203); break;  // could be UL | UL_C magic tags
			default  : tagtype = UNKNOWN; break;
		}
		// UL vs UL-C vs ntag203 test
		if (tagtype & (UL | UL_C | NTAG_203)) {
			if ( !ul_select(&card) ) return UL_ERROR;

			// do UL_C check first...
			uint8_t nonce[11] = {0x00};
			status = ulc_requestAuthentication(nonce, sizeof(nonce));
			ul_switch_off_field();
			if (status > 1) {
				tagtype = UL_C;
			} else { 
				// need to re-select after authentication error
				if ( !ul_select(&card) ) return UL_ERROR;

				uint8_t data[16] = {0x00};
				// read page 0x26-0x29 (last valid ntag203 page)
				status = ul_read(0x26, data, sizeof(data));
				if ( status <= 1 ) {
					tagtype = UL;
				} else {
					// read page 0x30 (should error if it is a ntag203)
					status = ul_read(0x30, data, sizeof(data));
					if ( status <= 1 ){
						tagtype = NTAG_203;
					} else {
						tagtype = UNKNOWN;
					}
				}
				ul_switch_off_field();
			}
		}
		if (tagtype & UL) {
			tagtype = ul_fudan_check(); 
			ul_switch_off_field();
		}
	} else {
		ul_switch_off_field();
		// Infinition MY-D tests   Exam high nibble 
		uint8_t nib = (card.uid[1] & 0xf0) >> 4;
		switch ( nib ){
			// case 0: tagtype =  SLE66R35E7; break; //or SLE 66R35E7 - mifare compat... should have different sak/atqa for mf 1k
			case 1:	tagtype =  MY_D; break; //or SLE 66RxxS ... up to 512 pages of 8 user bytes...
			case 2:	tagtype = (MY_D_NFC); break; //or SLE 66RxxP ... up to 512 pages of 8 user bytes... (or in nfc mode FF pages of 4 bytes)
			case 3:	tagtype = (MY_D_MOVE | MY_D_MOVE_NFC); break; //or SLE 66R01P // 38 pages of 4 bytes //notice: we can not currently distinguish between these two
			case 7: tagtype =  MY_D_MOVE_LEAN; break; //or SLE 66R01L  // 16 pages of 4 bytes
		}
	}

	tagtype |= ul_magic_test();
	if (tagtype == (UNKNOWN | MAGIC)) tagtype = (UL_MAGIC);
	return tagtype;
}

int CmdHF14AMfUInfo(const char *Cmd){

	uint8_t authlim = 0xff;
	uint8_t data[16] = {0x00};
	iso14a_card_select_t card;
	int status;
	bool errors = false;
	bool hasAuthKey = false;
	bool locked = false;
	bool swapEndian = false;
	uint8_t cmdp = 0;
	uint8_t dataLen = 0;
	uint8_t authenticationkey[16] = {0x00};
	uint8_t *authkeyptr = authenticationkey;
	uint8_t pwd[4] = {0,0,0,0};
	uint8_t	*key = pwd;
	uint8_t pack[4] = {0,0,0,0};
	int len = 0;
	char tempStr[50];

	while(param_getchar(Cmd, cmdp) != 0x00)
	{
		switch(param_getchar(Cmd, cmdp))
		{
		case 'h':
		case 'H':
			return usage_hf_mfu_info();
		case 'k':
		case 'K':
			dataLen = param_getstr(Cmd, cmdp+1, tempStr);
			if (dataLen == 32 || dataLen == 8) { //ul-c or ev1/ntag key length
				errors = param_gethex(tempStr, 0, authenticationkey, dataLen);
				dataLen /= 2; // handled as bytes from now on
			} else {
				PrintAndLog("\nERROR: Key is incorrect length\n");
				errors = true;
			}
			cmdp += 2;
			hasAuthKey = true;
			break;
		case 'l':
		case 'L':
			swapEndian = true;
			cmdp++;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = true;
			break;
		}
		if(errors) break;
	}

	//Validations
	if(errors) return usage_hf_mfu_info();

	TagTypeUL_t tagtype = GetHF14AMfU_Type();
	if (tagtype == UL_ERROR) return -1;

	PrintAndLog("\n--- Tag Information ---------");
	PrintAndLog("-------------------------------------------------------------");
	ul_print_type(tagtype, 6);

	// Swap endianness 
	if (swapEndian && hasAuthKey) authkeyptr = SwapEndian64(authenticationkey, dataLen, (dataLen == 16) ? 8 : 4 );

	if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;

	// read pages 0,1,2,3 (should read 4pages)
	status = ul_read(0, data, sizeof(data));
	if ( status == -1 ) {
		ul_switch_off_field();
		PrintAndLog("Error: tag didn't answer to READ");
		return status;
	} else if (status == 16) {
		ul_print_default(data);
		ndef_print_CC(data+12);
	} else {
		locked = true;
	}

	// UL_C Specific
	if ((tagtype & UL_C)) {

		// read pages 0x28, 0x29, 0x2A, 0x2B
		uint8_t ulc_conf[16] = {0x00};
		status = ul_read(0x28, ulc_conf, sizeof(ulc_conf));
		if ( status == -1 ){
			PrintAndLog("Error: tag didn't answer to READ UL-C");
			ul_switch_off_field();
			return status;
		} 
		if (status == 16) ulc_print_configuration(ulc_conf);
		else locked = true;

		if ((tagtype & MAGIC)) {
			//just read key
			uint8_t ulc_deskey[16] = {0x00};
			status = ul_read(0x2C, ulc_deskey, sizeof(ulc_deskey));
			if ( status == -1 ) {
				ul_switch_off_field();
				PrintAndLog("Error: tag didn't answer to READ magic");
				return status;
			}
			if (status == 16) ulc_print_3deskey(ulc_deskey);

		} else {
			ul_switch_off_field();
			// if we called info with key, just return 
			if ( hasAuthKey ) return 1;

			// also try to diversify default keys..  look into CmdHF14AMfuGenDiverseKeys
			PrintAndLog("Trying some default 3des keys");
			for (uint8_t i = 0; i < KEYS_3DES_COUNT; ++i ) {
				key = default_3des_keys[i];
				if (ulc_authentication(key, true)) {
					PrintAndLog("Found default 3des key: ");
					uint8_t keySwap[16];
					memcpy(keySwap, SwapEndian64(key,16,8), 16);
					ulc_print_3deskey(keySwap);
					return 1;
				} 
			}
			return 1;
		}
	}

	// do counters and signature first (don't neet auth) 

	// ul counters are different than ntag counters
	if ((tagtype & (UL_EV1_48 | UL_EV1_128))) {
		if (ulev1_print_counters() != 3) {
			// failed - re-select
			if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;
		}
	}

	// Read signature
	if ((tagtype & (UL_EV1_48 | UL_EV1_128 | NTAG_213 | NTAG_215 | NTAG_216 | NTAG_I2C_1K | NTAG_I2C_2K	))) {
		uint8_t ulev1_signature[32] = {0x00};
		status = ulev1_readSignature( ulev1_signature, sizeof(ulev1_signature));
		if ( status == -1 ) {
			PrintAndLog("Error: tag didn't answer to READ SIGNATURE");
			ul_switch_off_field();
			return status;
		}
		if (status == 32) ulev1_print_signature( ulev1_signature, sizeof(ulev1_signature));
		else {
			// re-select
			if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;
		}
	}

	// Get Version
	if ((tagtype & (UL_EV1_48 | UL_EV1_128 | NTAG_210 | NTAG_212 | NTAG_213 | NTAG_215 | NTAG_216 | NTAG_I2C_1K | NTAG_I2C_2K))) {
		uint8_t version[10] = {0x00};
		status  = ulev1_getVersion(version, sizeof(version));
		if ( status == -1 ) {
			PrintAndLog("Error: tag didn't answer to GETVERSION");
			ul_switch_off_field();
			return status;
		} else if (status == 10) {
			ulev1_print_version(version);
		} else {
			locked = true;
			if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;
		}

		uint8_t startconfigblock = 0;
		uint8_t ulev1_conf[16] = {0x00};
		// config blocks always are last 4 pages
		for (uint8_t idx = 0; idx < MAX_UL_TYPES; idx++)
			if (tagtype & UL_TYPES_ARRAY[idx])
				startconfigblock = UL_MEMORY_ARRAY[idx]-3;

		if (startconfigblock){ // if we know where the config block is...
			status = ul_read(startconfigblock, ulev1_conf, sizeof(ulev1_conf));
			if ( status == -1 ) {
				PrintAndLog("Error: tag didn't answer to READ EV1");
				ul_switch_off_field();
				return status;
			} else if (status == 16) {
				// save AUTHENTICATION LIMITS for later:
				authlim = (ulev1_conf[4] & 0x07);
				ulev1_print_configuration(ulev1_conf, startconfigblock);
			}
		}

		// AUTHLIMIT, (number of failed authentications)
		// 0 = limitless.
		// 1-7 = limit. No automatic tries then.
		// hasAuthKey,  if we was called with key, skip test.
		if ( !authlim && !hasAuthKey ) {
			PrintAndLog("\n--- Known EV1/NTAG passwords.");
			len = 0;
			
			// test pwd gen A
			num_to_bytes( ul_ev1_pwdgenA(card.uid), 4, key);
			len = ulev1_requestAuthentication(key, pack, sizeof(pack));
			if (len >= 1) {
				PrintAndLog("Found a default password: %s || Pack: %02X %02X",sprint_hex(key, 4), pack[0], pack[1]);
			}
			if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;
			
			// test pwd gen B
			num_to_bytes( ul_ev1_pwdgenB(card.uid), 4, key);
			len = ulev1_requestAuthentication(key, pack, sizeof(pack));
			if (len >= 1) {
				PrintAndLog("Found a default password: %s || Pack: %02X %02X",sprint_hex(key, 4), pack[0], pack[1]);
			}
			if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;

			// test pwd gen C
			num_to_bytes( ul_ev1_pwdgenC(card.uid), 4, key);
			len = ulev1_requestAuthentication(key, pack, sizeof(pack));
			if (len >= 1) {
				PrintAndLog("Found a default password: %s || Pack: %02X %02X",sprint_hex(key, 4), pack[0], pack[1]);
			}
			if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;			
			
			for (uint8_t i = 0; i < KEYS_PWD_COUNT; ++i ) {
				key = default_pwd_pack[i];
				len = ulev1_requestAuthentication(key, pack, sizeof(pack));
				if (len >= 1) {
					PrintAndLog("Found a default password: %s || Pack: %02X %02X",sprint_hex(key, 4), pack[0], pack[1]);
					break;
				} else {
					if (!ul_auth_select( &card, tagtype, hasAuthKey, authkeyptr, pack, sizeof(pack))) return -1;
				}
			}
			if (len < 1) PrintAndLog("password not known");
		}
	}

	ul_switch_off_field();
	if (locked) PrintAndLog("\nTag appears to be locked, try using the key to get more info");
	PrintAndLog("");
	return 1;
}

//
//  Write Single Block
//
int CmdHF14AMfUWrBl(const char *Cmd){

	int blockNo = -1;	
	bool errors = false;
	bool hasAuthKey = false;
	bool hasPwdKey = false;
	bool swapEndian = false;

	uint8_t cmdp = 0;
	uint8_t keylen = 0;
	uint8_t blockdata[20] = {0x00};
	uint8_t data[16] = {0x00};
	uint8_t authenticationkey[16] = {0x00};
	uint8_t *authKeyPtr = authenticationkey;

	while(param_getchar(Cmd, cmdp) != 0x00)
	{
		switch(param_getchar(Cmd, cmdp))
		{
			case 'h':
			case 'H':
				return usage_hf_mfu_wrbl();
			case 'k':
			case 'K':
				// EV1/NTAG size key
				keylen = param_gethex(Cmd, cmdp+1, data, 8);
				if ( !keylen ) {
					memcpy(authenticationkey, data, 4);
					cmdp += 2;
					hasPwdKey = true;
					break;
				}
				// UL-C size key	
				keylen = param_gethex(Cmd, cmdp+1, data, 32);
				if (!keylen){
					memcpy(authenticationkey, data, 16);
					cmdp += 2;
					hasAuthKey = true;
					break;
				}
				PrintAndLog("\nERROR: Key is incorrect length\n");
				errors = true; 
				break;
			case 'b':
			case 'B':
				blockNo = param_get8(Cmd, cmdp+1);
				if (blockNo < 0) {
					PrintAndLog("Wrong block number");
					errors = true;									
				}
				cmdp += 2;
				break;
			case 'l':
			case 'L':
				swapEndian = true;
				cmdp++;	
				break;
			case 'd':
			case 'D':
				if ( param_gethex(Cmd, cmdp+1, blockdata, 8) ) {
					PrintAndLog("Block data must include 8 HEX symbols");
					errors = true;
					break;
				}
				cmdp += 2;
				break;
			default:
				PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
				errors = true;
				break;
		}
		//Validations
		if(errors) return usage_hf_mfu_wrbl();
	}

	if ( blockNo == -1 ) return usage_hf_mfu_wrbl();
	// starting with getting tagtype
	TagTypeUL_t tagtype = GetHF14AMfU_Type();
	if (tagtype == UL_ERROR) return -1;

	uint8_t maxblockno = 0;
	for (uint8_t idx = 0; idx < MAX_UL_TYPES; idx++){
		if (tagtype & UL_TYPES_ARRAY[idx])
			maxblockno = UL_MEMORY_ARRAY[idx];
	}
	if (blockNo > maxblockno){
		PrintAndLog("block number too large. Max block is %u/0x%02X \n", maxblockno,maxblockno);
		return usage_hf_mfu_wrbl();
	}

	// Swap endianness 
	if (swapEndian && hasAuthKey) authKeyPtr = SwapEndian64(authenticationkey, 16, 8);
	if (swapEndian && hasPwdKey)  authKeyPtr = SwapEndian64(authenticationkey, 4, 4);

	if ( blockNo <= 3) 		
		PrintAndLog("Special Block: %0d (0x%02X) [ %s]", blockNo, blockNo, sprint_hex(blockdata, 4));
	else
		PrintAndLog("Block: %0d (0x%02X) [ %s]", blockNo, blockNo, sprint_hex(blockdata, 4));
	
	//Send write Block
	UsbCommand c = {CMD_MIFAREU_WRITEBL, {blockNo}};
	memcpy(c.d.asBytes,blockdata,4);

	if ( hasAuthKey ){
		c.arg[1] = 1;
		memcpy(c.d.asBytes+4,authKeyPtr,16);
	}
	else if ( hasPwdKey ) {
		c.arg[1] = 2;
		memcpy(c.d.asBytes+4,authKeyPtr,4);
	}
	
	clearCommandBuffer();
	SendCommand(&c);
	UsbCommand resp;
	if (WaitForResponseTimeout(CMD_ACK,&resp,1500)) {
		uint8_t isOK  = resp.arg[0] & 0xff;
		PrintAndLog("isOk:%02x", isOK);
	} else {
		PrintAndLog("Command execute timeout");
	}
	
	return 0;
}
//
//  Read Single Block
//
int CmdHF14AMfURdBl(const char *Cmd){

	int blockNo = -1;	
	bool errors = false;
	bool hasAuthKey = false;
	bool hasPwdKey = false;
	bool swapEndian = false;
	uint8_t cmdp = 0;
	uint8_t keylen = 0;
	uint8_t data[16] = {0x00};
	uint8_t authenticationkey[16] = {0x00};
	uint8_t *authKeyPtr = authenticationkey;

	while(param_getchar(Cmd, cmdp) != 0x00)
	{
		switch(param_getchar(Cmd, cmdp))
		{
			case 'h':
			case 'H':
				return usage_hf_mfu_rdbl();
			case 'k':
			case 'K':
				// EV1/NTAG size key
				keylen = param_gethex(Cmd, cmdp+1, data, 8);
				if ( !keylen ) {
					memcpy(authenticationkey, data, 4);
					cmdp += 2;
					hasPwdKey = true;
					break;
				}
				// UL-C size key	
				keylen = param_gethex(Cmd, cmdp+1, data, 32);
				if (!keylen){
					memcpy(authenticationkey, data, 16);
					cmdp += 2;
					hasAuthKey = true;
					break;
				}
				PrintAndLog("\nERROR: Key is incorrect length\n");
				errors = true; 
				break;
			case 'b':
			case 'B':
				blockNo = param_get8(Cmd, cmdp+1);
				if (blockNo < 0) {
					PrintAndLog("Wrong block number");
					errors = true;									
				}
				cmdp += 2;
				break;
			case 'l':
			case 'L':
				swapEndian = true;
				cmdp++;	
				break;				
			default:
				PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
				errors = true;
				break;
		}
		//Validations
		if(errors) return usage_hf_mfu_rdbl();
	}

	if ( blockNo == -1 ) return usage_hf_mfu_rdbl();
	// start with getting tagtype
	TagTypeUL_t tagtype = GetHF14AMfU_Type();
	if (tagtype == UL_ERROR) return -1;

	uint8_t maxblockno = 0;
	for (uint8_t idx = 0; idx < MAX_UL_TYPES; idx++){
		if (tagtype & UL_TYPES_ARRAY[idx])
			maxblockno = UL_MEMORY_ARRAY[idx];
	}
	if (blockNo > maxblockno){
		PrintAndLog("block number to large. Max block is %u/0x%02X \n", maxblockno,maxblockno);
		return usage_hf_mfu_rdbl();
	}

	// Swap endianness 
	if (swapEndian && hasAuthKey) authKeyPtr = SwapEndian64(authenticationkey, 16, 8);
	if (swapEndian && hasPwdKey)  authKeyPtr = SwapEndian64(authenticationkey, 4, 4);
	
	//Read Block
	UsbCommand c = {CMD_MIFAREU_READBL, {blockNo}};
	if ( hasAuthKey ){
		c.arg[1] = 1;
		memcpy(c.d.asBytes,authKeyPtr,16);
	}
	else if ( hasPwdKey ) {
		c.arg[1] = 2;
		memcpy(c.d.asBytes,authKeyPtr,4);
	}
	
	clearCommandBuffer();
	SendCommand(&c);
	UsbCommand resp;
	if (WaitForResponseTimeout(CMD_ACK,&resp,1500)) {
		uint8_t isOK = resp.arg[0] & 0xff;
		if (isOK) {
			uint8_t *data = resp.d.asBytes;
			PrintAndLog("\nBlock#  | Data        | Ascii");
			PrintAndLog("-----------------------------");
			PrintAndLog("%02d/0x%02X | %s| %s\n", blockNo, blockNo, sprint_hex(data, 4), sprint_ascii(data,4));
		}
		else {
			PrintAndLog("Failed reading block: (%02x)", isOK);
		}
	} else {
		PrintAndLog("Command execute time-out");
	}
	return 0;
}

int usage_hf_mfu_info(void) {
	PrintAndLog("It gathers information about the tag and tries to detect what kind it is.");
	PrintAndLog("Sometimes the tags are locked down, and you may need a key to be able to read the information");
	PrintAndLog("The following tags can be identified:\n");
	PrintAndLog("Ultralight, Ultralight-C, Ultralight EV1, NTAG 203, NTAG 210,");
	PrintAndLog("NTAG 212, NTAG 213, NTAG 215, NTAG 216, NTAG I2C 1K & 2K");
	PrintAndLog("my-d, my-d NFC, my-d move, my-d move NFC\n");
	PrintAndLog("Usage:  hf mfu info k <key> l");
	PrintAndLog("  Options : ");
	PrintAndLog("  k <key> : (optional) key for authentication [UL-C 16bytes, EV1/NTAG 4bytes]");
	PrintAndLog("  l       : (optional) swap entered key's endianness");
	PrintAndLog("");
	PrintAndLog("   sample : hf mfu info");
	PrintAndLog("          : hf mfu info k 00112233445566778899AABBCCDDEEFF");
	PrintAndLog("          : hf mfu info k AABBCCDDD");
	return 0;
}

int usage_hf_mfu_dump(void) {
	PrintAndLog("Reads all pages from Ultralight, Ultralight-C, Ultralight EV1");
	PrintAndLog("NTAG 203, NTAG 210, NTAG 212, NTAG 213, NTAG 215, NTAG 216");
	PrintAndLog("and saves binary dump into the file `filename.bin` or `cardUID.bin`");
	PrintAndLog("It autodetects card type.\n");	
	PrintAndLog("Usage:  hf mfu dump k <key> l n <filename w/o .bin> p <page#> q <#pages>");
	PrintAndLog("  Options :");
	PrintAndLog("  k <key> : (optional) key for authentication [UL-C 16bytes, EV1/NTAG 4bytes]");
	PrintAndLog("  l       : (optional) swap entered key's endianness");
	PrintAndLog("  n <FN > : filename w/o .bin to save the dump as");	
	PrintAndLog("  p <Pg > : starting Page number to manually set a page to start the dump at");	
	PrintAndLog("  q <qty> : number of Pages to manually set how many pages to dump");	
	PrintAndLog("");
	PrintAndLog("   sample : hf mfu dump");
	PrintAndLog("          : hf mfu dump n myfile");
	PrintAndLog("          : hf mfu dump k 00112233445566778899AABBCCDDEEFF");
	PrintAndLog("          : hf mfu dump k AABBCCDDD\n");
	return 0;
}

int usage_hf_mfu_rdbl(void) {
	PrintAndLog("Read a block and print. It autodetects card type.\n");	
	PrintAndLog("Usage:  hf mfu rdbl b <block number> k <key> l\n");
	PrintAndLog("  Options:");
	PrintAndLog("  b <no>  : block to read");
	PrintAndLog("  k <key> : (optional) key for authentication [UL-C 16bytes, EV1/NTAG 4bytes]");
	PrintAndLog("  l       : (optional) swap entered key's endianness");	
	PrintAndLog("");
	PrintAndLog("   sample : hf mfu rdbl b 0");
	PrintAndLog("          : hf mfu rdbl b 0 k 00112233445566778899AABBCCDDEEFF");
	PrintAndLog("          : hf mfu rdbl b 0 k AABBCCDDD\n");
	return 0;
}

int usage_hf_mfu_wrbl(void) {
	PrintAndLog("Write a block. It autodetects card type.\n");		
	PrintAndLog("Usage:  hf mfu wrbl b <block number> d <data> k <key> l\n");
	PrintAndLog("  Options:");	
	PrintAndLog("  b <no>   : block to write");
	PrintAndLog("  d <data> : block data - (8 hex symbols)");
	PrintAndLog("  k <key>  : (optional) key for authentication [UL-C 16bytes, EV1/NTAG 4bytes]");
	PrintAndLog("  l        : (optional) swap entered key's endianness");	
	PrintAndLog("");
	PrintAndLog("    sample : hf mfu wrbl b 0 d 01234567");
	PrintAndLog("           : hf mfu wrbl b 0 d 01234567 k AABBCCDDD\n");
	return 0;
}

int usage_hf_mfu_eload(void) {
	PrintAndLog("It loads emul dump from the file `filename.eml`");
	PrintAndLog("Hint: See script dumptoemul-mfu.lua to convert the .bin to the eml");
	PrintAndLog("Usage:  hf mfu eload u <file name w/o `.eml`> [numblocks]");
	PrintAndLog("  Options:");
	PrintAndLog("    h          : this help");	
	PrintAndLog("    u          : UL (required)");
	PrintAndLog("    [filename] : without `.eml` (required)");	
	PrintAndLog("    numblocks  : number of blocks to load from eml file (optional)");
	PrintAndLog("");
	PrintAndLog("  sample: hf mfu eload u filename");
	PrintAndLog("          hf mfu eload u filename 57");
			return 0;
}

int usage_hf_mfu_sim(void) {
	PrintAndLog("\nEmulating Ultralight tag from emulator memory\n");
	PrintAndLog("\nBe sure to load the emulator memory first!\n");
	PrintAndLog("Usage: hf mfu sim t 7 u <uid>");
	PrintAndLog("  Options:");
	PrintAndLog("    h       : this help");
	PrintAndLog("    t 7     : 7 = NTAG or Ultralight sim (required)");
	PrintAndLog("    u <uid> : 4 or 7 byte UID (optional)");
	PrintAndLog("\n   sample : hf mfu sim t 7");
	PrintAndLog("          : hf mfu sim t 7 u 1122344556677\n");
	
	return 0;
}

int usage_hf_mfu_ucauth(void) {
	PrintAndLog("Usage:  hf mfu cauth k <key number>");
	PrintAndLog("      0 (default): 3DES standard key");
	PrintAndLog("      1 : all 0x00 key");
	PrintAndLog("      2 : 0x00-0x0F key");
	PrintAndLog("      3 : nfc key");
	PrintAndLog("      4 : all 0x01 key");
	PrintAndLog("      5 : all 0xff key");
	PrintAndLog("      6 : 0x00-0xFF key");		
	PrintAndLog("\n      sample : hf mfu cauth k");
	PrintAndLog("               : hf mfu cauth k 3");
	return 0;
}

int usage_hf_mfu_ucsetpwd(void) {
	PrintAndLog("Usage:  hf mfu setpwd <password (32 hex symbols)>");
	PrintAndLog("       [password] - (32 hex symbols)");
	PrintAndLog("");
	PrintAndLog("sample: hf mfu setpwd 000102030405060708090a0b0c0d0e0f");
	PrintAndLog("");
	return 0;
}

int usage_hf_mfu_ucsetuid(void) {
	PrintAndLog("Usage:  hf mfu setuid <uid (14 hex symbols)>");
	PrintAndLog("       [uid] - (14 hex symbols)");
	PrintAndLog("\nThis only works for Magic Ultralight tags.");
	PrintAndLog("");
	PrintAndLog("sample: hf mfu setuid 11223344556677");
	PrintAndLog("");
	return 0;
}

int  usage_hf_mfu_gendiverse(void){
	PrintAndLog("Usage:  hf mfu gen <uid (8 hex symbols)>");
	PrintAndLog("");
	PrintAndLog("sample: hf mfu gen 11223344");
	PrintAndLog("");
	return 0;
}

int  usage_hf_mfu_pwdgen(void){
	PrintAndLog("Usage:  hf mfu pwdgen <uid (14 hex symbols)>");
	PrintAndLog("");
	PrintAndLog("sample: hf mfu pwdgen 11223344556677");
	PrintAndLog("");
	return 0;
}

#define DUMP_PREFIX_LENGTH 48 
//
//  Mifare Ultralight / Ultralight-C / Ultralight-EV1
//  Read and Dump Card Contents,  using auto detection of tag size.
int CmdHF14AMfUDump(const char *Cmd){

	FILE *fout;
	char filename[FILE_PATH_SIZE] = {0x00};
	char *fnameptr = filename;
	uint8_t *lockbytes_t = NULL;
	uint8_t lockbytes[2] = {0x00};
	uint8_t *lockbytes_t2 = NULL;
	uint8_t lockbytes2[2] = {0x00};
	bool bit[16]  = {0x00};
	bool bit2[16] = {0x00};
	uint8_t data[1024] = {0x00};
	bool hasAuthKey = false;
	int i = 0;
	int Pages = 16;
	bool tmplockbit = false;
	uint8_t dataLen = 0;
	uint8_t cmdp = 0;
	uint8_t authenticationkey[16] = {0x00};
	memset(authenticationkey, 0x00, sizeof(authenticationkey));
	uint8_t	*authKeyPtr = authenticationkey;
	size_t fileNlen = 0;
	bool errors = false;
	bool swapEndian = false;
	bool manualPages = false;
	uint8_t startPage = 0;
	char tempStr[50];

	while(param_getchar(Cmd, cmdp) != 0x00)
	{
		switch(param_getchar(Cmd, cmdp))
		{
		case 'h':
		case 'H':
			return usage_hf_mfu_dump();
		case 'k':
		case 'K':
			dataLen = param_getstr(Cmd, cmdp+1, tempStr);
			if (dataLen == 32 || dataLen == 8) { //ul-c or ev1/ntag key length
				errors = param_gethex(tempStr, 0, authenticationkey, dataLen);
				dataLen /= 2;
			} else {
				PrintAndLog("\nERROR: Key is incorrect length\n");
				errors = true;
			}
			cmdp += 2;
			hasAuthKey = true;
			break;
		case 'l':
		case 'L':
			swapEndian = true;
			cmdp++;
			break;
		case 'n':
		case 'N':
			fileNlen = param_getstr(Cmd, cmdp+1, filename);
			if (!fileNlen) errors = true; 
			if (fileNlen > FILE_PATH_SIZE-5) fileNlen = FILE_PATH_SIZE-5;
			cmdp += 2;
			break;
		case 'p':
		case 'P': //set start page
			startPage = param_get8(Cmd, cmdp+1);
			manualPages = true;
			cmdp += 2;
			break;
		case 'q':
		case 'Q':
			Pages = param_get8(Cmd, cmdp+1);
			cmdp += 2;
			manualPages = true;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = true;
			break;
		}
		if(errors) break;
	}

	//Validations
	if(errors) return usage_hf_mfu_dump();

	//if we entered a key in little endian and set the swapEndian switch - switch it...
	if (swapEndian && hasAuthKey) 
		authKeyPtr = SwapEndian64(authenticationkey, dataLen, (dataLen == 16) ? 8 : 4);

	TagTypeUL_t tagtype = GetHF14AMfU_Type();
	if (tagtype == UL_ERROR) return -1;

	if (!manualPages) //get number of pages to read
		for (uint8_t idx = 0; idx < MAX_UL_TYPES; idx++)
			if (tagtype & UL_TYPES_ARRAY[idx])
				Pages = UL_MEMORY_ARRAY[idx]+1; //add one as maxblks starts at 0

	ul_print_type(tagtype, 0);
	PrintAndLog("Reading tag memory...");
	UsbCommand c = {CMD_MIFAREU_READCARD, {startPage,Pages}};
	if ( hasAuthKey ) {
		if (tagtype & UL_C)
			c.arg[2] = 1; //UL_C auth
		else
			c.arg[2] = 2; //UL_EV1/NTAG auth

		memcpy(c.d.asBytes, authKeyPtr, dataLen);
	}

	clearCommandBuffer();
	SendCommand(&c);
	UsbCommand resp;
	if (!WaitForResponseTimeout(CMD_ACK, &resp,1500)) {
		PrintAndLog("Command execute time-out");
		return 1;
	}
	if (resp.arg[0] != 1) {
		PrintAndLog("Failed reading block: (%02x)", i);
		return 1;
	}

	uint32_t startindex = resp.arg[2];
	uint32_t bufferSize = resp.arg[1];
	if (bufferSize > sizeof(data)) {
		PrintAndLog("Data exceeded Buffer size!");
		bufferSize = sizeof(data);
	}
	GetFromBigBuf(data, bufferSize, startindex);
	WaitForResponse(CMD_ACK,NULL);

	Pages = bufferSize/4;
	// Load lock bytes.
	int j = 0;

	lockbytes_t = data + 8;
	lockbytes[0] = lockbytes_t[2];
	lockbytes[1] = lockbytes_t[3];
	for(j = 0; j < 16; j++){
		bit[j] = lockbytes[j/8] & ( 1 <<(7-j%8));
	}

	// Load bottom lockbytes if available
	// TODO -- FIGURE OUT LOCK BYTES FOR TO EV1 and/or NTAG
	if ( Pages == 44 ) {
		lockbytes_t2 = data + (40*4);
		lockbytes2[0] = lockbytes_t2[2];
		lockbytes2[1] = lockbytes_t2[3];
		for (j = 0; j < 16; j++) {
			bit2[j] = lockbytes2[j/8] & ( 1 <<(7-j%8));
		}
	}

	uint8_t	get_pack[] = {0,0};
	iso14a_card_select_t card;
	uint8_t dump_file_data[1024+DUMP_PREFIX_LENGTH] = {0x00};
	uint8_t get_version[] = {0,0,0,0,0,0,0,0,0};
	uint8_t	get_tearing[] = {0,0,0};
	uint8_t	get_counter[] = {0,0,0};
	uint8_t	dummy_pack[] = {0,0};
	uint8_t	get_signature[32];
	memset( get_signature, 0, sizeof(get_signature) );

	// not ul_c and not std ul then attempt to get deeper info
	if (!(tagtype & UL_C || tagtype & UL)) {
		//attempt to read pack
		if (!ul_auth_select( &card, tagtype, true, authKeyPtr, get_pack, sizeof(get_pack))) {
			//reset pack
			get_pack[0]=0;
			get_pack[1]=0;
		}
		ul_switch_off_field();
		// add pack to block read
		memcpy(data + (Pages*4) - 4, get_pack, sizeof(get_pack));
	if ( hasAuthKey )
		ul_auth_select( &card, tagtype, hasAuthKey, authKeyPtr, dummy_pack, sizeof(dummy_pack));
	else
		ul_select(&card);

	ulev1_getVersion( get_version, sizeof(get_version) );
	for ( uint8_t i = 0; i<3; ++i) {
		ulev1_readTearing(i, get_tearing+i, 1);
		ulev1_readCounter(i, get_counter, sizeof(get_counter) );
	}
	ul_switch_off_field();
	if ( hasAuthKey )
		ul_auth_select( &card, tagtype, hasAuthKey, authKeyPtr, dummy_pack, sizeof(dummy_pack));
	else
		ul_select(&card);
	ulev1_readSignature( get_signature, sizeof(get_signature));
	ul_switch_off_field();
	}

	// format and add keys to block dump output
	if (hasAuthKey) {
		// if we didn't swapendian before - do it now for the sprint_hex call
		// NOTE: default entry is bigendian (unless swapped), sprint_hex outputs little endian
		//       need to swap to keep it the same
		if (!swapEndian){
			authKeyPtr = SwapEndian64(authenticationkey, dataLen, (dataLen == 16) ? 8 : 4);
		} else {
			authKeyPtr = authenticationkey;
		}

		if (tagtype & UL_C){ //add 4 pages
			memcpy(data + Pages*4, authKeyPtr, dataLen);
			Pages += dataLen/4;  
		} else { // 2nd page from end
			memcpy(data + (Pages*4) - 8, authenticationkey, dataLen);
		}
	}

	//add *special* blocks to dump
	//get version
	memcpy(dump_file_data, get_version, sizeof(get_version));
	//tearing
	memcpy(dump_file_data+10, get_tearing, sizeof(get_tearing));
	//pack
	memcpy(dump_file_data+13, get_pack, sizeof(get_pack));
	//signature
	memcpy(dump_file_data+16, get_signature, sizeof(get_signature));
	//add regular block read data to dump
	memcpy(dump_file_data+DUMP_PREFIX_LENGTH, data, Pages*4);

	PrintAndLog("\n*Special* block data:");
	PrintAndLog("\nDataType| Data        |   | Ascii");
	PrintAndLog("---------------------------------");
	PrintAndLog("GetVer-1| %s|   | %s", sprint_hex(dump_file_data, 4), sprint_ascii(dump_file_data, 4));
	PrintAndLog("GetVer-2| %s|   | %s", sprint_hex(dump_file_data+4, 4), sprint_ascii(dump_file_data+4,4));
	PrintAndLog("TBD     | 00 00       |   | ");
	PrintAndLog("Tearing |    %s|   | %s", sprint_hex(dump_file_data+10, 3), sprint_ascii(dump_file_data+10,3) );
	PrintAndLog("Pack    |    %s   |   | %s", sprint_hex(dump_file_data+13, 2), sprint_ascii(dump_file_data+13,2) );
	PrintAndLog("TBD     |          00 |   | ");
	PrintAndLog("Sig-1   | %s|   | %s", sprint_hex(dump_file_data+16, 4), sprint_ascii(dump_file_data+16,4));
	PrintAndLog("Sig-2   | %s|   | %s", sprint_hex(dump_file_data+20, 4), sprint_ascii(dump_file_data+20,4));
	PrintAndLog("Sig-3   | %s|   | %s", sprint_hex(dump_file_data+24, 4), sprint_ascii(dump_file_data+24,4));
	PrintAndLog("Sig-4   | %s|   | %s", sprint_hex(dump_file_data+28, 4), sprint_ascii(dump_file_data+28,4));
	PrintAndLog("Sig-5   | %s|   | %s", sprint_hex(dump_file_data+32, 4), sprint_ascii(dump_file_data+32,4));
	PrintAndLog("Sig-6   | %s|   | %s", sprint_hex(dump_file_data+36, 4), sprint_ascii(dump_file_data+36,4));
	PrintAndLog("Sig-7   | %s|   | %s", sprint_hex(dump_file_data+40, 4), sprint_ascii(dump_file_data+40,4));
	PrintAndLog("Sig-8   | %s|   | %s", sprint_hex(dump_file_data+44, 4), sprint_ascii(dump_file_data+44,4));
	PrintAndLog("\nBlock#  | Data        |lck| Ascii");
	PrintAndLog("---------------------------------");
	for (i = 0; i < Pages; ++i) {
		if ( i < 3 ) {
			PrintAndLog("%02d/0x%02X | %s|   | %s", i+startPage, i+startPage, sprint_hex(data + i * 4, 4), sprint_ascii(data + i * 4,4) );
			continue;
		}
		switch(i){
			case 3: tmplockbit = bit[4]; break;
			case 4:	tmplockbit = bit[3]; break;
			case 5:	tmplockbit = bit[2]; break;
			case 6:	tmplockbit = bit[1]; break;
			case 7:	tmplockbit = bit[0]; break;
			case 8:	tmplockbit = bit[15]; break;
			case 9: tmplockbit = bit[14]; break;
			case 10: tmplockbit = bit[13]; break;
			case 11: tmplockbit = bit[12]; break;
			case 12: tmplockbit = bit[11]; break;
			case 13: tmplockbit = bit[10]; break;
			case 14: tmplockbit = bit[9]; break;
			case 15: tmplockbit = bit[8]; break;
			case 16:
			case 17:
			case 18:
			case 19: tmplockbit = bit2[6]; break;
			case 20:
			case 21:
			case 22:
			case 23: tmplockbit = bit2[5]; break; 
			case 24:
			case 25:
			case 26:
			case 27: tmplockbit = bit2[4]; break;
			case 28:
			case 29:
			case 30:
			case 31: tmplockbit = bit2[2]; break;
			case 32:
			case 33:
			case 34:
			case 35: tmplockbit = bit2[1]; break; 
			case 36:
			case 37:
			case 38:
			case 39: tmplockbit = bit2[0]; break; 
			case 40: tmplockbit = bit2[12]; break;
			case 41: tmplockbit = bit2[11]; break;
			case 42: tmplockbit = bit2[10]; break; //auth0
			case 43: tmplockbit = bit2[9]; break;  //auth1
			default: break;
		}
		PrintAndLog("%02d/0x%02X | %s| %d | %s", i+startPage, i+startPage, sprint_hex(data + i * 4, 4), tmplockbit, sprint_ascii(data + i * 4,4));
	}
	PrintAndLog("---------------------------------");

	// user supplied filename?
	if (fileNlen < 1) {
		// UID = data 0-1-2 4-5-6-7  (skips a beat)
		sprintf(fnameptr,"%02X%02X%02X%02X%02X%02X%02X.bin",
			data[0],data[1], data[2], data[4],data[5],data[6], data[7]);
	} else {
		sprintf(fnameptr + fileNlen,".bin");
	}

	if ((fout = fopen(filename,"wb")) == NULL) { 
		PrintAndLog("Could not create file name %s", filename);
		return 1;
	}
	fwrite( dump_file_data, 1, Pages*4 + DUMP_PREFIX_LENGTH, fout );
	if (fout) {
		fclose(fout);
		fout = NULL;
	}
	PrintAndLog("Dumped %d pages, wrote %d bytes to %s", Pages+(DUMP_PREFIX_LENGTH/4), Pages*4 + DUMP_PREFIX_LENGTH, filename);
	return 0;
}

//-------------------------------------------------------------------------------
// Ultralight C Methods
//-------------------------------------------------------------------------------

//
// Ultralight C Authentication Demo {currently uses hard-coded key}
//
int CmdHF14AMfucAuth(const char *Cmd){

	uint8_t keyNo = 3;
	bool errors = false;

	char cmdp = param_getchar(Cmd, 0);

	//Change key to user defined one
	if (cmdp == 'k' || cmdp == 'K'){
		keyNo = param_get8(Cmd, 1);
		if(keyNo >= KEYS_3DES_COUNT) 
			errors = true;
	}

	if (cmdp == 'h' || cmdp == 'H') errors = true;
	
	if (errors) return usage_hf_mfu_ucauth(); 

	uint8_t *key = default_3des_keys[keyNo];
	if (ulc_authentication(key, true))
		PrintAndLog("Authentication successful. 3des key: %s",sprint_hex(key, 16));
	else
		PrintAndLog("Authentication failed");
		
	return 0;
}

/**
A test function to validate that the polarssl-function works the same 
was as the openssl-implementation. 
Commented out, since it requires openssl 

int CmdTestDES(const char * cmd)
{
	uint8_t key[16] = {0x00};	
	
	memcpy(key,key3_3des_data,16);  
	DES_cblock RndA, RndB;

	PrintAndLog("----------OpenSSL DES implementation----------");
	{
		uint8_t e_RndB[8] = {0x00};
		unsigned char RndARndB[16] = {0x00};

		DES_cblock iv = { 0 };
		DES_key_schedule ks1,ks2;
		DES_cblock key1,key2;

		memcpy(key,key3_3des_data,16);  
		memcpy(key1,key,8);
		memcpy(key2,key+8,8);


		DES_set_key((DES_cblock *)key1,&ks1);
		DES_set_key((DES_cblock *)key2,&ks2);

		DES_random_key(&RndA);
		PrintAndLog("     RndA:%s",sprint_hex(RndA, 8));
		PrintAndLog("     e_RndB:%s",sprint_hex(e_RndB, 8));
		//void DES_ede2_cbc_encrypt(const unsigned char *input,
		//    unsigned char *output, long length, DES_key_schedule *ks1,
		//    DES_key_schedule *ks2, DES_cblock *ivec, int enc);
		DES_ede2_cbc_encrypt(e_RndB,RndB,sizeof(e_RndB),&ks1,&ks2,&iv,0);

		PrintAndLog("     RndB:%s",sprint_hex(RndB, 8));
		rol(RndB,8);
		memcpy(RndARndB,RndA,8);
		memcpy(RndARndB+8,RndB,8);
		PrintAndLog("     RA+B:%s",sprint_hex(RndARndB, 16));
		DES_ede2_cbc_encrypt(RndARndB,RndARndB,sizeof(RndARndB),&ks1,&ks2,&e_RndB,1);
		PrintAndLog("enc(RA+B):%s",sprint_hex(RndARndB, 16));

	}
	PrintAndLog("----------PolarSSL implementation----------");
	{
		uint8_t random_a[8]     = { 0 };
		uint8_t enc_random_a[8] = { 0 };
		uint8_t random_b[8]     = { 0 };
		uint8_t enc_random_b[8] = { 0 };
		uint8_t random_a_and_b[16] = { 0 };
		des3_context ctx        = { 0 };

		memcpy(random_a, RndA,8);

		uint8_t output[8]       = { 0 };
		uint8_t iv[8]           = { 0 };

		PrintAndLog("     RndA  :%s",sprint_hex(random_a, 8));
		PrintAndLog("     e_RndB:%s",sprint_hex(enc_random_b, 8));

		des3_set2key_dec(&ctx, key);

		des3_crypt_cbc(&ctx      // des3_context *ctx
			, DES_DECRYPT        // int mode
			, sizeof(random_b)   // size_t length
			, iv                 // unsigned char iv[8]
			, enc_random_b       // const unsigned char *input
			, random_b           // unsigned char *output
			);

		PrintAndLog("     RndB:%s",sprint_hex(random_b, 8));

		rol(random_b,8);
		memcpy(random_a_and_b  ,random_a,8);
		memcpy(random_a_and_b+8,random_b,8);
		
		PrintAndLog("     RA+B:%s",sprint_hex(random_a_and_b, 16));

		des3_set2key_enc(&ctx, key);

		des3_crypt_cbc(&ctx          // des3_context *ctx
			, DES_ENCRYPT            // int mode
			, sizeof(random_a_and_b)   // size_t length
			, enc_random_b           // unsigned char iv[8]
			, random_a_and_b         // const unsigned char *input
			, random_a_and_b         // unsigned char *output
			);

		PrintAndLog("enc(RA+B):%s",sprint_hex(random_a_and_b, 16));
	}
	return 0;	
}
**/

// 
// Mifare Ultralight C - Set password
//
int CmdHF14AMfucSetPwd(const char *Cmd){

	uint8_t pwd[16] = {0x00};	
	char cmdp = param_getchar(Cmd, 0);

	if (strlen(Cmd) == 0  || cmdp == 'h' || cmdp == 'H') return usage_hf_mfu_ucsetpwd();
	
	if (param_gethex(Cmd, 0, pwd, 32)) {
		PrintAndLog("Password must include 32 HEX symbols");
		return 1;
	}
	
	UsbCommand c = {CMD_MIFAREUC_SETPWD};	
	memcpy( c.d.asBytes, pwd, 16);
	clearCommandBuffer();
	SendCommand(&c);

	UsbCommand resp;
	if (WaitForResponseTimeout(CMD_ACK,&resp,1500) ) {
		if ( (resp.arg[0] & 0xff) == 1) {
			PrintAndLog("Ultralight-C new password: %s", sprint_hex(pwd,16));
		} else {
			PrintAndLog("Failed writing at block %d", resp.arg[1] & 0xff);
			return 1;
		}
	} else {
		PrintAndLog("command execution time out");
		return 1;
	}	
	return 0;
}

//
// Magic UL / UL-C tags  - Set UID
//
int CmdHF14AMfucSetUid(const char *Cmd){

	UsbCommand c;
	UsbCommand resp;
	uint8_t uid[7] = {0x00};
	char cmdp = param_getchar(Cmd, 0);
	
	if (strlen(Cmd) == 0  || cmdp == 'h' || cmdp == 'H') return usage_hf_mfu_ucsetuid();

	if (param_gethex(Cmd, 0, uid, 14)) {
		PrintAndLog("UID must include 14 HEX symbols");
		return 1;
	}

	// read block2. 
	c.cmd = CMD_MIFAREU_READBL;
	c.arg[0] = 2;
	clearCommandBuffer();
	SendCommand(&c);
	if (!WaitForResponseTimeout(CMD_ACK,&resp,1500)) {
		PrintAndLog("Command execute timeout");
		return 2;
	}

	// save old block2.
	uint8_t oldblock2[4] = {0x00};
	memcpy(resp.d.asBytes, oldblock2, 4);
	
	// block 0.
	c.cmd = CMD_MIFAREU_WRITEBL;
	c.arg[0] = 0;
	c.d.asBytes[0] = uid[0];
	c.d.asBytes[1] = uid[1];
	c.d.asBytes[2] = uid[2];
	c.d.asBytes[3] =  0x88 ^ uid[0] ^ uid[1] ^ uid[2];
	clearCommandBuffer();
	SendCommand(&c);
	if (!WaitForResponseTimeout(CMD_ACK,&resp,1500)) {
		PrintAndLog("Command execute timeout");
		return 3;
	}

	// block 1.
	c.arg[0] = 1;
	c.d.asBytes[0] = uid[3];
	c.d.asBytes[1] = uid[4];
	c.d.asBytes[2] = uid[5];
	c.d.asBytes[3] = uid[6];
	clearCommandBuffer();
	SendCommand(&c);
	if (!WaitForResponseTimeout(CMD_ACK,&resp,1500) ) {
		PrintAndLog("Command execute timeout");
		return 4;
	}

	// block 2.
	c.arg[0] = 2;
	c.d.asBytes[0] = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];
	c.d.asBytes[1] = oldblock2[1];
	c.d.asBytes[2] = oldblock2[2];
	c.d.asBytes[3] = oldblock2[3];
	clearCommandBuffer();
	SendCommand(&c);
	if (!WaitForResponseTimeout(CMD_ACK,&resp,1500) ) {
		PrintAndLog("Command execute timeout");
		return 5;
	}
	
	return 0;
}

int CmdHF14AMfuGenDiverseKeys(const char *Cmd){

	uint8_t uid[4];	
	char cmdp = param_getchar(Cmd, 0);
	if (strlen(Cmd) == 0  || cmdp == 'h' || cmdp == 'H') return usage_hf_mfu_gendiverse();

	if (param_gethex(Cmd, 0, uid, 8)) {
		PrintAndLog("UID must include 8 HEX symbols");
		return 1;
	}
	
	uint8_t iv[8] = { 0x00 };
	uint8_t block = 0x01;
	
	uint8_t mifarekeyA[] = { 0xA0,0xA1,0xA2,0xA3,0xA4,0xA5 };
	uint8_t mifarekeyB[] = { 0xB0,0xB1,0xB2,0xB3,0xB4,0xB5 };
	uint8_t dkeyA[8] = { 0x00 };
	uint8_t dkeyB[8] = { 0x00 };
	
	uint8_t masterkey[] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff };
	
	uint8_t mix[8] = { 0x00 };
	uint8_t divkey[8] = { 0x00 };
	
	memcpy(mix, mifarekeyA, 4);
	
	mix[4] = mifarekeyA[4] ^ uid[0];
	mix[5] = mifarekeyA[5] ^ uid[1];
	mix[6] = block ^ uid[2];
	mix[7] = uid[3];
	
	des3_context ctx = { 0x00 };
	des3_set2key_enc(&ctx, masterkey);

	des3_crypt_cbc(&ctx  // des3_context
		, DES_ENCRYPT    // int mode
		, sizeof(mix)    // length
		, iv             // iv[8]
		, mix            // input
		, divkey         // output
		);

	PrintAndLog("-- 3DES version");
	PrintAndLog("Masterkey    :\t %s", sprint_hex(masterkey,sizeof(masterkey)));
	PrintAndLog("UID          :\t %s", sprint_hex(uid, sizeof(uid)));
	PrintAndLog("block        :\t %0d", block);
	PrintAndLog("Mifare key   :\t %s", sprint_hex(mifarekeyA, sizeof(mifarekeyA)));
	PrintAndLog("Message      :\t %s", sprint_hex(mix, sizeof(mix)));
	PrintAndLog("Diversified key: %s", sprint_hex(divkey+1, 6));
	
	for (int i=0; i < sizeof(mifarekeyA); ++i){
		dkeyA[i] = (mifarekeyA[i] << 1) & 0xff;
		dkeyA[6] |=  ((mifarekeyA[i] >> 7) & 1) << (i+1);
	}
	
	for (int i=0; i < sizeof(mifarekeyB); ++i){
		dkeyB[1] |=  ((mifarekeyB[i] >> 7) & 1) << (i+1);
		dkeyB[2+i] = (mifarekeyB[i] << 1) & 0xff;
	}
	
	uint8_t zeros[8] = {0x00};
	uint8_t newpwd[8] = {0x00};
	uint8_t dmkey[24] = {0x00};
	memcpy(dmkey, dkeyA, 8);
	memcpy(dmkey+8, dkeyB, 8);
	memcpy(dmkey+16, dkeyA, 8);
	memset(iv, 0x00, 8);
	
	des3_set3key_enc(&ctx, dmkey);

	des3_crypt_cbc(&ctx  // des3_context
		, DES_ENCRYPT    // int mode
		, sizeof(newpwd) // length
		, iv             // iv[8]
		, zeros         // input
		, newpwd         // output
		);
	
	PrintAndLog("\n-- DES version");
	PrintAndLog("Mifare dkeyA :\t %s", sprint_hex(dkeyA, sizeof(dkeyA)));
	PrintAndLog("Mifare dkeyB :\t %s", sprint_hex(dkeyB, sizeof(dkeyB)));
	PrintAndLog("Mifare ABA   :\t %s", sprint_hex(dmkey, sizeof(dmkey)));
	PrintAndLog("Mifare Pwd   :\t %s", sprint_hex(newpwd, sizeof(newpwd)));
	
	// next. from the diversify_key method.	
	return 0;
}

int CmdHF14AMfUeLoad(const char *Cmd) {
	char ctmp = param_getchar(Cmd, 0);
	if ( ctmp == 'h' || ctmp == 'H' || ctmp == 0x00) return usage_hf_mfu_eload();
	return CmdHF14AMfELoad(Cmd);
}

int CmdHF14AMfUSim(const char *Cmd) {
	char ctmp = param_getchar(Cmd, 0);
	if ( ctmp == 'h' || ctmp == 'H' || ctmp == 0x00) return usage_hf_mfu_sim();
	return CmdHF14ASim(Cmd);
}

int CmdHF14AMfuPwdGen(const char *Cmd){
	uint8_t uid[7] = {0x00};	
	char cmdp = param_getchar(Cmd, 0);
	if (strlen(Cmd) == 0  || cmdp == 'h' || cmdp == 'H') return usage_hf_mfu_pwdgen();

	if (param_gethex(Cmd, 0, uid, 14)) return usage_hf_mfu_pwdgen();
	
	PrintAndLog(" algo | pwd      | pack");
	PrintAndLog("------+----------+-----");
	PrintAndLog(" EV1  | %08X | %04X", ul_ev1_pwdgenA(uid), ul_ev1_packgenA(uid));
	PrintAndLog(" Ami  | %08X | %04X", ul_ev1_pwdgenB(uid), ul_ev1_packgenB(uid));
	PrintAndLog(" LD   | %08X | %04X", ul_ev1_pwdgenC(uid), ul_ev1_packgenC(uid));
	return 0;
}
//------------------------------------
// Menu Stuff
//------------------------------------
static command_t CommandTable[] =
{
	{"help",	CmdHelp,			1, "This help"},
	{"dbg",		CmdHF14AMfDbg,		0, "Set default debug mode"},
	{"info",	CmdHF14AMfUInfo,	0, "Tag information"},
	{"dump",	CmdHF14AMfUDump,	0, "Dump Ultralight / Ultralight-C / NTAG tag to binary file"},
	{"eload",	CmdHF14AMfUeLoad,   0, "load Ultralight .eml dump file into emulator memory"},	
	{"rdbl",	CmdHF14AMfURdBl,	0, "Read block"},
	{"wrbl",	CmdHF14AMfUWrBl,	0, "Write block"},
	{"cauth",	CmdHF14AMfucAuth,	0, "Authentication    - Ultralight C"},
	{"setpwd",	CmdHF14AMfucSetPwd, 0, "Set 3des password - Ultralight-C"},
	{"setuid",	CmdHF14AMfucSetUid, 0, "Set UID - MAGIC tags only"},
	{"sim",		CmdHF14AMfUSim,     0, "Simulate Ultralight from emulator memory"},		
	{"gen",		CmdHF14AMfuGenDiverseKeys , 1, "Generate 3des mifare diversified keys"},
	{"pwdgen",	CmdHF14AMfuPwdGen, 1, "Generate pwd from known algos"},
	{NULL, NULL, 0, NULL}
};

int CmdHFMFUltra(const char *Cmd){
	clearCommandBuffer();
	//WaitForResponseTimeout(CMD_ACK,NULL,100);
	CmdsParse(CommandTable, Cmd);
	return 0;
}

int CmdHelp(const char *Cmd){
	CmdsHelp(CommandTable);
	return 0;
}
