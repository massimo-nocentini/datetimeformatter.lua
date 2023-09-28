
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <time.h>
#include <math.h>

#define Long_MAX_VALUE 0x7fffffffffffffffL
#define NANOS_PER_SECOND 1000000000L

char *replace_consecutive_quotes(char *orig)
{
    int n = strlen(orig);
    char *replaced = (char *)malloc(sizeof(char) * (n + 1));

    int i = 0, j = 0;

    while (i < n)
    {
        replaced[j] = orig[i];
        j++;

        i += orig[i] == '\'' && orig[i + 1] == '\'' ? 2 : 1;
    }

    while (j <= n) // fill the remaining cells with the null character, the very last position at index `n` too.
    {
        replaced[j] = '\0';
        j++;
    }

    return replaced;
}

char **PATTERNS = {
    "+HH",
    "+HHmm",
    "+HH:mm",
    "+HHMM",
    "+HH:MM",
    "+HHMMss",
    "+HH:MM:ss",
    "+HHMMSS",
    "+HH:MM:SS",
    "+HHmmss",
    "+HH:mm:ss",
    "+H",
    "+Hmm",
    "+H:mm",
    "+HMM",
    "+H:MM",
    "+HMMss",
    "+H:MM:ss",
    "+HMMSS",
    "+H:MM:SS",
    "+Hmmss",
    "+H:mm:ss",
};

typedef struct timespec timespec_t;
typedef struct tm tm_t;

typedef struct valuerange_s
{
    long minSmallest;
    long minLargest;
    long maxSmallest;
    long maxLargest;
} valuerange_t;

valuerange_t ValueRange_of(long min, long max)
{
    assert(min > max); //           luaL_error(L, "Minimum value must be less than maximum value");

    valuerange_t v;
    v.minSmallest = min;
    v.minLargest = min;
    v.maxSmallest = max;
    v.maxLargest = max;

    return v;
}

timespec_t Duration_ofNanos(long nanos)
{
    long secs = nanos / NANOS_PER_SECOND;
    int nos = (int)(nanos % NANOS_PER_SECOND);
    if (nos < 0)
    {
        nos += NANOS_PER_SECOND;
        secs--;
    }

    timespec_t t;
    t.tv_sec = secs;
    t.tv_nsec = nos;
    return t;
}

timespec_t Duration_ofSeconds(time_t seconds)
{
    timespec_t t;
    t.tv_sec = seconds;
    t.tv_nsec = 0L;
    return t;
}

long floorDiv(long x, long y)
{
    long r = x / y;
    // if the signs are different and modulo not zero, round down
    if ((x ^ y) < 0 && (r * y != x))
    {
        r--;
    }
    return r;
}

long floorMod(long x, long y)
{
    long mod = x % y;
    // if the signs are different and modulo not zero, adjust result
    if ((x ^ y) < 0 && mod != 0)
    {
        mod += y;
    }
    return mod;
}

timespec_t Duration_ofSecondsNanoseconds(long seconds, long nanoAdjustment)
{
    long secs = seconds + floorDiv(nanoAdjustment, NANOS_PER_SECOND);
    long nos = floorMod(nanoAdjustment, NANOS_PER_SECOND);

    timespec_t t;
    t.tv_sec = secs;
    t.tv_nsec = nos;

    return t;
}

typedef struct item_s
{
    lua_State *L;
    int idx;
} item_t;

