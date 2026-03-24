/*
 * ss_report.h -- Report generation interface.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef SS_REPORT_H
#define SS_REPORT_H

#include "ss_types.h"

/* Emit the text report to Valgrind's user message channel */
void ss_report_text(void);

/* Write the JSON report to a file called bridge.out.json */
void ss_report_json(void);

#endif /* SS_REPORT_H */
