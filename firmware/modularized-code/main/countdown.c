#include "countdown.h"

#include <time.h>

int calculate_days_until_birthday(int current_date, int current_month, int current_year,
                                  int birthday_date, int birthday_month) {
    struct tm current_time = {0}, birthday_time = {0};
    time_t now, birthday_epoch;

    // Set current time structure
    current_time.tm_mday = current_date;
    current_time.tm_mon = current_month - 1;  // tm_mon is 0-indexed
    current_time.tm_year = current_year - 1900;  // tm_year starts from 1900

    // Set birthday time structure for this year
    birthday_time.tm_mday = birthday_date;
    birthday_time.tm_mon = birthday_month - 1;
    birthday_time.tm_year = current_year - 1900;

    // Get epoch times
    now = mktime(&current_time);
    birthday_epoch = mktime(&birthday_time);

    // If birthday has passed this year, calculate for next year
    if (mktime(&birthday_time) <= mktime(&current_time)) {
        birthday_time.tm_year += 1;  // Next year's birthday
        birthday_epoch = mktime(&birthday_time);
    }

    // Calculate the difference in seconds and convert to days
    double seconds_until_birthday = difftime(birthday_epoch, now);
    int days_until_birthday = (int)(seconds_until_birthday / (60 * 60 * 24));

    // If we hit exactly 0 days (birthday), return 365 to start next year's countdown
    if (days_until_birthday <= 0 &&
        current_date == birthday_date &&
        current_month == birthday_month) {
        return 365;  // Start counting down to next year
    }

    return days_until_birthday;
}