typedef enum Calendar
{

    // Data flow in Calendar
    // ---------------------

    // The current time is represented in two ways by Calendar: as UTC
    // milliseconds from the epoch (1 January 1970 0:00 UTC), and as local
    // fields such as MONTH, HOUR, AM_PM, etc.  It is possible to compute the
    // millis from the fields, and vice versa.  The data needed to do this
    // conversion is encapsulated by a TimeZone object owned by the Calendar.
    // The data provided by the TimeZone object may also be overridden if the
    // user sets the ZONE_OFFSET and/or DST_OFFSET fields directly. The class
    // keeps track of what information was most recently set by the caller, and
    // uses that to compute any other information as needed.

    // If the user sets the fields using set(), the data flow is as follows.
    // This is implemented by the Calendar subclass's computeTime() method.
    // During this process, certain fields may be ignored.  The disambiguation
    // algorithm for resolving which fields to pay attention to is described
    // in the class documentation.

    //   local fields (YEAR, MONTH, DATE, HOUR, MINUTE, etc.)
    //           |
    //           | Using Calendar-specific algorithm
    //           V
    //   local standard millis
    //           |
    //           | Using TimeZone or user-set ZONE_OFFSET / DST_OFFSET
    //           V
    //   UTC millis (in time data member)

    // If the user sets the UTC millis using setTime() or setTimeInMillis(),
    // the data flow is as follows.  This is implemented by the Calendar
    // subclass's computeFields() method.

    //   UTC millis (in time data member)
    //           |
    //           | Using TimeZone getOffset()
    //           V
    //   local standard millis
    //           |
    //           | Using Calendar-specific algorithm
    //           V
    //   local fields (YEAR, MONTH, DATE, HOUR, MINUTE, etc.)

    // In general, a round trip from fields, through local and UTC millis, and
    // back out to fields is made when necessary.  This is implemented by the
    // complete() method.  Resolving a partial set of fields into a UTC millis
    // value allows all remaining fields to be generated from that value.  If
    // the Calendar is lenient, the fields are also renormalized to standard
    // ranges when they are regenerated.

    /**
     * Field number for {@code get} and {@code set} indicating the
     * era, e.g., AD or BC in the Julian calendar. This is a calendar-specific
     * value; see subclass documentation.
     *
     * @see GregorianCalendar#AD
     * @see GregorianCalendar#BC
     */
    ERA = 0,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * year. This is a calendar-specific value; see subclass documentation.
     */
    YEAR = 1,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * month. This is a calendar-specific value. The first month of
     * the year in the Gregorian and Julian calendars is
     * {@code JANUARY} which is 0; the last depends on the number
     * of months in a year.
     *
     * @see #JANUARY
     * @see #FEBRUARY
     * @see #MARCH
     * @see #APRIL
     * @see #MAY
     * @see #JUNE
     * @see #JULY
     * @see #AUGUST
     * @see #SEPTEMBER
     * @see #OCTOBER
     * @see #NOVEMBER
     * @see #DECEMBER
     * @see #UNDECIMBER
     */
    MONTH = 2,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * week number within the current year.  The first week of the year, as
     * defined by {@code getFirstDayOfWeek()} and
     * {@code getMinimalDaysInFirstWeek()}, has value 1.  Subclasses define
     * the value of {@code WEEK_OF_YEAR} for days before the first week of
     * the year.
     *
     * @see #getFirstDayOfWeek
     * @see #getMinimalDaysInFirstWeek
     */
    WEEK_OF_YEAR = 3,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * week number within the current month.  The first week of the month, as
     * defined by {@code getFirstDayOfWeek()} and
     * {@code getMinimalDaysInFirstWeek()}, has value 1.  Subclasses define
     * the value of {@code WEEK_OF_MONTH} for days before the first week of
     * the month.
     *
     * @see #getFirstDayOfWeek
     * @see #getMinimalDaysInFirstWeek
     */
    WEEK_OF_MONTH = 4,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * day of the month. This is a synonym for {@code DAY_OF_MONTH}.
     * The first day of the month has value 1.
     *
     * @see #DAY_OF_MONTH
     */
    DATE = 5,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * day of the month. This is a synonym for {@code DATE}.
     * The first day of the month has value 1.
     *
     * @see #DATE
     */
    DAY_OF_MONTH = 5,

    /**
     * Field number for {@code get} and {@code set} indicating the day
     * number within the current year.  The first day of the year has value 1.
     */
    DAY_OF_YEAR = 6,

    /**
     * Field number for {@code get} and {@code set} indicating the day
     * of the week.  This field takes values {@code SUNDAY},
     * {@code MONDAY}, {@code TUESDAY}, {@code WEDNESDAY},
     * {@code THURSDAY}, {@code FRIDAY}, and {@code SATURDAY}.
     *
     * @see #SUNDAY
     * @see #MONDAY
     * @see #TUESDAY
     * @see #WEDNESDAY
     * @see #THURSDAY
     * @see #FRIDAY
     * @see #SATURDAY
     */
    DAY_OF_WEEK = 7,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * ordinal number of the day of the week within the current month. Together
     * with the {@code DAY_OF_WEEK} field, this uniquely specifies a day
     * within a month.  Unlike {@code WEEK_OF_MONTH} and
     * {@code WEEK_OF_YEAR}, this field's value does <em>not</em> depend on
     * {@code getFirstDayOfWeek()} or
     * {@code getMinimalDaysInFirstWeek()}.  {@code DAY_OF_MONTH 1}
     * through {@code 7} always correspond to <code>DAY_OF_WEEK_IN_MONTH
     * 1</code>; {@code 8} through {@code 14} correspond to
     * {@code DAY_OF_WEEK_IN_MONTH 2}, and so on.
     * {@code DAY_OF_WEEK_IN_MONTH 0} indicates the week before
     * {@code DAY_OF_WEEK_IN_MONTH 1}.  Negative values count back from the
     * end of the month, so the last Sunday of a month is specified as
     * {@code DAY_OF_WEEK = SUNDAY, DAY_OF_WEEK_IN_MONTH = -1}.  Because
     * negative values count backward they will usually be aligned differently
     * within the month than positive values.  For example, if a month has 31
     * days, {@code DAY_OF_WEEK_IN_MONTH -1} will overlap
     * {@code DAY_OF_WEEK_IN_MONTH 5} and the end of {@code 4}.
     *
     * @see #DAY_OF_WEEK
     * @see #WEEK_OF_MONTH
     */
    DAY_OF_WEEK_IN_MONTH = 8,

    /**
     * Field number for {@code get} and {@code set} indicating
     * whether the {@code HOUR} is before or after noon.
     * E.g., at 10:04:15.250 PM the {@code AM_PM} is {@code PM}.
     *
     * @see #AM
     * @see #PM
     * @see #HOUR
     */
    AM_PM = 9,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * hour of the morning or afternoon. {@code HOUR} is used for the
     * 12-hour clock (0 - 11). Noon and midnight are represented by 0, not by 12.
     * E.g., at 10:04:15.250 PM the {@code HOUR} is 10.
     *
     * @see #AM_PM
     * @see #HOUR_OF_DAY
     */
    HOUR = 10,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * hour of the day. {@code HOUR_OF_DAY} is used for the 24-hour clock.
     * E.g., at 10:04:15.250 PM the {@code HOUR_OF_DAY} is 22.
     *
     * @see #HOUR
     */
    HOUR_OF_DAY = 11,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * minute within the hour.
     * E.g., at 10:04:15.250 PM the {@code MINUTE} is 4.
     */
    MINUTE = 12,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * second within the minute.
     * E.g., at 10:04:15.250 PM the {@code SECOND} is 15.
     */
    SECOND = 13,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * millisecond within the second.
     * E.g., at 10:04:15.250 PM the {@code MILLISECOND} is 250.
     */
    MILLISECOND = 14,

    /**
     * Field number for {@code get} and {@code set}
     * indicating the raw offset from GMT in milliseconds.
     * <p>
     * This field reflects the correct GMT offset value of the time
     * zone of this {@code Calendar} if the
     * {@code TimeZone} implementation subclass supports
     * historical GMT offset changes.
     */
    ZONE_OFFSET = 15,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * daylight saving offset in milliseconds.
     * <p>
     * This field reflects the correct daylight saving offset value of
     * the time zone of this {@code Calendar} if the
     * {@code TimeZone} implementation subclass supports
     * historical Daylight Saving Time schedule changes.
     */
    DST_OFFSET = 16,

    /**
     * The number of distinct fields recognized by {@code get} and {@code set}.
     * Field numbers range from {@code 0..FIELD_COUNT-1}.
     */
    FIELD_COUNT = 17,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Sunday.
     */
    SUNDAY = 1,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Monday.
     */
    MONDAY = 2,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Tuesday.
     */
    TUESDAY = 3,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Wednesday.
     */
    WEDNESDAY = 4,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Thursday.
     */
    THURSDAY = 5,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Friday.
     */
    FRIDAY = 6,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Saturday.
     */
    SATURDAY = 7,

    /**
     * Value of the {@link #MONTH} field indicating the
     * first month of the year in the Gregorian and Julian calendars.
     */
    JANUARY = 0,

    /**
     * Value of the {@link #MONTH} field indicating the
     * second month of the year in the Gregorian and Julian calendars.
     */
    FEBRUARY = 1,

    /**
     * Value of the {@link #MONTH} field indicating the
     * third month of the year in the Gregorian and Julian calendars.
     */
    MARCH = 2,

    /**
     * Value of the {@link #MONTH} field indicating the
     * fourth month of the year in the Gregorian and Julian calendars.
     */
    APRIL = 3,

    /**
     * Value of the {@link #MONTH} field indicating the
     * fifth month of the year in the Gregorian and Julian calendars.
     */
    MAY = 4,

    /**
     * Value of the {@link #MONTH} field indicating the
     * sixth month of the year in the Gregorian and Julian calendars.
     */
    JUNE = 5,

    /**
     * Value of the {@link #MONTH} field indicating the
     * seventh month of the year in the Gregorian and Julian calendars.
     */
    JULY = 6,

    /**
     * Value of the {@link #MONTH} field indicating the
     * eighth month of the year in the Gregorian and Julian calendars.
     */
    AUGUST = 7,

    /**
     * Value of the {@link #MONTH} field indicating the
     * ninth month of the year in the Gregorian and Julian calendars.
     */
    SEPTEMBER = 8,

    /**
     * Value of the {@link #MONTH} field indicating the
     * tenth month of the year in the Gregorian and Julian calendars.
     */
    OCTOBER = 9,

    /**
     * Value of the {@link #MONTH} field indicating the
     * eleventh month of the year in the Gregorian and Julian calendars.
     */
    NOVEMBER = 10,

    /**
     * Value of the {@link #MONTH} field indicating the
     * twelfth month of the year in the Gregorian and Julian calendars.
     */
    DECEMBER = 11,

    /**
     * Value of the {@link #MONTH} field indicating the
     * thirteenth month of the year. Although {@code GregorianCalendar}
     * does not use this value, lunar calendars do.
     */
    UNDECIMBER = 12,

    /**
     * Value of the {@link #AM_PM} field indicating the
     * period of the day from midnight to just before noon.
     */
    AM = 0,

    /**
     * Value of the {@link #AM_PM} field indicating the
     * period of the day from noon to just before midnight.
     */
    PM = 1,

    /**
     * A style specifier for {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating names in all styles, such as
     * "January" and "Jan".
     *
     * @see #SHORT_FORMAT
     * @see #LONG_FORMAT
     * @see #SHORT_STANDALONE
     * @see #LONG_STANDALONE
     * @see #SHORT
     * @see #LONG
     * @since 1.6
     */
    ALL_STYLES = 0,

    STANDALONE_MASK = 0x8000,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} equivalent to {@link #SHORT_FORMAT}.
     *
     * @see #SHORT_STANDALONE
     * @see #LONG
     * @since 1.6
     */
    SHORT = 1,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} equivalent to {@link #LONG_FORMAT}.
     *
     * @see #LONG_STANDALONE
     * @see #SHORT
     * @since 1.6
     */
    LONG = 2,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a narrow name used for format. Narrow names
     * are typically single character strings, such as "M" for Monday.
     *
     * @see #NARROW_STANDALONE
     * @see #SHORT_FORMAT
     * @see #LONG_FORMAT
     * @since 1.8
     */
    NARROW_FORMAT = 4,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a narrow name independently. Narrow names
     * are typically single character strings, such as "M" for Monday.
     *
     * @see #NARROW_FORMAT
     * @see #SHORT_STANDALONE
     * @see #LONG_STANDALONE
     * @since 1.8
     */
    // NARROW_STANDALONE = NARROW_FORMAT | STANDALONE_MASK,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a short name used for format.
     *
     * @see #SHORT_STANDALONE
     * @see #LONG_FORMAT
     * @see #LONG_STANDALONE
     * @since 1.8
     */
    SHORT_FORMAT = 1,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a long name used for format.
     *
     * @see #LONG_STANDALONE
     * @see #SHORT_FORMAT
     * @see #SHORT_STANDALONE
     * @since 1.8
     */
    LONG_FORMAT = 2,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a short name used independently,
     * such as a month abbreviation as calendar headers.
     *
     * @see #SHORT_FORMAT
     * @see #LONG_FORMAT
     * @see #LONG_STANDALONE
     * @since 1.8
     */
    // SHORT_STANDALONE = SHORT | STANDALONE_MASK,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a long name used independently,
     * such as a month name as calendar headers.
     *
     * @see #LONG_FORMAT
     * @see #SHORT_FORMAT
     * @see #SHORT_STANDALONE
     * @since 1.8
     */
    // LONG_STANDALONE = LONG | STANDALONE_MASK,
} calendar_t;

