#pragma once

#include "Point.h"
#include <vector>

struct RaySegment {
    Point a, b;
    double intensity;
    int reflection_count;
};

struct TraceResult {
    bool hit;
    int reflection_index_when_hit;
    std::vector<RaySegment> segments;
    std::vector<Point> intersection_points;

    TraceResult();
};