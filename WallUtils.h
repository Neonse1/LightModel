#pragma once

#include "Config.h"
#include "Constants.h"
#include <vector>

std::vector<Point> circleCentersFromTwoPointsAndRadius(const Point& p1, const Point& p2, double r);
bool angleOnArc(double angA, double angDelta, double theta);
Point pointOnWall(const WallConfig& wcfg, const Config& cfg, double t);
Point tangentOnWall(const WallConfig& wcfg, const Config& cfg, double t);