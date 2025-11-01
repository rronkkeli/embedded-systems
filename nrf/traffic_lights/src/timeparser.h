#ifndef TIMEPARSER_H
#define TIMEPARSER_H

// Return codes and flags to signal multiple errors so the user can try to correct all of them

// Return value for correctly parsed time string (Unused)
#define TIME_OK                   0

// Signals that the parsed time string has incorrect length
#define FLAG_TIME_LEN             1
// Signals that the parsed time string has invalid characters
#define FLAG_TIME_SYNTAX          2
// Signals that the parsed time string has invalid hour field
#define FLAG_TIME_VALUE_HOUR      4
// Signals that the parsed time string has invalid minute field
#define FLAG_TIME_VALUE_MINUTE    8
// Signals that the parsed time string has invalid seconds field
#define FLAG_TIME_VALUE_SECOND   16

// Convert OR'd flags into error. Keeps syntax and intent clear.
#define ERR_TIME(flags) (-(flags))

int time_parse(char *time);

#endif