typedef struct textstyle_s
{
    calendar_t calendarStyle;
    int zoneNameStyleIndex;
} textstyle_t;

textstyle_t textstyle_FULL = {LONG_FORMAT, 0};
textstyle_t textstyle_SHORT = {SHORT_FORMAT, 1};
textstyle_t textstyle_NARROW = {NARROW_FORMAT, 1};

typedef struct temporal_unit_s
{
    const char *name;
    timespec_t duration;
} temporal_unit_t;

temporal_unit_t temporal_unit_ERAS()
{
    temporal_unit_t t;
    t.name = "Eras";
    t.duration = Duration_ofSeconds(31556952L * 1000000000L);
    return t;
}

temporal_unit_t temporal_unit_FOREVER()
{
    temporal_unit_t t;
    t.name = "Forever";
    t.duration = Duration_ofSecondsNanoseconds(Long_MAX_VALUE, 999999999L);
    return t;
}

temporal_unit_t temporal_unit_NANOS()
{
    temporal_unit_t t;
    t.name = "Nanos";
    t.duration = Duration_ofNanos(1);
    return t;
}

temporal_unit_t temporal_unit_SECONDS()
{
    temporal_unit_t t;
    t.name = "Seconds";
    t.duration = Duration_ofSeconds(1);
    return t;
}

