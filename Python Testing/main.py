import pytz

timezones = pytz.all_timezones

for tz in timezones:
    timezone = pytz.timezone(tz)
    timezone_info = timezone.tzname(None)
    timezone_info += "," + ",".join(str(x) for x in timezone._utc_transition_times[0].timetuple()[1:3])
    timezone_info += "," + ",".join(str(x) for x in timezone._utc_transition_times[-1].timetuple()[1:3])
    print(f"{tz:30s}\t{timezone_info}") 
