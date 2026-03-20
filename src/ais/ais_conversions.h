#ifndef AIS_CONVERSIONS_H_
#define AIS_CONVERSIONS_H_

#include <cmath>
#include <cstdint>

namespace ais {

constexpr double kDegreesToRadians = M_PI / 180.0;
constexpr double kKnotsToMs = 1852.0 / 3600.0;  // 0.514444...

// Convert AIS heading (0-359 degrees, 511=not available) to radians.
// Returns NAN if not available.
inline double heading_to_radians(uint16_t heading) {
  if (heading >= 360) return NAN;
  return heading * kDegreesToRadians;
}

// Convert AIS COG (0-359.9 degrees, 360.0=not available) to radians.
inline double cog_to_radians(double cog_degrees) {
  if (cog_degrees >= 360.0) return NAN;
  return cog_degrees * kDegreesToRadians;
}

// Convert AIS SOG (knots, NAN if not available) to m/s.
inline double sog_to_ms(double sog_knots) {
  if (std::isnan(sog_knots)) return NAN;
  return sog_knots * kKnotsToMs;
}

// Convert AIS ROT (deg/min, NAN if not available) to rad/s.
inline double rot_to_rads(double rot_degmin) {
  if (std::isnan(rot_degmin)) return NAN;
  return rot_degmin * kDegreesToRadians / 60.0;
}

// Convert AIS dimensions (to_bow, to_stern, to_port, to_starboard) to
// N2K length, beam, position reference from starboard, position reference
// from bow.
struct N2kDimensions {
  double length;        // meters
  double beam;          // meters
  double pos_ref_stbd;  // meters from starboard
  double pos_ref_bow;   // meters from bow
};

inline N2kDimensions dimensions_to_n2k(uint16_t to_bow, uint16_t to_stern,
                                        uint16_t to_port,
                                        uint16_t to_starboard) {
  return {
      static_cast<double>(to_bow + to_stern),
      static_cast<double>(to_port + to_starboard),
      static_cast<double>(to_starboard),
      static_cast<double>(to_bow),
  };
}

// Convert AIS ETA (month, day, hour, minute) to N2K date and time.
// N2K ETAdate = days since 1970-01-01 (uint16_t)
// N2K ETAtime = seconds since midnight (double)
// Returns 0 for both if ETA is not available (month=0 or day=0).
struct N2kETA {
  uint16_t date;  // days since 1970-01-01
  double time;    // seconds since midnight
};

inline N2kETA eta_to_n2k(uint8_t month, uint8_t day, uint8_t hour,
                          uint8_t minute, uint16_t current_year = 2026) {
  if (month == 0 || day == 0) {
    return {0, 0.0};
  }

  // Calculate days since 1970-01-01
  // Simple calculation — no leap second precision needed
  uint16_t year = current_year;
  int days = 0;
  for (uint16_t y = 1970; y < year; y++) {
    bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    days += leap ? 366 : 365;
  }
  static const int month_days[] = {0, 31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  for (int m = 1; m < month && m <= 12; m++) {
    days += month_days[m];
    if (m == 2 && leap) days += 1;
  }
  days += day - 1;

  double time_secs = (hour < 24 ? hour : 0) * 3600.0 +
                     (minute < 60 ? minute : 0) * 60.0;

  return {static_cast<uint16_t>(days), time_secs};
}

}  // namespace ais

#endif  // AIS_CONVERSIONS_H_
