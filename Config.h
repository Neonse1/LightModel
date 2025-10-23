#pragma once

#include "Point.h"
#include <vector>

enum class WallType { FLAT, SPHERICAL_CONVEX, SPHERICAL_CONCAVE };

struct WallConfig {
    WallType type;
    int idxA, idxB;
    double radius;
    Point center;
    bool center_set;
    double angA, angB;
    double angDelta;
    double reflectivity;

    WallConfig();
};

struct Config {
    std::vector<Point> points;
    std::vector<WallConfig> walls;
    int emit_wall_index;
    double emit_wall_t;
    double emit_angle_deg;
    Point target_point;
    double target_radius;
    int max_reflections;
    bool return_on_hit;

    Config();
};