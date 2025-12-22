#include "libcron/CronSchedule.h"
#include <tuple>

using namespace std::chrono;
using namespace date;

namespace libcron
{
    std::tuple<bool, std::chrono::system_clock::time_point>
    CronSchedule::calculate_from(const std::chrono::system_clock::time_point& from) const
    {
        auto curr = from;

        bool done = false;
        auto max_iterations = std::numeric_limits<uint16_t>::max();

        while (!done && --max_iterations > 0)
        {
            bool date_changed = false;
            year_month_day ymd = date::floor<days>(curr);


            // Add years until one of the allowed years are found, or stay at the current one.
            if (data.get_years().find(static_cast<Years>((int)ymd.year())) == data.get_years().end())
            {
                auto next_year = ymd.year() + years{1};
                sys_days s = next_year / 1 / 1;
                curr = s;
                date_changed = true;
            }
            // Add months until one of the allowed days are found, or stay at the current one.
            else if (data.get_months().find(static_cast<Months>(unsigned(ymd.month()))) == data.get_months().end())
            {
                auto next_month = ymd + months{1};
                sys_days s = next_month.year() / next_month.month() / 1;
                curr = s;
                date_changed = true;
            }
                // If all days are allowed (or the field is ignored via '?'), then the 'day of week' takes precedence.
            else if (data.get_day_of_month().size() != CronData::value_of(DayOfMonth::Last))
            {
                auto day = ymd.day();

                if (data.is_index_of_month_day_reversed())
                {
                    // flip the day offset to count from the end of the month
                    year_month_day_last mdl = ymd.year() / ymd.month() / last;
                    day = date::day(unsigned(mdl.day()) - unsigned(day) + 1);
                }

                // Add days until one of the allowed days are found, or stay at the current one.
                if (data.get_day_of_month().find(static_cast<DayOfMonth>(unsigned(day))) ==
                    data.get_day_of_month().end())
                {
                    sys_days s = ymd;
                    curr = s;
                    curr += days{1};
                    date_changed = true;
                }
            }
            else
            {
                //Add days until the current weekday is one of the allowed weekdays
                year_month_weekday ymw = date::floor<days>(curr);

                if (data.get_day_of_week().find(static_cast<DayOfWeek>(ymw.weekday().c_encoding())) ==
                    data.get_day_of_week().end())
                {
                    sys_days s = ymd;
                    curr = s;
                    curr += days{1};
                    date_changed = true;
                }
                else {
                    // Add weeks until the current index is one of the allowed indexes
                    if (!data.is_index_of_week_day_reversed())
                    {
                        if (data.get_index_of_day().find(static_cast<IndexOfDay>(ymw.weekday_indexed().index())) ==
                            data.get_index_of_day().end())
                        {
                            sys_days s = ymd;
                            curr = s;
                            curr += days{ 7 };
                            date_changed = true;
                        }
                    }
                    else {
                        sys_days s = ymw;
                        bool ok = false;

                        for (auto& index : data.get_index_of_day()) {
                            year_month_weekday_last ymwl(ymw.year(), ymw.month(), weekday_last(ymw.weekday()));
                            sys_days ymwl_days = ymwl;
                            ymwl_days -= weeks((((uint8_t) index) - 1));
                            if (s == (sys_days)ymwl_days) {
                                ok = true;
                                break;
                            }
                        }

                        if (!ok) {
                            curr = s;
                            curr += days{ 7 };
                            date_changed = true;
                        }
                    }
                }
            }

            if (!date_changed)
            {
                auto date_time = to_calendar_time(curr);
                if (data.get_hours().find(static_cast<Hours>(date_time.hour)) == data.get_hours().end())
                {
                    curr += hours{1};
                    curr -= minutes{date_time.min};
                    curr -= seconds{date_time.sec};
                }
                else if (data.get_minutes().find(static_cast<Minutes >(date_time.min)) == data.get_minutes().end())
                {
                    curr += minutes{1};
                    curr -= seconds{date_time.sec};
                }
                else if (data.get_seconds().find(static_cast<Seconds>(date_time.sec)) == data.get_seconds().end())
                {
                    curr += seconds{1};
                }
                else
                {
                    done = true;
                }
            }
        }

        // Discard fraction seconds in the calculated schedule time
        //  that may leftover from the argument `from`, which in turn comes from `now()`.
        // Fraction seconds will potentially make the task be triggered more than 1 second late
        //  if the `tick()` within the same second is earlier than schedule time,
        //  in that the task will not trigger until the next `tick()` next second.
        // By discarding fraction seconds in the scheduled time,
        //  the `tick()` within the same second will never be earlier than schedule time,
        //  and the task will trigger in that `tick()`.
        curr -= curr.time_since_epoch() % seconds{1};

        return std::make_tuple(max_iterations > 0, curr);
    }
}