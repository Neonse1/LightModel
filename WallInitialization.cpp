#include "WallInitialization.h"
#include "WallUtils.h"
#include <iostream>

bool initializeWalls(Config& cfg) {
    if (cfg.points.size() < 3) return false;

    double area = 0;
    for (size_t i = 0; i < cfg.points.size(); ++i) {
        const Point& a = cfg.points[i];
        const Point& b = cfg.points[(i + 1) % cfg.points.size()];
        area += cross(a, b);
    }
    bool ccw = area > 0.0;

    for (WallConfig& w : cfg.walls) {
        w.center_set = false;
        if (w.type == WallType::FLAT) continue;

        const Point& A = cfg.points[w.idxA];
        const Point& B = cfg.points[w.idxB];
        double chord = len(B - A);
        if (chord / 2.0 > w.radius + EPS) {
            std::cerr << "Sphere radius too small: " << w.idxA << " and " << w.idxB << "\n";
            return false;
        }

        auto centers = circleCentersFromTwoPointsAndRadius(A, B, w.radius);
        if (centers.empty()) {
            std::cerr << "Failed to find center for spherical mirror\n";
            return false;
        }

        struct Cand {
            Point c;
            double angA, angB, delta;
            bool centerInsideOrientation;
        };
        std::vector<Cand> cands;

        for (Point c : centers) {
            double angA = angleOf(A - c);
            double angB = angleOf(B - c);
            double delta = normalizeAngle(angB - angA);
            if (std::fabs(delta) > PI + 1e-6) {
                if (delta > 0) delta -= 2 * PI;
                else delta += 2 * PI;
            }
            bool centerIsLeft = cross(B - A, c - A) > 0;
            bool centerInside = (ccw ? centerIsLeft : !centerIsLeft);
            cands.push_back({ c, angA, angB, delta, centerInside });
        }

        bool wantInside = (w.type == WallType::SPHERICAL_CONVEX);
        int chosen = -1;
        for (size_t k = 0; k < cands.size(); ++k) {
            if (cands[k].centerInsideOrientation == wantInside) {
                chosen = (int)k;
                break;
            }
        }
        if (chosen == -1) chosen = 0;

        w.center = cands[chosen].c;
        w.angA = cands[chosen].angA;
        double rawDelta = normalizeAngle(cands[chosen].angB - cands[chosen].angA);
        if (std::fabs(rawDelta) > PI) {
            if (rawDelta > 0) rawDelta -= 2 * PI;
            else rawDelta += 2 * PI;
        }
        w.angDelta = rawDelta;
        w.angB = w.angA + w.angDelta;
        w.center_set = true;
    }
    return true;
}