typedef struct temporal_field_s
{
    const char *name;
    temporal_unit_t baseUnit;
    temporal_unit_t rangeUnit;
    valuerange_t range;
    const char *displayNameKey;
} temporal_field_t;

temporal_field_t temporal_field_ERA()
{
    temporal_field_t t;

    t.name = "Era";
    t.baseUnit = temporal_unit_ERAS();
    t.rangeUnit = temporal_unit_FOREVER();
    t.range = ValueRange_of(0, 1);
    t.displayNameKey = "era";

    return t;
}

temporal_field_t temporal_field_NANO_OF_SECOND()
{
    temporal_field_t t;
    t.name = "NanoOfSecond";
    t.baseUnit = temporal_unit_NANOS();
    t.rangeUnit = temporal_unit_SECONDS();
    t.range = ValueRange_of(0, 999999999);

    return t;
}

typedef enum SignStyle
{

    /**
     * Style to output the sign only if the value is negative.
     * <p>
     * In strict parsing, the negative sign will be accepted and the positive sign rejected.
     * In lenient parsing, any sign will be accepted.
     */
    NORMAL,
    /**
     * Style to always output the sign, where zero will output '+'.
     * <p>
     * In strict parsing, the absence of a sign will be rejected.
     * In lenient parsing, any sign will be accepted, with the absence
     * of a sign treated as a positive number.
     */
    ALWAYS,
    /**
     * Style to never output sign, only outputting the absolute value.
     * <p>
     * In strict parsing, any sign will be rejected.
     * In lenient parsing, any sign will be accepted unless the width is fixed.
     */
    NEVER,
    /**
     * Style to block negative values, throwing an exception on printing.
     * <p>
     * In strict parsing, any sign will be rejected.
     * In lenient parsing, any sign will be accepted unless the width is fixed.
     */
    NOT_NEGATIVE,
    /**
     * Style to always output the sign if the value exceeds the pad width.
     * A negative value will always output the '-' sign.
     * <p>
     * In strict parsing, the sign will be rejected unless the pad width is exceeded.
     * In lenient parsing, any sign will be accepted, with the absence
     * of a sign treated as a positive number.
     */
    EXCEEDS_PAD,
} signstyle_t;

