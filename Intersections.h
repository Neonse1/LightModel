#pragma once

#include "Config.h"
#include "Constants.h"

bool intersectRaySegment(const Point& P0, const Point& dir,
    const Point& A, const Point& B, double& tRay,
    double& uSeg);
bool intersectRayCircle(const Point& P0, const Point& dir,
    const Point& center, double r, double& tOut1,
    double& tOut2);
bool pointOnArc(const WallConfig& wcfg, const Point& p);
bool findNearestIntersection(const Config& cfg, const Point& P0,
    const Point& dir, int ignore_wall_idx,
    double ignore_coord_t, int& out_wall_idx,
    Point& out_pt, double& out_tRay,
    bool& out_isSpherical);