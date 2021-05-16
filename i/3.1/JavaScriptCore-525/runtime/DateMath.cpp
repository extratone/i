/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "DateMath.h"

#include "JSNumberCell.h"
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>

#if HAVE(ERRNO_H)
#include <errno.h>
#endif

#if PLATFORM(DARWIN)
#include <notify.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

#if HAVE(STRINGS_H)
#include <strings.h>
#endif

using namespace WTF;

namespace JSC {

/* Constants */

static const double minutesPerDay = 24.0 * 60.0;
static const double secondsPerDay = 24.0 * 60.0 * 60.0;
static const double secondsPerYear = 24.0 * 60.0 * 60.0 * 365.0;

static const double usecPerSec = 1000000.0;

static const double maxUnixTime = 2145859200.0; // 12/31/2037

// Day of year for the first day of each month, where index 0 is January, and day 0 is January 1.
// First for non-leap years, then for leap years.
static const int firstDayOfMonth[2][12] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
};

static inline bool isLeapYear(int year)
{
    if (year % 4 != 0)
        return false;
    if (year % 400 == 0)
        return true;
    if (year % 100 == 0)
        return false;
    return true;
}

static inline int daysInYear(int year)
{
    return 365 + isLeapYear(year);
}

static inline double daysFrom1970ToYear(int year)
{
    // The Gregorian Calendar rules for leap years:
    // Every fourth year is a leap year.  2004, 2008, and 2012 are leap years.
    // However, every hundredth year is not a leap year.  1900 and 2100 are not leap years.
    // Every four hundred years, there's a leap year after all.  2000 and 2400 are leap years.

    static const int leapDaysBefore1971By4Rule = 1970 / 4;
    static const int excludedLeapDaysBefore1971By100Rule = 1970 / 100;
    static const int leapDaysBefore1971By400Rule = 1970 / 400;

    const double yearMinusOne = year - 1;
    const double yearsToAddBy4Rule = floor(yearMinusOne / 4.0) - leapDaysBefore1971By4Rule;
    const double yearsToExcludeBy100Rule = floor(yearMinusOne / 100.0) - excludedLeapDaysBefore1971By100Rule;
    const double yearsToAddBy400Rule = floor(yearMinusOne / 400.0) - leapDaysBefore1971By400Rule;

    return 365.0 * (year - 1970) + yearsToAddBy4Rule - yearsToExcludeBy100Rule + yearsToAddBy400Rule;
}

static inline double msToDays(double ms)
{
    return floor(ms / msPerDay);
}

static inline int msToYear(double ms)
{
    int approxYear = static_cast<int>(floor(ms / (msPerDay * 365.2425)) + 1970);
    double msFromApproxYearTo1970 = msPerDay * daysFrom1970ToYear(approxYear);
    if (msFromApproxYearTo1970 > ms)
        return approxYear - 1;
    if (msFromApproxYearTo1970 + msPerDay * daysInYear(approxYear) <= ms)
        return approxYear + 1;
    return approxYear;
}

static inline int dayInYear(double ms, int year)
{
    return static_cast<int>(msToDays(ms) - daysFrom1970ToYear(year));
}

static inline double msToMilliseconds(double ms)
{
    double result = fmod(ms, msPerDay);
    if (result < 0)
        result += msPerDay;
    return result;
}

// 0: Sunday, 1: Monday, etc.
static inline int msToWeekDay(double ms)
{
    int wd = (static_cast<int>(msToDays(ms)) + 4) % 7;
    if (wd < 0)
        wd += 7;
    return wd;
}

static inline int msToSeconds(double ms)
{
    double result = fmod(floor(ms / msPerSecond), secondsPerMinute);
    if (result < 0)
        result += secondsPerMinute;
    return static_cast<int>(result);
}

static inline int msToMinutes(double ms)
{
    double result = fmod(floor(ms / msPerMinute), minutesPerHour);
    if (result < 0)
        result += minutesPerHour;
    return static_cast<int>(result);
}

static inline int msToHours(double ms)
{
    double result = fmod(floor(ms/msPerHour), hoursPerDay);
    if (result < 0)
        result += hoursPerDay;
    return static_cast<int>(result);
}