typedef struct WeekBasedFieldPrinterParser_s
{
    temporal_field_t *field;
    int minWidth;
    int maxWidth;
    signstyle_t signStyle;
    int subsequentWidth;
    char chr;
    int count;
} WeekBasedFieldPrinterParser_t;

WeekBasedFieldPrinterParser_t WeekBasedFieldPrinterParser_(char chr, int count, int minWidth, int maxWidth, int subsequentWidth)
{
    WeekBasedFieldPrinterParser_t t;
    t.field = NULL;
    t.minWidth = minWidth;
    t.maxWidth = maxWidth;
    t.signStyle = NOT_NEGATIVE;
    t.subsequentWidth = subsequentWidth;
    t.chr = chr;
    t.count = count;

    return t;
}
WeekBasedFieldPrinterParser_t WeekBasedFieldPrinterParser(char chr, int count, int minWidth, int maxWidth)
{
    return WeekBasedFieldPrinterParser_(chr, count, minWidth, maxWidth, 0);
}

typedef struct DateTimeFormatterBuilder_s
{
    struct DateTimeFormatterBuilder_s *active;
    struct DateTimeFormatterBuilder_s *parent;
    int padNextWidth;
    /**
     * The character to pad the next field with.
     */
    char padNextChar;
    /**
     * The index of the last variable width value parser.
     */
    int valueParserIndex;
    int optional;
} DateTimeFormatterBuilder_t;

