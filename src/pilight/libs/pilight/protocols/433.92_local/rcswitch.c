/*
	Copyright (C) 2015 CurlyMo and Meloen

	This file is part of pilight.

        It is a crude implementation of rcswitch

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "rcswitch.h"

#define PULSE_MULTIPLIER	4
#define MIN_PULSE_LENGTH	80
#define MAX_PULSE_LENGTH	650
#define AVG_PULSE_LENGTH	400
#define MIN_RAW_LENGTH		8
#define MAX_RAW_LENGTH      100
#define TOLERANCE           60

static const rcproto proto[] = {
  { 350, {   1,  31 }, {   1,  3 }, {  3,   1 }, false },    // protocol 1
  { 650, {   1,  10 }, {   1,  2 }, {  2,   1 }, false },    // protocol 2
  { 100, {  30,  71 }, {   4, 11 }, {  9,   6 }, false },    // protocol 3
  { 380, {   1,   6 }, {   1,  3 }, {  3,   1 }, false },    // protocol 4
  { 500, {   6,  14 }, {   1,  2 }, {  2,   1 }, false },    // protocol 5
  { 450, {  23,   1 }, {   1,  2 }, {  2,   1 },  true },    // protocol 6 (HT6P20B)
  { 150, {   2,  62 }, {   1,  6 }, {  6,   1 }, false },    // protocol 7 (HS2303-PT, i. e. used in AUKEY Remote)
  { 200, {   3, 130 }, {   7, 16 }, {  3,  16 }, false },    // protocol 8 Conrad RS-200 RX
  { 200, { 130,   7 }, {  16,  7 }, { 16,   3 },  true },    // protocol 9 Conrad RS-200 TX
  { 365, {  18,   1 }, {   3,  1 }, {  1,   3 },  true },    // protocol 10 (1ByOne Doorbell)
  { 270, {  36,   1 }, {   1,  2 }, {  2,   1 },  true },    // protocol 11 (HT12E)
  { 320, {  36,   1 }, {   1,  2 }, {  2,   1 },  true },    // protocol 12 (SM5212)
  { 500, {   1,  14 }, {   1,  3 }, {  3,   1 }, false },    // protocol 13 (Blyss Doorbell Ref. DC6-FR-WH 656185)
  { 415, {   1,  30 }, {   1,  3 }, {  4,   1 }, false },    // protocol 14 (sc2260R4)
  { 250, {  20,  10 }, {   1,  1 }, {  3,   1 }, false },    // protocol 15 (Home NetWerks Bathroom Fan Model 6201-500)
  {  80, {   3,  25 }, {   3, 13 }, { 11,   5 }, false },    // protocol 16 (ORNO OR-GB-417GD)
  {  82, {   2,  65 }, {   3,  5 }, {  7,   1 }, false },    // protocol 17 (CLARUS BHC993BF-3)
  { 560, {  16,   8 }, {   1,  1 }, {  1,   3 }, false }     // protocol 18 (NEC)
  };

enum {
   numProto = sizeof(proto) / sizeof(proto[0])
};

static int validate(void) {
    if(rcswitch->rawlen < 7) {
        return -1;
    }

	return 0;
}

static char* dec2binWzerofill(unsigned long Dec, unsigned int bitLength) {
  static char bin[64];
  unsigned int i = 0;

  while (Dec > 0) {
    bin[32 + i++] = ((Dec & 1) > 0) ? '1' : '0';
    Dec = Dec >> 1;
  }

  for (unsigned int j = 0; j < bitLength; j++) {
    if (j >= bitLength - i) {
      bin[j] = bin[31 + i - (j - (bitLength - i))];
    } else {
      bin[j] = '0';
    }
  }
  bin[bitLength] = '\0';

  return bin;
}

static void createMessage(unsigned long value, int protocol, int length, int delay) {
	rcswitch->message = json_mkobject();
	json_append_member(rcswitch->message, "id", json_mknumber(value, 0));
	json_append_member(rcswitch->message, "protocol", json_mknumber(protocol, 0));
	json_append_member(rcswitch->message, "length", json_mknumber(length,0));
	json_append_member(rcswitch->message, "delay", json_mknumber(delay,0));
	const char* binary = dec2binWzerofill(value, length);
	json_append_member(rcswitch->message, "binary", json_mkstring(binary));
	char tx_data[7];
	sprintf(tx_data, "%06X", value);
    json_append_member(rcswitch->message, "hex", json_mkstring(tx_data));

}
static inline unsigned int diff(int A, int B) {
  return abs(A - B);
}
static void parseCode(void) {
        for(unsigned int i = 1; i <= numProto; i++) {
          if (receiveProtocol(i)) {
            // receive succeeded for protocol i
            break;
          }
        }
}
bool receiveProtocol(const int p) {
    const rcproto pro = proto[p-1];

    unsigned long code = 0;
    const unsigned int syncLengthInPulses =  ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
    const unsigned int delay = rcswitch->raw[rcswitch->rawlen-1] / syncLengthInPulses;
    const unsigned int delayTolerance = delay * TOLERANCE / 100;

    const unsigned int firstDataTiming = (pro.invertedSignal) ? (1) : (0);

    for (unsigned int i = firstDataTiming; i < rcswitch->rawlen-2; i += 2) {
        code <<= 1;
        if (diff(rcswitch->raw[i], delay * pro.zero.high) < delayTolerance &&
            diff(rcswitch->raw[i + 1], delay * pro.zero.low) < delayTolerance) {
            // zero
        } else if (diff(rcswitch->raw[i], delay * pro.one.high) < delayTolerance &&
                   diff(rcswitch->raw[i + 1], delay * pro.one.low) < delayTolerance) {
            // one
            code |= 1;
        } else {
            // Failed
            return false;
        }
    }
	createMessage(code, p, (rcswitch->rawlen - 1) / 2, delay);

    return true;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void rcswitchInit(void) {

	protocol_register(&rcswitch);
	protocol_set_id(rcswitch, "rcswitch");
	protocol_device_add(rcswitch, "rcswitch", "rcswitch device");
	rcswitch->devtype = CONTACT;
	rcswitch->hwtype = RF433;
	rcswitch->minrawlen = MIN_RAW_LENGTH;
	rcswitch->maxrawlen = MAX_RAW_LENGTH;
	rcswitch->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	rcswitch->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&rcswitch->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&rcswitch->options, "s", "state", OPTION_NO_VALUE, DEVICES_STATE, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");

	rcswitch->parseCode=&parseCode;
	rcswitch->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "rcswitch";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	rcswitchInit();
}
#endif
