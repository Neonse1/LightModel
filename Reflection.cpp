#include "Reflection.h"

Point reflect(const Point& dir, const Point& n) {
    double dDotN = dot(dir, n);
    return dir - n * (2.0 * dDotN);
}