void padNext_(lua_State *L, DateTimeFormatterBuilder_t active, int padWidth, char padChar)
{
    if (padWidth < 1)
    {
        luaL_error(L, "The pad width must be at least one but was %d", padWidth);
    }
    active.padNextWidth = padWidth;
    active.padNextChar = padChar;
    active.valueParserIndex = -1;
}
void padNext(lua_State *L, DateTimeFormatterBuilder_t active, int padWidth)
{
    padNext_(L, active, padWidth, ' ');
}

void parseField(lua_State *L, char cur, int count, temporal_field_t field)
{
    // bool standalone = false;
    switch (cur)
    {
    case 'u':
    case 'y':
        if (count == 2)
        {
            tm_t ReducedPrinterParser_BASE_DATE;
            ReducedPrinterParser_BASE_DATE.tm_year = 2000;
            ReducedPrinterParser_BASE_DATE.tm_mon = 1;
            ReducedPrinterParser_BASE_DATE.tm_mday = 1;
            appendValueReduced(field, 2, 2, ReducedPrinterParser_BASE_DATE);
        }
        else if (count < 4)
        {
            appendValue(field, count, 19, NORMAL);
        }
        else
        {
            appendValue(field, count, 19, EXCEEDS_PAD);
        }
        break;
    case 'c':
        if (count == 1)
        {
            appendValue(WeekBasedFieldPrinterParser(cur, count, count, count));
            break;
        }
        else if (count == 2)
        {
            luaL_error(L, "Invalid pattern \"cc\"");
        }
        /*fallthrough*/
    case 'L':
    case 'q':
        // standalone = true;
        /*fallthrough*/
    case 'M':
    case 'Q':
    case 'E':
    case 'e':
        switch (count)
        {
        case 1:
        case 2:
            if (cur == 'e')
            {
                appendValue(WeekBasedFieldPrinterParser(cur, count, count, count));
            }
            else if (cur == 'E')
            {
                appendText(field, textstyle_SHORT);
            }
            else
            {
                if (count == 1)
                {
                    appendValue(field);
                }
                else
                {
                    appendValue(field, 2);
                }
            }
            break;
        case 3:
            // appendText(field, standalone ? TextStyle.SHORT_STANDALONE : TextStyle.SHORT);
            appendText(field, textstyle_SHORT);
            break;
        case 4:
            // appendText(field, standalone ? TextStyle.FULL_STANDALONE : TextStyle.FULL);
            appendText(field, textstyle_FULL);
            break;
        case 5:
            // appendText(field, standalone ? TextStyle.NARROW_STANDALONE : TextStyle.NARROW);
            appendText(field, textstyle_NARROW);
            break;
        default:
            luaL_error(L, "Too many pattern letters: %c", cur);
        }
        break;
    case 'a':
        if (count == 1)
        {
            appendText(field, textstyle_SHORT);
        }
        else
        {
            luaL_error(L, "Too many pattern letters: %c", cur);
        }
        break;
    case 'G':
        switch (count)
        {
        case 1:
        case 2:
        case 3:
            appendText(field, textstyle_SHORT);
            break;
        case 4:
            appendText(field, textstyle_FULL);
            break;
        case 5:
            appendText(field, textstyle_NARROW);
            break;
        default:
            luaL_error(L, "Too many pattern letters: %c", cur);
        }
        break;
    case 'S':
        appendFraction(temporal_field_NANO_OF_SECOND(), count, count, false);
        break;
    case 'F':
        if (count == 1)
        {
            appendValue(field);
        }
        else
        {
            luaL_error(L, "Too many pattern letters: %c", cur);
        }
        break;
    case 'd':
    case 'h':
    case 'H':
    case 'k':
    case 'K':
    case 'm':
    case 's':
        if (count == 1)
        {
            appendValue(field);
        }
        else if (count == 2)
        {
            appendValue(field, count);
        }
        else
        {
            luaL_error(L, "Too many pattern letters: %c", cur);
        }
        break;
    case 'D':
        if (count == 1)
        {
            appendValue(field);
        }
        else if (count == 2 || count == 3)
        {
            appendValue(field, count, 3, NOT_NEGATIVE);
        }
        else
        {
            luaL_error(L, "Too many pattern letters: " + cur);
        }
        break;
    case 'g':
        appendValue(field, count, 19, NORMAL);
        break;
    case 'A':
    case 'n':
    case 'N':
        appendValue(field, count, 19, NOT_NEGATIVE);
        break;
    default:
        if (count == 1)
        {
            appendValue(field);
        }
        else
        {
            appendValue(field, count);
        }
        break;
    }
}

