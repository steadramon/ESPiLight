/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _PROTOCOL_RCSWITCH_H_
#define _PROTOCOL_RCSWITCH_H_

#include "../protocol.h"

struct protocol_t *rcswitch;

typedef struct HighLow {
        uint8_t high;
        uint8_t low;
} HighLow_t;

typedef struct rcproto {
        /** base pulse length in microseconds, e.g. 350 */
        uint16_t pulseLength;

        HighLow_t syncFactor;
        HighLow_t zero;
        HighLow_t one;

        /**
         * If true, interchange high and low logic levels in all transmissions.
         *
         * By default, RCSwitch assumes that any signals it sends or receives
         * can be broken down into pulses which start with a high signal level,
         * followed by a a low signal level. This is e.g. the case for the
         * popular PT 2260 encoder chip, and thus many switches out there.
         *
         * But some devices do it the other way around, and start with a low
         * signal level, followed by a high signal level, e.g. the HT6P20B. To
         * accommodate this, one can set invertedSignal to true, which causes
         * RCSwitch to change how it interprets any HighLow struct FOO: It will
         * then assume transmissions start with a low signal lasting
         * FOO.high*pulseLength microseconds, followed by a high signal lasting
         * FOO.low*pulseLength microseconds.
         */
        bool invertedSignal;
} rcproto;

void rcswitchInit(void);

#endif