static inline int monthFromDayInYear(int dayInYear, bool leapYear)
{
    const int d = dayInYear;
    int step;

    if (d < (step = 31))
        return 0;
    step += (leapYear ? 29 : 28);
    if (d < step)
        return 1;
    if (d < (step += 31))
        return 2;
    if (d < (step += 30))
        return 3;
    if (d < (step += 31))
        return 4;
    if (d < (step += 30))
        return 5;
    if (d < (step += 31))
        return 6;
    if (d < (step += 31))
        return 7;
    if (d < (step += 30))
        return 8;
    if (d < (step += 31))
        return 9;
    if (d < (step += 30))
        return 10;
    return 11;
}

static inline bool checkMonth(int dayInYear, int& startDayOfThisMonth, int& startDayOfNextMonth, int daysInThisMonth)
{
    startDayOfThisMonth = startDayOfNextMonth;
    startDayOfNextMonth += daysInThisMonth;
    return (dayInYear <= startDayOfNextMonth);
}

static inline int dayInMonthFromDayInYear(int dayInYear, bool leapYear)
{
    const int d = dayInYear;
    int step;
    int next = 30;

    if (d <= next)
        return d + 1;
    const int daysInFeb = (leapYear ? 29 : 28);
    if (checkMonth(d, step, next, daysInFeb))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    step = next;
    return d - step;
}

static inline int monthToDayInYear(int month, bool isLeapYear)
{
    return firstDayOfMonth[isLeapYear][month];
}

static inline double timeToMS(double hour, double min, double sec, double ms)
{
    return (((hour * minutesPerHour + min) * secondsPerMinute + sec) * msPerSecond + ms);
}

static int dateToDayInYear(int year, int month, int day)
{
    year += month / 12;

    month %= 12;
    if (month < 0) {
        month += 12;
        --year;
    }

    int yearday = static_cast<int>(floor(daysFrom1970ToYear(year)));
    int monthday = monthToDayInYear(month, isLeapYear(year));

    return yearday + monthday + day - 1;
}

double getCurrentUTCTime()
{
    return floor(getCurrentUTCTimeWithMicroseconds());
}

// Returns current time in milliseconds since 1 Jan 1970.
double getCurrentUTCTimeWithMicroseconds()
{
    return currentTime() * 1000.0; 
}

void getLocalTime(const time_t* localTime, struct tm* localTM)
{
#if COMPILER(MSVC7) || COMPILER(MINGW) || PLATFORM(WIN_CE)
    *localTM = *localtime(localTime);
#elif COMPILER(MSVC)
    localtime_s(localTM, localTime);
#else
    localtime_r(localTime, localTM);
#endif
}

// There is a hard limit at 2038 that we currently do not have a workaround
// for (rdar://problem/5052975).
static inline int maximumYearForDST()
{
    return 2037;
}

static inline int minimumYearForDST()
{
    // Because of the 2038 issue (see maximumYearForDST) if the current year is
    // greater than the max year minus 27 (2010), we want to use the max year
    // minus 27 instead, to ensure there is a range of 28 years that all years
    // can map to.
    return std::min(msToYear(getCurrentUTCTime()), maximumYearForDST() - 27) ;
}

/*
 * Find an equivalent year for the one given, where equivalence is deterined by
 * the two years having the same leapness and the first day of the year, falling
 * on the same day of the week.
 *
 * This function returns a year between this current year and 2037, however this
 * function will potentially return incorrect results if the current year is after
 * 2010, (rdar://problem/5052975), if the year passed in is before 1900 or after
 * 2100, (rdar://problem/5055038).
 */
int equivalentYearForDST(int year)
{
    // It is ok if the cached year is not the current year as long as the rules
    // for DST did not change between the two years; if they did the app would need
    // to be restarted.
    static int minYear = minimumYearForDST();
    int maxYear = maximumYearForDST();

    int difference;
    if (year > maxYear)
        difference = minYear - year;
    else if (year < minYear)
        difference = maxYear - year;
    else
        return year;

    int quotient = difference / 28;
    int product = (quotient) * 28;

    year += product;
    ASSERT((year >= minYear && year <= maxYear) || (product - year == static_cast<int>(NaN)));
    return year;
}

static int32_t calculateUTCOffset()
{
    tm localt;
    memset(&localt, 0, sizeof(localt));
 
    // get the difference between this time zone and UTC on Jan 01, 2000 12:00:00 AM
    localt.tm_mday = 1;
    localt.tm_year = 100;
    time_t utcOffset = 946684800 - mktime(&localt);

    return static_cast<int32_t>(utcOffset * 1000);
}

