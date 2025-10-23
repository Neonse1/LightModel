#pragma once

#include "Constants.h"

struct Point {
    double x, y;
    Point();
    Point(double _x, double _y);
    Point operator+(const Point& o) const;
    Point operator-(const Point& o) const;
    Point operator*(double s) const;
    Point operator/(double s) const;
    bool operator==(const Point& o) const;
};

double dot(const Point& a, const Point& b);
double cross(const Point& a, const Point& b);
double len(const Point& a);
Point norm(const Point& a);
double angleOf(const Point& a);
Point fromAngle(double a);
double normalizeAngle(double a);
double deg2rad(double d);
double rad2deg(double r);