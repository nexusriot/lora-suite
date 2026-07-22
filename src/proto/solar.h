#pragma once

namespace ls {

// Approximate daylight length in hours at a latitude on a given day-of-year
// (1..366). Returns 24 for polar day, 0 for polar night. Used by Chronos'
// darkness planner; no timezone/longitude needed for length alone.
double daylightHours(double latDeg, int dayOfYear);

} // namespace ls
