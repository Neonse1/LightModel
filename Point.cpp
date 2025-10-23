#include "Point.h"
#include <cmath>

Point::Point() : x(0), y(0) {}
Point::Point(double _x, double _y) : x(_x), y(_y) {}

Point Point::operator+(const Point& o) const { return Point(x + o.x, y + o.y); }
Point Point::operator-(const Point& o) const { return Point(x - o.x, y - o.y); }
Point Point::operator*(double s) const { return Point(x * s, y * s); }
Point Point::operator/(double s) const { return Point(x / s, y / s); }

bool Point::operator==(const Point& o) const {
    return std::abs(x - o.x) < EPS && std::abs(y - o.y) < EPS;
}

double dot(const Point& a, const Point& b) { return a.x * b.x + a.y * b.y; }
double cross(const Point& a, const Point& b) { return a.x * b.y - a.y * b.x; }
double len(const Point& a) { return std::hypot(a.x, a.y); }

Point norm(const Point& a) {
    double L = len(a);
    if (L < EPS) return Point(0, 0);
    return a / L;
}

double angleOf(const Point& a) { return std::atan2(a.y, a.x); }
Point fromAngle(double a) { return Point(std::cos(a), std::sin(a)); }

double normalizeAngle(double a) {
    a = std::fmod(a + PI, 2 * PI);
    if (a < 0) a += 2 * PI;
    return a - PI;
}

double deg2rad(double d) { return d * PI / 180.0; }
double rad2deg(double r) { return r * 180.0 / PI; }