/* blzr_mecer.c - Subdriver for Mecer UPSes
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "main.h"
#include "blzr.h"
#include "blzr_blazer-common.h"

#include "blzr_mecer.h"

#define MECER_VERSION "Mecer 0.01"

/* blzr2nut lookup table */
static item_t	mecer_blzr2nut[] = {

	/*
	 * > [Q1\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	{ "input.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	1,	5,	"%.1f",	0,	NULL },
	{ "input.voltage.fault",	0,	NULL,	"Q1\r",	"",	47,	'(',	"",	7,	11,	"%.1f",	0,	NULL },
	{ "output.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	13,	17,	"%.1f",	0,	NULL },
	{ "ups.load",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	19,	21,	"%.0f",	0,	NULL },
	{ "input.frequency",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	23,	26,	"%.1f",	0,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL },
	{ "ups.temperature",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	33,	36,	"%.1f",	0,	NULL },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	BLZR_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	BLZR_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	40,	40,	NULL,	BLZR_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	BLZR_FLAG_STATIC,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	BLZR_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	0,			blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	BLZR_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			blazer_process_status_bits },	/* Beeper status */

	/*
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "input.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.0f",	BLZR_FLAG_STATIC,	NULL },
	{ "input.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	BLZR_FLAG_STATIC,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	BLZR_FLAG_STATIC,	NULL },
	{ "input.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.0f",	BLZR_FLAG_STATIC,	NULL },

	/*
	 * > [I\r]
	 * < [#-------------   ------     VT12046Q  \r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */

	{ "device.mfr",			0,	NULL,	"I\r",	"",	39,	'#',	"",	1,	15,	"%s",	BLZR_FLAG_STATIC | BLZR_FLAG_TRIM,	NULL },
	{ "device.model",		0,	NULL,	"I\r",	"",	39,	'#',	"",	17,	26,	"%s",	BLZR_FLAG_STATIC | BLZR_FLAG_TRIM,	NULL },
	{ "ups.firmware",		0,	NULL,	"I\r",	"",	39,	'#',	"",	28,	37,	"%s",	BLZR_FLAG_STATIC | BLZR_FLAG_TRIM,	NULL },

	/* Instant commands
	 * The UPS will reply '(ACK\r' in case of success, '(NAK\r' if the command is rejected or invalid */

	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },
	{ "test.battery.start",		0,	NULL,	"T%02d\r",	"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	blazer_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	5,	'(',	"",	1,	3,	NULL,	BLZR_FLAG_CMD,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	BLZR_FLAG_ABSENT | BLZR_FLAG_SETVAR | BLZR_FLAG_RANGE,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	BLZR_FLAG_ABSENT | BLZR_FLAG_SETVAR | BLZR_FLAG_RANGE,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL }
};

/* Testing table */
#ifdef TESTING
static testing_t	mecer_testing[] = {
	{ "Q1\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r" },
	{ "F\r",	"#230.0 000 024.0 50.0\r" },
	{ "I\r",	"#NOT_A_LIVE_UPS  TESTING    TESTING   \r" },
	{ "Q\r",	"(ACK\r" },
	{ "S03\r",	"(NAK\r" },
	{ "C\r",	"(NAK\r" },
	{ "S02R0005\r",	"(ACK\r" },
	{ "S.5R0000\r",	"(ACK\r" },
	{ "T04\r",	"(NAK\r" },
	{ "TL\r",	"(ACK\r" },
	{ "T\r",	"(NAK\r" },
	{ "CT\r",	"(ACK\r" },
	{ NULL }
};
#endif	/* TESTING */

/* Subdriver-specific initups */
static void	mecer_initups(void)
{

	blazer_initups(mecer_blzr2nut);

}

/* Subdriver interface */
subdriver_t	mecer_subdriver = {
	MECER_VERSION,
	blazer_claim,
	mecer_blzr2nut,
	mecer_initups,
	NULL,
	blazer_makevartable,
	"ACK",
	"(NAK\r",
#ifdef TESTING
	mecer_testing,
#endif	/* TESTING */
};
