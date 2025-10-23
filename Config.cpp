#include "Config.h"

WallConfig::WallConfig()
    : type(WallType::FLAT), idxA(-1), idxB(-1), radius(0), center(),
    center_set(false), angA(0), angB(0), angDelta(0), reflectivity(0.9) {
}

Config::Config()
    : emit_wall_index(0), emit_wall_t(0.5), emit_angle_deg(45.0),
    target_point(400, 300), target_radius(10.0),
    max_reflections(MAX_REFLECTIONS_DEFAULT), return_on_hit(false) {
}