#if PLATFORM(DARWIN)
static int32_t s_cachedUTCOffset; // In milliseconds. An assumption here is that access to an int32_t variable is atomic on platforms that take this code path.
static bool s_haveCachedUTCOffset;
static int s_notificationToken;
#endif

/*
 * Get the difference in milliseconds between this time zone and UTC (GMT)
 * NOT including DST.
 */
double getUTCOffset()
{
#if PLATFORM(DARWIN)
    if (s_haveCachedUTCOffset) {
        int notified;
        uint32_t status = notify_check(s_notificationToken, &notified);
        if (status == NOTIFY_STATUS_OK && !notified)
            return s_cachedUTCOffset;
    }
#endif

    int32_t utcOffset = calculateUTCOffset();

#if PLATFORM(DARWIN)
    // Theoretically, it is possible that several threads will be executing this code at once, in which case we will have a race condition,
    // and a newer value may be overwritten. In practice, time zones don't change that often.
    s_cachedUTCOffset = utcOffset;
#endif

    return utcOffset;
}

/*
 * Get the DST offset for the time passed in.  Takes
 * seconds (not milliseconds) and cannot handle dates before 1970
 * on some OS'
 */
static double getDSTOffsetSimple(double localTimeSeconds, double utcOffset)
{
    if (localTimeSeconds > maxUnixTime)
        localTimeSeconds = maxUnixTime;
    else if (localTimeSeconds < 0) // Go ahead a day to make localtime work (does not work with 0)
        localTimeSeconds += secondsPerDay;

    //input is UTC so we have to shift back to local time to determine DST thus the + getUTCOffset()
    double offsetTime = (localTimeSeconds * msPerSecond) + utcOffset;

    // Offset from UTC but doesn't include DST obviously
    int offsetHour =  msToHours(offsetTime);
    int offsetMinute =  msToMinutes(offsetTime);

    // FIXME: time_t has a potential problem in 2038
    time_t localTime = static_cast<time_t>(localTimeSeconds);

    tm localTM;
    getLocalTime(&localTime, &localTM);

    double diff = ((localTM.tm_hour - offsetHour) * secondsPerHour) + ((localTM.tm_min - offsetMinute) * 60);

    if (diff < 0)
        diff += secondsPerDay;

    return (diff * msPerSecond);
}

// Get the DST offset, given a time in UTC
static double getDSTOffset(double ms, double utcOffset)
{
    // On Mac OS X, the call to localtime (see getDSTOffsetSimple) will return historically accurate
    // DST information (e.g. New Zealand did not have DST from 1946 to 1974) however the JavaScript
    // standard explicitly dictates that historical information should not be considered when
    // determining DST. For this reason we shift away from years that localtime can handle but would
    // return historically accurate information.
    int year = msToYear(ms);
    int equivalentYear = equivalentYearForDST(year);
    if (year != equivalentYear) {
        bool leapYear = isLeapYear(year);
        int dayInYearLocal = dayInYear(ms, year);
        int dayInMonth = dayInMonthFromDayInYear(dayInYearLocal, leapYear);
        int month = monthFromDayInYear(dayInYearLocal, leapYear);
        int day = dateToDayInYear(equivalentYear, month, dayInMonth);
        ms = (day * msPerDay) + msToMilliseconds(ms);
    }

    return getDSTOffsetSimple(ms / msPerSecond, utcOffset);
}

double gregorianDateTimeToMS(const GregorianDateTime& t, double milliSeconds, bool inputIsUTC)
{
    int day = dateToDayInYear(t.year + 1900, t.month, t.monthDay);
    double ms = timeToMS(t.hour, t.minute, t.second, milliSeconds);
    double result = (day * msPerDay) + ms;

    if (!inputIsUTC) { // convert to UTC
        double utcOffset = getUTCOffset();
        result -= utcOffset;
        result -= getDSTOffset(result, utcOffset);
    }

    return result;
}