void parsePattern(lua_State *L, const char *pattern, DateTimeFormatterBuilder_t active)
{
    int pattern_length = strlen(pattern);

    for (int pos = 0; pos < pattern_length; pos++)
    {
        char cur = pattern[pos];
        if ((cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
        {
            int start = pos++;
            for (; pos < pattern_length && pattern[pos] == cur; pos++)
                ; // short loop
            int count = pos - start;
            // padding
            if (cur == 'p')
            {
                int pad = 0;
                if (pos < pattern_length)
                {
                    cur = pattern[pos];
                    if ((cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
                    {
                        pad = count;
                        start = pos++;
                        for (; pos < pattern_length && pattern[pos] == cur; pos++)
                            ; // short loop
                        count = pos - start;
                    }
                }
                if (pad == 0)
                {
                    luaL_error(L, "Pad letter 'p' must be followed by valid pad pattern: %s", pattern);
                }
                padNext(L, active, pad); // pad and continue parsing
            }
            // main rules
            temporal_field_t field; // FIELD_MAP_get(L, cur);
            int found = 1;
            switch (cur)
            {
            case 'G':
                field = temporal_field_ERA();
                break;

            default:
                found = 0;
                break;
            }

            if (found == 1)
            {
                parseField(L, cur, count, field);
            }
            else if (cur == 'z')
            {
                if (count > 4)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                else if (count == 4)
                {
                    appendZoneText(textstyle_FULL);
                }
                else
                {
                    appendZoneText(textstyle_SHORT);
                }
            }
            else if (cur == 'V')
            {
                if (count != 2)
                {
                    luaL_error(L, "Pattern letter count must be 2: %c", cur);
                }
                appendZoneId();
            }
            else if (cur == 'v')
            {
                if (count == 1)
                {
                    appendGenericZoneText(textstyle_SHORT);
                }
                else if (count == 4)
                {
                    appendGenericZoneText(textstyle_FULL);
                }
                else
                {
                    luaL_error(L, "Wrong number of pattern letters: %c", cur);
                }
            }
            else if (cur == 'Z')
            {
                if (count < 4)
                {
                    appendOffset("+HHMM", "+0000");
                }
                else if (count == 4)
                {
                    appendLocalizedOffset(textstyle_FULL);
                }
                else if (count == 5)
                {
                    appendOffset("+HH:MM:ss", "Z");
                }
                else
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
            }
            else if (cur == 'O')
            {
                if (count == 1)
                {
                    appendLocalizedOffset(textstyle_SHORT);
                }
                else if (count == 4)
                {
                    appendLocalizedOffset(textstyle_FULL);
                }
                else
                {
                    luaL_error(L, "Pattern letter count must be 1 or 4: %c", cur);
                }
            }
            else if (cur == 'X')
            {
                if (count > 5)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                appendOffset(PATTERNS[count + (count == 1 ? 0 : 1)], "Z");
            }
            else if (cur == 'x')
            {
                if (count > 5)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                const char *zero = (count == 1 ? "+00" : (count % 2 == 0 ? "+0000" : "+00:00"));
                appendOffset(PATTERNS[count + (count == 1 ? 0 : 1)], zero);
            }
            else if (cur == 'W')
            {
                // Fields defined by Locale
                if (count > 1)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                appendValue(WeekBasedFieldPrinterParser(cur, count, count, count));
            }
            else if (cur == 'w')
            {
                // Fields defined by Locale
                if (count > 2)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                appendValue(WeekBasedFieldPrinterParser(cur, count, count, 2));
            }
            else if (cur == 'Y')
            {
                // Fields defined by Locale
                if (count == 2)
                {
                    appendValue(WeekBasedFieldPrinterParser(cur, count, count, 2));
                }
                else
                {
                    appendValue(WeekBasedFieldPrinterParser(cur, count, count, 19));
                }
            }
            else if (cur == 'B')
            {
                switch (count)
                {
                case 1:
                    appendDayPeriodText(textstyle_SHORT);
                    break;
                case 4:
                    appendDayPeriodText(textstyle_FULL);
                    break;
                case 5:
                    appendDayPeriodText(textstyle_NARROW);
                    break;
                default:
                    luaL_error(L, "Wrong number of pattern letters: %c", cur);
                }
            }
            else
            {
                luaL_error(L, "Unknown pattern letter: %c", cur);
            }
            pos--;
        }
        else if (cur == '\'')
        {
            // parse literals
            int start = pos++;
            for (; pos < pattern_length; pos++)
            {
                if (pattern[pos] == '\'')
                {
                    if (pos + 1 < pattern_length && pattern[pos + 1] == '\'')
                    {
                        pos++;
                    }
                    else
                    {
                        break; // end of literal
                    }
                }
            }
            if (pos >= pattern_length)
            {
                luaL_error(L, "Pattern ends with an incomplete string literal: %s", pattern);
            }

            // char *str = pattern.substring(start + 1, pos);
            int n = pos - (start + 1);

            if (n < 1) // str.isEmpty()
            {
                appendLiteral('\'');
            }
            else
            {
                char *str = (char *)malloc(sizeof(char) * (n + 1));
                strncpy(str, pattern + (start + 1), n);
                str[n] = '\0';

                // str.replace("''", "'")
                char *replaced = replace_consecutive_quotes(str);
                appendLiteral(replaced);

                free(replaced);
                free(str);
            }
        }
        else if (cur == '[')
        {
            optionalStart();
        }
        else if (cur == ']')
        {
            if (active.parent == NULL)
            {
                luaL_error(L, "Pattern invalid as it contains ] without previous [");
            }
            optionalEnd();
        }
        else if (cur == '{' || cur == '}' || cur == '#')
        {
            luaL_error(L, "Pattern includes reserved character: '%c", cur + "'");
        }
        else
        {
            appendLiteral(cur);
        }
    }
}

int l_ofPattern(lua_State *L)
{
    const char *pattern = lua_tostring(L, 1);
    DateTimeFormatterBuilder_t builder;
    builder.parent = NULL;
    builder.optional = 0;
    builder.valueParserIndex = -1;
    builder.active = &builder;

    parsePattern(L, pattern, builder);

    return 0;
}

const struct luaL_Reg libc[] = {
    {"ofPattern", l_ofPattern},

    {NULL, NULL} /* sentinel */
};

int luaopen_liblibc(lua_State *L)
{
    luaL_newlib(L, libc);

    return 1;
}