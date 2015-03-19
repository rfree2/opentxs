/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#include <opentxs/core/util/Common.hpp>
#include <opentxs/core/util/StringUtils.hpp>
#include <opentxs/core/util/Assert.hpp>

#include <opentxs/core/String.hpp>

#ifdef ANDROID
#include <time64.h>
#endif

namespace opentxs
{

// If 10 is passed in, then 11 will be allocated,
// then the data is copied, and then the result[10] (11th element)
// is set to 0. This way the original 10-length string is untouched.
//
char* str_dup2(const char* str, uint32_t length) // length doesn't/shouldn't
                                                 // include the byte for the
                                                 // terminating 0.
{
    char* str_new = new char[length + 1]; // CREATE EXTRA BYTE OF SPACE FOR \0
                                          // (NOT PART OF LENGTH)
    OT_ASSERT(nullptr != str_new);

#ifdef _WIN32
    strncpy_s(str_new, length + 1, str, length);
#else
    strncpy(str_new, str, length);
#endif

    // INITIALIZE EXTRA BYTE OF SPACE
    //
    // If length is 10, then buffer is created with 11 elements,
    // indexed from 0 (first element) through 10 (11th element).
    //
    // Therefore str_new[length==10] is the 11th element, which was
    // the extra one created on our buffer, to store the \0 null terminator.
    //
    // This way I know I'm never cutting off data that was in the string itself.
    // Rather, I am only setting to 0 an EXTRA byte that I created myself, AFTER
    // the string's length itself.
    //
    str_new[length] = '\0';

    return str_new;
}

} // namespace opentxs

std::string formatTimestamp(time64_t tt)
{
    struct tm tm;
    time_t t = tt;
    char buf[255];

    strftime(buf, sizeof(buf), "%FT%T", gmtime_r(&t, &tm));
    return std::string(buf);
}

std::string formatInt(int32_t tt)
{
    opentxs::String temp;

    temp.Format("%" PRId32 "", tt);

    return temp.Get();
}

std::string formatUint(uint32_t tt)
{
    opentxs::String temp;

    temp.Format("%" PRIu32 "", tt);

    return temp.Get();
}

std::string formatChar(char tt)
{
    std::string temp;

    temp += tt;

    return temp;
}

std::string formatLong(int64_t tt)
{
    opentxs::String temp;

    temp.Format("%" PRId64 "", tt);

    return temp.Get();
}

std::string formatUlong(uint64_t tt)
{
    opentxs::String temp;

    temp.Format("%" PRIu64 "", tt);

    return temp.Get();
}

std::string formatBool(bool tt)
{
    return tt ? "true" : "false";
}

std::string getTimestamp()
{
    return formatTimestamp(time(NULL));
}

#ifdef _MSC_VER
char * strptime(const char *s, const char *format, struct tm *tm);
#endif

time64_t parseTimestamp(std::string extendedTimeString)
{
    struct tm tm;
    if (!strptime(extendedTimeString.c_str(), "%Y-%m-%dT%H:%M:%S", &tm)) {
        return 0;
    }

#ifdef ANDROID
    time64_t t = timegm64(&tm);
#elif defined(_WIN32)
	time_t time = mktime(&tm);
	tm = *gmtime(&time);
	time_t t = mktime(&tm);
#else
    time_t t = timegm(&tm);
#endif
    if (t == -1) return 0;
    return t;
}

//strptime() from
//http://stackoverflow.com/questions/667250/strptime-in-windows