void msToGregorianDateTime(double ms, bool outputIsUTC, GregorianDateTime& tm)
{
    // input is UTC
    double dstOff = 0.0;
    const double utcOff = getUTCOffset();

    if (!outputIsUTC) {  // convert to local time
        dstOff = getDSTOffset(ms, utcOff);
        ms += dstOff + utcOff;
    }

    const int year = msToYear(ms);
    tm.second   =  msToSeconds(ms);
    tm.minute   =  msToMinutes(ms);
    tm.hour     =  msToHours(ms);
    tm.weekDay  =  msToWeekDay(ms);
    tm.yearDay  =  dayInYear(ms, year);
    tm.monthDay =  dayInMonthFromDayInYear(tm.yearDay, isLeapYear(year));
    tm.month    =  monthFromDayInYear(tm.yearDay, isLeapYear(year));
    tm.year     =  year - 1900;
    tm.isDST    =  dstOff != 0.0;

    tm.utcOffset = static_cast<long>((dstOff + utcOff) / msPerSecond);
    tm.timeZone = NULL;
}

void initDateMath()
{
#ifndef NDEBUG
    static bool alreadyInitialized;
    ASSERT(!alreadyInitialized++);
#endif

    equivalentYearForDST(2000); // Need to call once to initialize a static used in this function.
#if PLATFORM(DARWIN)
    // Register for a notification whenever the time zone changes.
    uint32_t status = notify_register_check("com.apple.system.timezone", &s_notificationToken);
    if (status == NOTIFY_STATUS_OK) {
        s_cachedUTCOffset = calculateUTCOffset();
        s_haveCachedUTCOffset = true;
    }
#endif
}

static inline double ymdhmsToSeconds(long year, int mon, int day, int hour, int minute, int second)
{
    double days = (day - 32075)
        + floor(1461 * (year + 4800.0 + (mon - 14) / 12) / 4)
        + 367 * (mon - 2 - (mon - 14) / 12 * 12) / 12
        - floor(3 * ((year + 4900.0 + (mon - 14) / 12) / 100) / 4)
        - 2440588;
    return ((days * hoursPerDay + hour) * minutesPerHour + minute) * secondsPerMinute + second;
}

// We follow the recommendation of RFC 2822 to consider all
// obsolete time zones not listed here equivalent to "-0000".
static const struct KnownZone {
#if !PLATFORM(WIN_OS)
    const
#endif
        char tzName[4];
    int tzOffset;
} known_zones[] = {
    { "UT", 0 },
    { "GMT", 0 },
    { "EST", -300 },
    { "EDT", -240 },
    { "CST", -360 },
    { "CDT", -300 },
    { "MST", -420 },
    { "MDT", -360 },
    { "PST", -480 },
    { "PDT", -420 }
};

inline static void skipSpacesAndComments(const char*& s)
{
    int nesting = 0;
    char ch;
    while ((ch = *s)) {
        if (!isASCIISpace(ch)) {
            if (ch == '(')
                nesting++;
            else if (ch == ')' && nesting > 0)
                nesting--;
            else if (nesting == 0)
                break;
        }
        s++;
    }
}

// returns 0-11 (Jan-Dec); -1 on failure
static int findMonth(const char* monthStr)
{
    ASSERT(monthStr);
    char needle[4];
    for (int i = 0; i < 3; ++i) {
        if (!*monthStr)
            return -1;
        needle[i] = static_cast<char>(toASCIILower(*monthStr++));
    }
    needle[3] = '\0';
    const char *haystack = "janfebmaraprmayjunjulaugsepoctnovdec";
    const char *str = strstr(haystack, needle);
    if (str) {
        int position = static_cast<int>(str - haystack);
        if (position % 3 == 0)
            return position / 3;
    }
    return -1;
}

static bool parseLong(const char* string, char** stopPosition, int base, long* result)
{
    *result = strtol(string, stopPosition, base);
    // Avoid the use of errno as it is not available on Windows CE
    if (string == *stopPosition || *result == LONG_MIN || *result == LONG_MAX)
        return false;
    return true;
}

