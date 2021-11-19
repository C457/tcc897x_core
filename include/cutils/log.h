#include <log/log.h>

#ifndef ALOG_T_NDBUG
#define ALOG_T_NDBUG 1
#endif

/*
 * Telechips run-time on/off log function with user-defined tag suffix.
 *
 *  ----------------------------------------------
 *     cmd   |     name     |        value
 *  ----------------------------------------------
 *   setprop | type #1 / #2 | {android_log_digit}
 *  ----------------------------------------------
 *
 *  android_log_digit := {0|1|2|3|4|5|6|7|8}
 *                        U| |V|D|I|W|E|F|S
 *
 *   : Refer to the android_LogPriority enum in <android/log.h>
 *
 */

// Type #1: {LOG_TAG}.{TAG_SUFFIX}
#if ALOG_T_NDBUG
#define ALOGT(TAG_SUFFIX, ...) ((void)0)
#else
#define ALOGT(TAG_SUFFIX, ...) \
    ALOG_T(LOG_TAG "." #TAG_SUFFIX, "["#TAG_SUFFIX"] " __VA_ARGS__)

#define ALOG_T(TagName, ...) { \
    char log_level[PROPERTY_VALUE_MAX]; \
    property_get(TagName, log_level, "0"); \
    if (log_level[0] != '0') \
        ((void)__android_log_buf_print( \
            LOG_ID_SYSTEM, (int)log_level[0]-'0', LOG_TAG, __VA_ARGS__)); \
}
#endif

// Type #2: {TCCLOG_PREFIX}{TagName}
#define TCCLOG_PREFIX "dbg.log."
#define PrePropertyName TCCLOG_PREFIX
#if ALOG_T_NDBUG
#define TCCLOG(TagName, ...) ((void)0)
#else
#define TCCLOG(TagName, ...) ALOG_T(TCCLOG_PREFIX #TagName, __VA_ARGS__)
#endif
