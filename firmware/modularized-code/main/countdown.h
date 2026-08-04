#pragma once

// Days remaining until the next occurrence of birthday_month/birthday_date,
// rolling to next year once this year's date has passed. Returns 365 on the
// birthday itself, to start the following year's countdown.
int calculate_days_until_birthday(int current_date, int current_month, int current_year,
                                  int birthday_date, int birthday_month);