#ifdef _MSC_VER
const char * strp_weekdays[] =
{ "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };
const char * strp_monthnames[] =
{ "january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december" };
bool strp_atoi(const char * & s, int & result, int low, int high, int offset)
{
	bool worked = false;
	char * end;
	unsigned long num = strtoul(s, &end, 10);
	if (num >= (unsigned long)low && num <= (unsigned long)high)
	{
		result = (int)(num + offset);
		s = end;
		worked = true;
	}
	return worked;
}
char * strptime(const char *s, const char *format, struct tm *tm)
{
	bool working = true;
	while (working && *format && *s)
	{
		switch (*format)
		{
		case '%':
		{
					++format;
					switch (*format)
					{
					case 'a':
					case 'A': // weekday name
						tm->tm_wday = -1;
						working = false;
						for (size_t i = 0; i < 7; ++i)
						{
							size_t len = strlen(strp_weekdays[i]);
							if (!strnicmp(strp_weekdays[i], s, len))
							{
								tm->tm_wday = i;
								s += len;
								working = true;
								break;
							}
							else if (!strnicmp(strp_weekdays[i], s, 3))
							{
								tm->tm_wday = i;
								s += 3;
								working = true;
								break;
							}
						}
						break;
					case 'b':
					case 'B':
					case 'h': // month name
						tm->tm_mon = -1;
						working = false;
						for (size_t i = 0; i < 12; ++i)
						{
							size_t len = strlen(strp_monthnames[i]);
							if (!strnicmp(strp_monthnames[i], s, len))
							{
								tm->tm_mon = i;
								s += len;
								working = true;
								break;
							}
							else if (!strnicmp(strp_monthnames[i], s, 3))
							{
								tm->tm_mon = i;
								s += 3;
								working = true;
								break;
							}
						}
						break;
					case 'd':
					case 'e': // day of month number
						working = strp_atoi(s, tm->tm_mday, 1, 31, 0);
						break;
					case 'D': // %m/%d/%y
					{
								  const char * s_save = s;
								  working = strp_atoi(s, tm->tm_mon, 1, 12, -1);
								  if (working && *s == '/')
								  {
									  ++s;
									  working = strp_atoi(s, tm->tm_mday, 1, 31, 0);
									  if (working && *s == '/')
									  {
										  ++s;
										  working = strp_atoi(s, tm->tm_year, 0, 99, 0);
										  if (working && tm->tm_year < 69)
											  tm->tm_year += 100;
									  }
								  }
								  if (!working)
									  s = s_save;
					}
						break;
					case 'H': // hour
						working = strp_atoi(s, tm->tm_hour, 0, 23, 0);
						break;
					case 'I': // hour 12-hour clock
						working = strp_atoi(s, tm->tm_hour, 1, 12, 0);
						break;
					case 'j': // day number of year
						working = strp_atoi(s, tm->tm_yday, 1, 366, -1);
						break;
					case 'm': // month number
						working = strp_atoi(s, tm->tm_mon, 1, 12, -1);
						break;
					case 'M': // minute
						working = strp_atoi(s, tm->tm_min, 0, 59, 0);
						break;
					case 'n': // arbitrary whitespace
					case 't':
						while (isspace((int)*s))
							++s;
						break;
					case 'p': // am / pm
						if (!strnicmp(s, "am", 2))
						{ // the hour will be 1 -> 12 maps to 12 am, 1 am .. 11 am, 12 noon 12 pm .. 11 pm
							if (tm->tm_hour == 12) // 12 am == 00 hours
								tm->tm_hour = 0;
						}
						else if (!strnicmp(s, "pm", 2))
						{
							if (tm->tm_hour < 12) // 12 pm == 12 hours
								tm->tm_hour += 12; // 1 pm -> 13 hours, 11 pm -> 23 hours
						}
						else
							working = false;
						break;
					case 'r': // 12 hour clock %I:%M:%S %p
					{
								  const char * s_save = s;
								  working = strp_atoi(s, tm->tm_hour, 1, 12, 0);
								  if (working && *s == ':')
								  {
									  ++s;
									  working = strp_atoi(s, tm->tm_min, 0, 59, 0);
									  if (working && *s == ':')
									  {
										  ++s;
										  working = strp_atoi(s, tm->tm_sec, 0, 60, 0);
										  if (working && isspace((int)*s))
										  {
											  ++s;
											  while (isspace((int)*s))
												  ++s;
											  if (!strnicmp(s, "am", 2))
											  { // the hour will be 1 -> 12 maps to 12 am, 1 am .. 11 am, 12 noon 12 pm .. 11 pm
												  if (tm->tm_hour == 12) // 12 am == 00 hours
													  tm->tm_hour = 0;
											  }
											  else if (!strnicmp(s, "pm", 2))
											  {
												  if (tm->tm_hour < 12) // 12 pm == 12 hours
													  tm->tm_hour += 12; // 1 pm -> 13 hours, 11 pm -> 23 hours
											  }
											  else
												  working = false;
										  }
									  }
								  }
								  if (!working)
									  s = s_save;
					}
						break;
					case 'R': // %H:%M
					{
								  const char * s_save = s;
								  working = strp_atoi(s, tm->tm_hour, 0, 23, 0);
								  if (working && *s == ':')
								  {
									  ++s;
									  working = strp_atoi(s, tm->tm_min, 0, 59, 0);
								  }
								  if (!working)
									  s = s_save;
					}
						break;
					case 'S': // seconds
						working = strp_atoi(s, tm->tm_sec, 0, 60, 0);
						break;
					case 'T': // %H:%M:%S
					{
								  const char * s_save = s;
								  working = strp_atoi(s, tm->tm_hour, 0, 23, 0);
								  if (working && *s == ':')
								  {
									  ++s;
									  working = strp_atoi(s, tm->tm_min, 0, 59, 0);
									  if (working && *s == ':')
									  {
										  ++s;
										  working = strp_atoi(s, tm->tm_sec, 0, 60, 0);
									  }
								  }
								  if (!working)
									  s = s_save;
					}
						break;
					case 'w': // weekday number 0->6 sunday->saturday
						working = strp_atoi(s, tm->tm_wday, 0, 6, 0);
						break;
					case 'Y': // year
						working = strp_atoi(s, tm->tm_year, 1900, 65535, -1900);
						break;
					case 'y': // 2-digit year
						working = strp_atoi(s, tm->tm_year, 0, 99, 0);
						if (working && tm->tm_year < 69)
							tm->tm_year += 100;
						break;
					case '%': // escaped
						if (*s != '%')
							working = false;
						++s;
						break;
					default:
						working = false;
					}
		}
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case '\v':
			// zero or more whitespaces:
			while (isspace((int)*s))
				++s;
			break;
		default:
			// match character
			if (*s != *format)
				working = false;
			else
				++s;
			break;
		}
		++format;
	}
	return (working ? (char *)s : 0);
}
#endif // _MSC_VER
