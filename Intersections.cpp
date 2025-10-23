#include "Intersections.h"
#include "WallUtils.h"
#include <cmath>
#include <limits>
#include <algorithm>

bool intersectRaySegment(const Point& P0, const Point& dir,
    const Point& A, const Point& B, double& tRay,
    double& uSeg) {
    Point v = B - A;
    double denom = cross(dir, v);
    if (std::fabs(denom) < EPS) return false;
    Point rhs = A - P0;
    tRay = cross(rhs, v) / denom;
    uSeg = cross(rhs, dir) / denom;
    if (tRay > EPS && uSeg > EPS && uSeg < 1.0 - EPS) return true;
    return false;
}

bool intersectRayCircle(const Point& P0, const Point& dir,
    const Point& center, double r, double& tOut1,
    double& tOut2) {
    Point oc = P0 - center;
    double a = dot(dir, dir);
    double b = 2.0 * dot(oc, dir);
    double c = dot(oc, oc) - r * r;
    double disc = b * b - 4 * a * c;
    if (disc < -EPS) return false;
    disc = std::max(0.0, disc);
    double sqrtD = std::sqrt(disc);
    tOut1 = (-b - sqrtD) / (2 * a);
    tOut2 = (-b + sqrtD) / (2 * a);
    return true;
}

bool pointOnArc(const WallConfig& wcfg, const Point& p) {
    double ang = angleOf(p - wcfg.center);
    return angleOnArc(wcfg.angA, wcfg.angDelta, ang);
}

bool findNearestIntersection(const Config& cfg, const Point& P0,
    const Point& dir, int ignore_wall_idx,
    double ignore_coord_t, int& out_wall_idx,
    Point& out_pt, double& out_tRay,
    bool& out_isSpherical) {
    double bestT = std::numeric_limits<double>::infinity();
    bool found = false;

    double t1, t2;
    if (intersectRayCircle(P0, dir, cfg.target_point, cfg.target_radius, t1, t2)) {
        if (t1 > EPS && t1 < bestT) {
            bestT = t1;
            out_wall_idx = -1;
            out_pt = P0 + dir * t1;
            out_tRay = t1;
            out_isSpherical = true;
            found = true;
        }
    }

    for (size_t i = 0; i < cfg.walls.size(); ++i) {
        const WallConfig& w = cfg.walls[i];
        if (w.type == WallType::FLAT) {
            double tRay, uSeg;
            if (!intersectRaySegment(P0, dir, cfg.points[w.idxA], cfg.points[w.idxB],
                tRay, uSeg))
                continue;
            if ((int)i == ignore_wall_idx && tRay < EPS_MOVE * 10) continue;
            if (tRay < bestT) {
                bestT = tRay;
                out_wall_idx = (int)i;
                out_pt = P0 + dir * tRay;
                out_tRay = tRay;
                out_isSpherical = false;
                found = true;
            }
        }
        else {
            double t1, t2;
            if (!intersectRayCircle(P0, dir, w.center, w.radius, t1, t2)) continue;
            if (t1 > t2) std::swap(t1, t2);

            for (int pass = 0; pass < 2; ++pass) {
                double tCand = (pass == 0 ? t1 : t2);
                if (tCand <= EPS) continue;
                Point ip = P0 + dir * tCand;
                if (!pointOnArc(w, ip)) continue;
                if ((int)i == ignore_wall_idx && tCand < EPS_MOVE * 10) continue;
                if (tCand < bestT) {
                    bestT = tCand;
                    out_wall_idx = (int)i;
                    out_pt = ip;
                    out_tRay = tCand;
                    out_isSpherical = true;
                    found = true;
                }
                break;
            }
        }
    }
    return found;
}