double parseDate(const UString &date)
{
    // This parses a date in the form:
    //     Tuesday, 09-Nov-99 23:12:40 GMT
    // or
    //     Sat, 01-Jan-2000 08:00:00 GMT
    // or
    //     Sat, 01 Jan 2000 08:00:00 GMT
    // or
    //     01 Jan 99 22:00 +0100    (exceptions in rfc822/rfc2822)
    // ### non RFC formats, added for Javascript:
    //     [Wednesday] January 09 1999 23:12:40 GMT
    //     [Wednesday] January 09 23:12:40 GMT 1999
    //
    // We ignore the weekday.

    CString dateCString = date.UTF8String();
    const char *dateString = dateCString.c_str();
     
    // Skip leading space
    skipSpacesAndComments(dateString);

    long month = -1;
    const char *wordStart = dateString;
    // Check contents of first words if not number
    while (*dateString && !isASCIIDigit(*dateString)) {
        if (isASCIISpace(*dateString) || *dateString == '(') {
            if (dateString - wordStart >= 3)
                month = findMonth(wordStart);
            skipSpacesAndComments(dateString);
            wordStart = dateString;
        } else
           dateString++;
    }

    // Missing delimiter between month and day (like "January29")?
    if (month == -1 && wordStart != dateString)
        month = findMonth(wordStart);

    skipSpacesAndComments(dateString);

    if (!*dateString)
        return NaN;

    // ' 09-Nov-99 23:12:40 GMT'
    char* newPosStr;
    long day;
    if (!parseLong(dateString, &newPosStr, 10, &day))
        return NaN;
    dateString = newPosStr;

    if (!*dateString)
        return NaN;

    if (day < 0)
        return NaN;

    long year = 0;
    if (day > 31) {
        // ### where is the boundary and what happens below?
        if (*dateString != '/')
            return NaN;
        // looks like a YYYY/MM/DD date
        if (!*++dateString)
            return NaN;
        year = day;
        if (!parseLong(dateString, &newPosStr, 10, &month))
            return NaN;
        month -= 1;
        dateString = newPosStr;
        if (*dateString++ != '/' || !*dateString)
            return NaN;
        if (!parseLong(dateString, &newPosStr, 10, &day))
            return NaN;
        dateString = newPosStr;
    } else if (*dateString == '/' && month == -1) {
        dateString++;
        // This looks like a MM/DD/YYYY date, not an RFC date.
        month = day - 1; // 0-based
        if (!parseLong(dateString, &newPosStr, 10, &day))
            return NaN;
        if (day < 1 || day > 31)
            return NaN;
        dateString = newPosStr;
        if (*dateString == '/')
            dateString++;
        if (!*dateString)
            return NaN;
     } else {
        if (*dateString == '-')
            dateString++;

        skipSpacesAndComments(dateString);

        if (*dateString == ',')
            dateString++;

        if (month == -1) { // not found yet
            month = findMonth(dateString);
            if (month == -1)
                return NaN;

            while (*dateString && *dateString != '-' && *dateString != ',' && !isASCIISpace(*dateString))
                dateString++;

            if (!*dateString)
                return NaN;

            // '-99 23:12:40 GMT'
            if (*dateString != '-' && *dateString != '/' && *dateString != ',' && !isASCIISpace(*dateString))
                return NaN;
            dateString++;
        }
    }

    if (month < 0 || month > 11)
        return NaN;

    // '99 23:12:40 GMT'
    if (year <= 0 && *dateString) {
        if (!parseLong(dateString, &newPosStr, 10, &year))
            return NaN;
    }
    
    // Don't fail if the time is missing.
    long hour = 0;
    long minute = 0;
    long second = 0;
    if (!*newPosStr)
        dateString = newPosStr;
    else {
        // ' 23:12:40 GMT'
        if (!(isASCIISpace(*newPosStr) || *newPosStr == ',')) {
            if (*newPosStr != ':')
                return NaN;
            // There was no year; the number was the hour.
            year = -1;
        } else {
            // in the normal case (we parsed the year), advance to the next number
            dateString = ++newPosStr;
            skipSpacesAndComments(dateString);
        }

        parseLong(dateString, &newPosStr, 10, &hour);
        // Do not check for errno here since we want to continue
        // even if errno was set becasue we are still looking
        // for the timezone!

        // Read a number? If not, this might be a timezone name.
        if (newPosStr != dateString) {
            dateString = newPosStr;

            if (hour < 0 || hour > 23)
                return NaN;

            if (!*dateString)
                return NaN;

            // ':12:40 GMT'
            if (*dateString++ != ':')
                return NaN;

            if (!parseLong(dateString, &newPosStr, 10, &minute))
                return NaN;
            dateString = newPosStr;

            if (minute < 0 || minute > 59)
                return NaN;

            // ':40 GMT'
            if (*dateString && *dateString != ':' && !isASCIISpace(*dateString))
                return NaN;

            // seconds are optional in rfc822 + rfc2822
            if (*dateString ==':') {
                dateString++;

                if (!parseLong(dateString, &newPosStr, 10, &second))
                    return NaN;
                dateString = newPosStr;
            
                if (second < 0 || second > 59)
                    return NaN;
            }

            skipSpacesAndComments(dateString);

            if (strncasecmp(dateString, "AM", 2) == 0) {
                if (hour > 12)
                    return NaN;
                if (hour == 12)
                    hour = 0;
                dateString += 2;
                skipSpacesAndComments(dateString);
            } else if (strncasecmp(dateString, "PM", 2) == 0) {
                if (hour > 12)
                    return NaN;
                if (hour != 12)
                    hour += 12;
                dateString += 2;
                skipSpacesAndComments(dateString);
            }
        }
    }

    bool haveTZ = false;
    int offset = 0;

    // Don't fail if the time zone is missing. 
    // Some websites omit the time zone (4275206).
    if (*dateString) {
        if (strncasecmp(dateString, "GMT", 3) == 0 || strncasecmp(dateString, "UTC", 3) == 0) {
            dateString += 3;
            haveTZ = true;
        }

        if (*dateString == '+' || *dateString == '-') {
            long o;
            if (!parseLong(dateString, &newPosStr, 10, &o))
                return NaN;
            dateString = newPosStr;

            if (o < -9959 || o > 9959)
                return NaN;

            int sgn = (o < 0) ? -1 : 1;
            o = abs(o);
            if (*dateString != ':') {
                offset = ((o / 100) * 60 + (o % 100)) * sgn;
            } else { // GMT+05:00
                long o2;
                if (!parseLong(dateString, &newPosStr, 10, &o2))
                    return NaN;
                dateString = newPosStr;
                offset = (o * 60 + o2) * sgn;
            }
            haveTZ = true;
        } else {
            for (int i = 0; i < int(sizeof(known_zones) / sizeof(KnownZone)); i++) {
                if (0 == strncasecmp(dateString, known_zones[i].tzName, strlen(known_zones[i].tzName))) {
                    offset = known_zones[i].tzOffset;
                    dateString += strlen(known_zones[i].tzName);
                    haveTZ = true;
                    break;
                }
            }
        }
    }

    skipSpacesAndComments(dateString);

    if (*dateString && year == -1) {
        if (!parseLong(dateString, &newPosStr, 10, &year))
            return NaN;
        dateString = newPosStr;
    }
     
    skipSpacesAndComments(dateString);
     
    // Trailing garbage
    if (*dateString)
        return NaN;

    // Y2K: Handle 2 digit years.
    if (year >= 0 && year < 100) {
        if (year < 50)
            year += 2000;
        else
            year += 1900;
    }

    // fall back to local timezone
    if (!haveTZ) {
        GregorianDateTime t;
        t.monthDay = day;
        t.month = month;
        t.year = year - 1900;
        t.isDST = -1;
        t.second = second;
        t.minute = minute;
        t.hour = hour;

        // Use our gregorianDateTimeToMS() rather than mktime() as the latter can't handle the full year range.
        return gregorianDateTimeToMS(t, 0, false);
    }

    return (ymdhmsToSeconds(year, month + 1, day, hour, minute, second) - (offset * 60.0)) * msPerSecond;
}

