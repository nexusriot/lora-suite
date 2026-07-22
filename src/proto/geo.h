#pragma once

namespace ls {

// Great-circle distance in metres between two WGS84 points.
double haversineMeters(double lat1, double lon1, double lat2, double lon2);

// Initial bearing in degrees (0=N, 90=E) from point 1 to point 2.
double bearingDeg(double lat1, double lon1, double lat2, double lon2);

} // namespace ls
