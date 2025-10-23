#include <SFML/Graphics.hpp>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

// ------------------- Константы -------------------
constexpr double EPS = 1e-12;  // Рассматриваемое приближение
constexpr double EPS_MOVE =
    1e-11;  // Сдвиг относительно стены(чтобы луч не начинался в стене)
constexpr double PI = 3.14159265358979323846;
constexpr int MAX_REFLECTIONS_DEFAULT = 10000;  // Кап отражений
constexpr double THRESHOLD_INTENSITY =
    1.0 / 510.0;  // Минимальная яркость луча, иначе – выход

int main() { return 0; }