double timeClip(double t)
{
    if (!isfinite(t))
        return NaN;
    if (fabs(t) > 8.64E15)
        return NaN;
    return trunc(t);
}

UString formatDate(const GregorianDateTime &t)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s %s %02d %04d",
        weekdayName[(t.weekDay + 6) % 7],
        monthName[t.month], t.monthDay, t.year + 1900);
    return buffer;
}

UString formatDateUTCVariant(const GregorianDateTime &t)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s, %02d %s %04d",
        weekdayName[(t.weekDay + 6) % 7],
        t.monthDay, monthName[t.month], t.year + 1900);
    return buffer;
}

UString formatTime(const GregorianDateTime &t, bool utc)
{
    char buffer[100];
    if (utc) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT", t.hour, t.minute, t.second);
    } else {
        int offset = abs(gmtoffset(t));
        char tzname[70];
        struct tm gtm = t;
        strftime(tzname, sizeof(tzname), "%Z", &gtm);

        if (tzname[0]) {
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT%c%02d%02d (%s)",
                t.hour, t.minute, t.second,
                gmtoffset(t) < 0 ? '-' : '+', offset / (60*60), (offset / 60) % 60, tzname);
        } else {
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT%c%02d%02d",
                t.hour, t.minute, t.second,
                gmtoffset(t) < 0 ? '-' : '+', offset / (60*60), (offset / 60) % 60);
        }
    }
    return UString(buffer);
}

} // namespace JSC
