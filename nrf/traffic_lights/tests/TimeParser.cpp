#include <stdlib.h>
#include <string.h>
#include "TimeParser.h"

// time format: HHMMSS (6 characters)
int time_parse(char *time) {

	// how many seconds, default returns error
	int seconds = 0;
	int len;
	bool iserr = false;

	// Can't do anything else if there is no data to parse
	if (time == NULL) return ERR_TIME(FLAG_TIME_LEN);

	len = strlen(time);
	// Check for length correctness but do not return yet
	if (len != 6) {
		seconds |= FLAG_TIME_LEN;
	}

	// Check that input contains only digits (ASCII characters between 0x2f and 0x3a exclusive)
	// Parse them simultaneously to integers
	int values[3] = {0, 0, 0};

	for (int i = 0; i < len; i++) {
		if (!(time[i] > '/') || !(time[i] < ':')) {
			seconds |= FLAG_TIME_SYNTAX;
			if (i < 2) seconds |= FLAG_TIME_VALUE_HOUR;
			else if (i < 4) seconds |= FLAG_TIME_VALUE_MINUTE;
			else if (i < 6) seconds |= FLAG_TIME_VALUE_SECOND;

		} else if (i < 6) {
			// Parse the integer and multiply it with ten if it is first
			values[i / 2] += (i & 1) == 0 ? (time[i] - '0') * 10 : time[i] - '0';
		}
	}
	
	if (seconds != 0) iserr = true;

	// Parse values from time string
	// values[2] = atoi(time+4); // seconds
	// time[4] = 0;
	// values[1] = atoi(time+2); // minutes
	// time[2] = 0;
	// values[0] = atoi(time); // hours

	// More than 59 seconds
	if (values[2] > 59) {
		if (iserr) {
			seconds |= FLAG_TIME_VALUE_SECOND;
		} else {
			seconds = FLAG_TIME_VALUE_SECOND;
			iserr = true;
		}
	}

	// More than 59 minutes
	if (values[1] > 59) {
		if (iserr) {
			seconds |= FLAG_TIME_VALUE_MINUTE;
		} else {
			seconds = FLAG_TIME_VALUE_MINUTE;
			iserr = true;
		}
	}

	// More than 23 hours
	if (values[0] > 23) {
		if (iserr) {
			seconds |= FLAG_TIME_VALUE_HOUR;
		} else {
			seconds = FLAG_TIME_VALUE_HOUR;
			iserr = true;
		}
	}

	// Return error flags or seconds
	return iserr ? ERR_TIME(seconds) : values[0] * 3600 + values[1] * 60 + values[2];
}
