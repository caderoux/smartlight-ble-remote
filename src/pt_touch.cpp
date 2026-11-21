// Touch maths: taps from raw readings, and the calibration that maps them to pixels (see pt_touch.h).
#include "pt_touch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace pt {

int median(std::vector<int> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

bool Taps::feed(bool pressed, Raw reading, Raw &out) {
  if (pressed) {
    if (int(xs.size()) < MAX_READINGS) {  // a book or a hand resting on the panel mustn't grow this forever
      xs.push_back(reading.x);
      ys.push_back(reading.y);
    }
    gap = 0;
    return false;
  }
  if (xs.empty()) return false;
  if (++gap < releaseAfter) return false;
  bool enough = int(xs.size()) >= minReadings;
  if (enough) out = Raw{median(xs), median(ys)};
  xs.clear();
  ys.clear();
  gap = 0;
  return enough;
}

// Least-squares line screen = a * raw + b; false if the raw values don't vary.
static bool fitLine(const std::vector<double> &raw, const std::vector<double> &screen, float &a, float &b) {
  double n = raw.size(), mr = 0, ms = 0;
  for (size_t i = 0; i < raw.size(); i++) {
    mr += raw[i];
    ms += screen[i];
  }
  mr /= n;
  ms /= n;
  double var = 0, cov = 0;
  for (size_t i = 0; i < raw.size(); i++) {
    var += (raw[i] - mr) * (raw[i] - mr);
    cov += (raw[i] - mr) * (screen[i] - ms);
  }
  if (var == 0) return false;
  a = float(cov / var);
  b = float(ms - a * mr);
  return true;
}

static double correlation(const std::vector<double> &xs, const std::vector<double> &ys) {
  double n = xs.size(), mx = 0, my = 0;
  for (size_t i = 0; i < xs.size(); i++) {
    mx += xs[i];
    my += ys[i];
  }
  mx /= n;
  my /= n;
  double sxy = 0, sx = 0, sy = 0;
  for (size_t i = 0; i < xs.size(); i++) {
    sxy += (xs[i] - mx) * (ys[i] - my);
    sx += (xs[i] - mx) * (xs[i] - mx);
    sy += (ys[i] - my) * (ys[i] - my);
  }
  return (sx == 0 || sy == 0) ? 0 : sxy / std::sqrt(sx * sy);
}

bool Calibration::fit(const std::vector<Target> &taps, int width, int height, Calibration &out) {
  std::vector<double> rx, ry, sx, sy;
  for (const Target &t : taps) {
    rx.push_back(t.raw.x);
    ry.push_back(t.raw.y);
    sx.push_back(t.sx);
    sy.push_back(t.sy);
  }
  Calibration c;
  c.width = width;
  c.height = height;
  c.swap = std::fabs(correlation(ry, sx)) > std::fabs(correlation(rx, sx));
  if (!fitLine(c.swap ? ry : rx, sx, c.ax, c.bx)) return false;
  if (!fitLine(c.swap ? rx : ry, sy, c.ay, c.by)) return false;
  out = c;
  return true;
}

void Calibration::toScreen(Raw raw, int &x, int &y) const {
  float u = swap ? raw.y : raw.x, v = swap ? raw.x : raw.y;
  x = std::min(std::max(int(std::lround(ax * u + bx)), 0), width - 1);
  y = std::min(std::max(int(std::lround(ay * v + by)), 0), height - 1);
}

std::string Calibration::toString() const {
  char buf[96];
  snprintf(buf, sizeof buf, "%d,%.7f,%.6f,%.7f,%.6f", swap ? 1 : 0, ax, bx, ay, by);
  return buf;
}

bool Calibration::parse(const std::string &text) {
  int s = 0, used = 0;
  float a, b, c, d;
  if (sscanf(text.c_str(), "%d,%f,%f,%f,%f%n", &s, &a, &b, &c, &d, &used) != 5) return false;
  if (used != int(text.size()) || (s != 0 && s != 1)) return false;  // trailing junk, or a swap that isn't 0/1
  for (float v : {a, b, c, d}) {
    if (!std::isfinite(v)) return false;
  }
  if (a == 0 || c == 0) return false;  // a zero slope maps every touch to one line
  swap = s == 1;
  ax = a;
  bx = b;
  ay = c;
  by = d;
  return true;
}

int Calibration::worstError(const std::vector<Target> &taps) const {
  int worst = 0;
  for (const Target &t : taps) {
    int x, y;
    toScreen(t.raw, x, y);
    worst = std::max(worst, std::max(std::abs(x - t.sx), std::abs(y - t.sy)));
  }
  return worst;
}

}  // namespace pt
