#include "WallUtils.h"
#include <cmath>
#include <algorithm>
#include <iostream>

std::vector<Point> circleCentersFromTwoPointsAndRadius(const Point& p1, const Point& p2, double r) {
    std::vector<Point> res;
    Point mid = (p1 + p2) * 0.5;
    double d = len(p2 - p1);
    if (d > 2 * r + EPS) return res;
    if (d < EPS) return res;
    double h = std::sqrt(std::max(0.0, r * r - (d * d) / 4.0));
    Point dir = norm(Point(-(p2.y - p1.y), p2.x - p1.x));
    res.push_back(mid + dir * h);
    res.push_back(mid - dir * h);
    return res;
}

bool angleOnArc(double angA, double angDelta, double theta) {
    double t = normalizeAngle(theta - angA);
    if (angDelta >= 0)
        return (t >= -EPS && t <= angDelta + EPS);
    else
        return (t <= EPS && t >= angDelta - EPS);
}

Point pointOnWall(const WallConfig& wcfg, const Config& cfg, double t) {
    const Point& A = cfg.points[wcfg.idxA];
    const Point& B = cfg.points[wcfg.idxB];
    if (wcfg.type == WallType::FLAT) {
        return A * (1.0 - t) + B * t;
    }
    else {
        double ang = wcfg.angA + wcfg.angDelta * t;
        return Point(wcfg.center.x + wcfg.radius * std::cos(ang),
            wcfg.center.y + wcfg.radius * std::sin(ang));
    }
}

Point tangentOnWall(const WallConfig& wcfg, const Config& cfg, double t) {
    const Point& A = cfg.points[wcfg.idxA];
    const Point& B = cfg.points[wcfg.idxB];
    if (wcfg.type == WallType::FLAT) {
        return norm(B - A);
    }
    else {
        double ang = wcfg.angA + wcfg.angDelta * t;
        double tangentAng = ang + (wcfg.angDelta >= 0 ? PI / 2.0 : -PI / 2.0);
        return norm(Point(std::cos(tangentAng), std::sin(tangentAng)));
    }
}