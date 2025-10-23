#include "RayTracing.h"
#include "Intersections.h"
#include "Reflection.h"
#include "Constants.h"
#include "WallUtils.h"

TraceResult traceRay(const Config& cfg) {
    TraceResult res;
    res.hit = false;
    res.reflection_index_when_hit = -1;
    res.segments.clear();
    res.intersection_points.clear();

    if (cfg.emit_wall_index < 0 || cfg.emit_wall_index >= (int)cfg.walls.size()) {
        return res;
    }

    const WallConfig& w0 = cfg.walls[cfg.emit_wall_index];
    Point pos = pointOnWall(w0, cfg, cfg.emit_wall_t);
    Point tan = tangentOnWall(w0, cfg, cfg.emit_wall_t);
    double phi = deg2rad(cfg.emit_angle_deg);
    double baseAng = angleOf(tan);
    Point dir = fromAngle(baseAng + phi);

    double intensity = 1.0;
    int reflections = 0;
    Point curP = pos;
    Point curDir = norm(dir);
    int last_hit_wall = cfg.emit_wall_index;

    res.intersection_points.push_back(pos);

    for (int iter = 0; iter < cfg.max_reflections && intensity >= THRESHOLD_INTENSITY; ++iter) {
        int hit_wall = -1;
        Point hit_pt;
        double tRay;
        bool isSpherical;

        bool found = findNearestIntersection(cfg, curP, curDir, last_hit_wall,
            cfg.emit_wall_t, hit_wall, hit_pt, tRay, isSpherical);
        if (!found) break;

        RaySegment seg;
        seg.a = curP;
        seg.b = hit_pt;
        seg.intensity = intensity;
        seg.reflection_count = reflections;
        res.segments.push_back(seg);
        res.intersection_points.push_back(hit_pt);

        if (hit_wall == -1) {
            res.hit = true;
            res.reflection_index_when_hit = reflections;
            break;
        }

        Point normal;
        if (!isSpherical) {
            Point A = cfg.points[cfg.walls[hit_wall].idxA];
            Point B = cfg.points[cfg.walls[hit_wall].idxB];
            Point tangent = norm(B - A);
            Point n1 = Point(-tangent.y, tangent.x);
            Point n2 = Point(tangent.y, -tangent.x);
            normal = (dot(curDir, n1) < 0) ? n1 : n2;
        }
        else {
            Point n = norm(hit_pt - cfg.walls[hit_wall].center);
            normal = (dot(curDir, n) > 0) ? n * -1.0 : n;
        }
        normal = norm(normal);

        Point refl = reflect(curDir, normal);
        curP = hit_pt + refl * EPS_MOVE;
        curDir = norm(refl);
        last_hit_wall = hit_wall;

        double wall_reflectivity = cfg.walls[hit_wall].reflectivity;
        intensity *= wall_reflectivity;
        ++reflections;
    }
    return res;
}