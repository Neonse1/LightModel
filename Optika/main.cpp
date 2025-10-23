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

// ------------------- Вспомогательные структуры -------------------
struct Point {
  double x, y;
  Point() : x(0), y(0) {}
  Point(double _x, double _y) : x(_x), y(_y) {}
  Point operator+(const Point& o) const { return Point(x + o.x, y + o.y); }
  Point operator-(const Point& o) const { return Point(x - o.x, y - o.y); }
  Point operator*(double s) const { return Point(x * s, y * s); }
  Point operator/(double s) const { return Point(x / s, y / s); }
};

// Скалярное произведение
inline double dot(const Point& a, const Point& b) {
  return a.x * b.x + a.y * b.y;
}
// Псевдоскалярное произведение
inline double cross(const Point& a, const Point& b) {
  return a.x * b.y - a.y * b.x;
}
inline double len(const Point& a) { return std::hypot(a.x, a.y); }
inline Point norm(const Point& a) {
  double L = len(a);
  if (L < EPS) return Point(0, 0);
  return a / L;
}
inline double angleOf(const Point& a) { return std::atan2(a.y, a.x); }
inline Point fromAngle(double a) { return Point(std::cos(a), std::sin(a)); }

// Переделать в от -180 до 180
inline double normalizeAngle(double a) {
  a = std::fmod(a + PI, 2 * PI);
  if (a < 0) a += 2 * PI;
  return a - PI;
}
inline double deg2rad(double d) { return d * PI / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / PI; }

// ------------------- Конфигурация -------------------
enum class WallType { FLAT, SPHERICAL_CONVEX, SPHERICAL_CONCAVE };

struct WallConfig {
  WallType type;
  int idxA, idxB;
  double radius;
  Point center;
  bool center_set;
  double angA, angB;
  double angDelta;
  WallConfig()
      : type(WallType::FLAT),
        idxA(-1),
        idxB(-1),
        radius(0),
        center(),
        center_set(false),
        angA(0),
        angB(0),
        angDelta(0) {}
};

struct Config {
  std::vector<Point> points;
  std::vector<WallConfig> walls;
  int emit_wall_index;  // с какой стены выпускается луч (индекс в walls)
  double emit_wall_t;  // от 0 до 1 (концы должны исключаться)
  double emit_angle_deg;  // 0..180 — угол
  double reflectivity;    // 0.01 .. 0.999
  Point target_point;     // точка цели
  double target_radius;   // радиус зоны попадания
  int max_reflections;
  bool return_on_hit;
  Config()
      : emit_wall_index(0),
        emit_wall_t(0.5),
        emit_angle_deg(-10.0),
        reflectivity(0.9),
        target_point(),
        target_radius(5.0),
        max_reflections(MAX_REFLECTIONS_DEFAULT),
        return_on_hit(1) {}
};

// ------------------- Результаты трассировки -------------------
struct RaySegment {
  Point a, b;
  double intensity;  // 0..1
};
struct TraceResult {
  bool hit;
  int reflection_index_when_hit;  // как много раз луч отразился до достижения
                                  // цели
  std::vector<RaySegment> segments;  // для визуализации
};

// ------------------- Утилиты для работы со стенами -------------------

// найти центр окружностей радиуса r, проходящих через p1 и p2.
// возвращает 0 если невозможно (расстояние > 2r), иначе 2 центров (вектора size
// 2)
static std::vector<Point> circleCentersFromTwoPointsAndRadius(const Point& p1,
                                                              const Point& p2,
                                                              double r) {
  std::vector<Point> res;
  Point mid = (p1 + p2) * 0.5;
  double d = len(p2 - p1);
  if (d > 2 * r + EPS) return res;  // Невозможно
  if (d < EPS) return res;
  double h = std::sqrt(std::max(0.0, r * r - (d * d) / 4.0));
  Point dir = norm(Point(-(p2.y - p1.y), p2.x - p1.x));  // единичная нормаль
  res.push_back(mid + dir * h);
  res.push_back(mid - dir * h);
  return res;
}

// Проверка, входит ли угол theta в диапазон [angA, angA + angDelta]
static bool angleOnArc(double angA, double angDelta, double theta) {
  // angDelta — signed, |angDelta| <= PI (малый угол)
  double t = normalizeAngle(theta - angA);
  if (angDelta >= 0)
    return (t >= -EPS && t <= angDelta + EPS);
  else
    return (t <= EPS && t >= angDelta - EPS);
}

// вычисление точки на стене по параметру t in (0,1)
static Point pointOnWall(const WallConfig& wcfg, const Config& cfg, double t) {
  const Point& A = cfg.points[wcfg.idxA];
  const Point& B = cfg.points[wcfg.idxB];
  if (wcfg.type == WallType::FLAT) {
    return A * (1.0 - t) + B * t;
  } else {
    // spherical small arc from angA to angA + angDelta
    double ang = wcfg.angA + wcfg.angDelta * t;
    return Point(wcfg.center.x + wcfg.radius * std::cos(ang),
                 wcfg.center.y + wcfg.radius * std::sin(ang));
  }
}

// Единичный касательный вектор
static Point tangentOnWall(const WallConfig& wcfg, const Config& cfg,
                           double t) {
  const Point& A = cfg.points[wcfg.idxA];
  const Point& B = cfg.points[wcfg.idxB];
  if (wcfg.type == WallType::FLAT) {
    return norm(B - A);
  } else {
    double ang = wcfg.angA + wcfg.angDelta * t;
    double tangentAng = ang + (wcfg.angDelta >= 0 ? PI / 2.0 : -PI / 2.0);
    return norm(Point(std::cos(tangentAng), std::sin(tangentAng)));
  }
}

// ------------------- Пересечения луча с геометрией -------------------

// Ray: P(t) = P0 + dir * t, t >= 0
// Возвращает true + t + hit_point если есть пересечение с отрезком AB (плоская
// стена). Задаёт tRay: параметр луча; u: параметр отрезка (0..1).
static bool intersectRaySegment(const Point& P0, const Point& dir,
                                const Point& A, const Point& B, double& tRay,
                                double& uSeg) {
  Point v = B - A;
  // Решаем P0 + dir * t = A + v * u
  double denom = cross(dir, v);
  if (std::fabs(denom) < EPS) return false;  // параллельны
  Point rhs = A - P0;
  tRay = cross(rhs, v) / denom;
  uSeg = cross(rhs, dir) / denom;
  if (tRay > EPS && uSeg > EPS && uSeg < 1.0 - EPS) return true;
  return false;
}

// Пересечение луча с окружностью center,radius: вернуть минимальное
// положительное t, либо false
static bool intersectRayCircle(const Point& P0, const Point& dir,
                               const Point& center, double r, double& tOut1,
                               double& tOut2) {
  // (P0 + d t - C)^2 = r^2
  Point oc = P0 - center;
  double a = dot(dir, dir);
  double b = 2.0 * dot(oc, dir);
  double c = dot(oc, oc) - r * r;
  double disc = b * b - 4 * a * c;
  if (disc < -EPS) return false;
  disc = std::max(0.0, disc);
  double sqrtD = std::sqrt(disc);
  double t1 = (-b - sqrtD) / (2 * a);
  double t2 = (-b + sqrtD) / (2 * a);
  tOut1 = t1;
  tOut2 = t2;
  return true;
}

// Проверка попадания точки p в малую дугу wcfg
static bool pointOnArc(const WallConfig& wcfg, const Point& p) {
  double ang = angleOf(p - wcfg.center);
  return angleOnArc(wcfg.angA, wcfg.angDelta, ang);
}

// Найти ближайшее пересечение луча с любой стеной (не считая игнорирования
// своей же стены в очень маленькой окрестности) Возвращает индекс стены, точку
// пересечения, расстояние параметра tRay, и флаг, wasSpherical
static bool findNearestIntersection(const Config& cfg, const Point& P0,
                                    const Point& dir, int ignore_wall_idx,
                                    double ignore_coord_t, int& out_wall_idx,
                                    Point& out_pt, double& out_tRay,
                                    bool& out_isSpherical) {
  double bestT = std::numeric_limits<double>::infinity();
  bool found = false;
  for (size_t i = 0; i < cfg.walls.size(); ++i) {
    const WallConfig& w = cfg.walls[i];
    if (w.type == WallType::FLAT) {
      double tRay, uSeg;
      if (!intersectRaySegment(P0, dir, cfg.points[w.idxA], cfg.points[w.idxB],
                               tRay, uSeg))
        continue;
      if ((int)i == ignore_wall_idx) {
        // если пересекаем ту же стену — позволяем только если расстояние вдоль
        // от начала > EPS_MOVE
        if (tRay < EPS_MOVE * 10) continue;
      }
      if (tRay < bestT) {
        bestT = tRay;
        out_wall_idx = (int)i;
        out_pt = P0 + dir * tRay;
        out_tRay = tRay;
        out_isSpherical = false;
        found = true;
      }
    } else {
      double t1, t2;
      if (!intersectRayCircle(P0, dir, w.center, w.radius, t1, t2)) continue;
      if (t1 > t2) std::swap(t1, t2);

      // Проверяем каждую из двух точек
      for (int pass = 0; pass < 2; ++pass) {
        double tCand = (pass == 0 ? t1 : t2);
        if (tCand <= EPS) continue;  // точка позади луча

        Point ip = P0 + dir * tCand;

        // Проверяем, что лежит на нужной дуге
        if (!pointOnArc(w, ip)) continue;

        // Игнорируем самопересечения
        if ((int)i == ignore_wall_idx && tCand < EPS_MOVE * 10) continue;

        // Если это ближайшая стена — запоминаем
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

// --- Инициализация стен (выбор центров для сферических стен) -------
static bool initializeWalls(Config& cfg) {
  if (cfg.points.size() < 3) return false;
  double area = 0;
  for (size_t i = 0; i < cfg.points.size(); ++i) {
    const Point& a = cfg.points[i];
    const Point& b = cfg.points[(i + 1) % cfg.points.size()];
    area += cross(a, b);
  }
  // если area > 0, обход против часовой (внутри слева)
  bool ccw = area > 0.0;

  for (WallConfig& w : cfg.walls) {
    w.center_set = false;
    if (w.type == WallType::FLAT) continue;
    const Point& A = cfg.points[w.idxA];
    const Point& B = cfg.points[w.idxB];
    double chord = len(B - A);
    if (chord / 2.0 > w.radius + EPS) {
      std::cerr << "Слишком маленький радиус сферы(должен быть не меньше "
                   "размера стены пополам): "
                << w.idxA << " и " << w.idxB << "\n";
      return false;
    }
    auto centers = circleCentersFromTwoPointsAndRadius(A, B, w.radius);
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
        if (delta > 0)
          delta -= 2 * PI;
        else
          delta += 2 * PI;
      }
      bool centerIsLeft = cross(B - A, c - A) > 0;
      bool centerInside = (ccw ? centerIsLeft : !centerIsLeft);
      cands.push_back({c, angA, angB, delta, centerInside});
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
      if (rawDelta > 0)
        rawDelta -= 2 * PI;
      else
        rawDelta += 2 * PI;
    }
    if (std::fabs(rawDelta) > PI + 1e-6) {
      if (rawDelta > 0)
        rawDelta -= 2 * PI;
      else
        rawDelta += 2 * PI;
    }
    w.angDelta = rawDelta;
    w.angB = w.angA + w.angDelta;
    w.center_set = true;
  }
  return true;
}

// ------------------- Отражение -------------------

// отражение вектора dir относительно нормали n (единичная)
static Point reflect(const Point& dir, const Point& n) {
  double dDotN = dot(dir, n);
  Point r = dir - n * (2.0 * dDotN);
  return r;
}

// ------------------- Главная расчётная функция трассировки -------------------
static TraceResult traceRay(const Config& cfg) {
  TraceResult res;
  res.hit = false;
  res.reflection_index_when_hit = -1;
  res.segments.clear();

  /*if (cfg.emit_wall_index < 0 || cfg.emit_wall_index >= (int)cfg.walls.size())
  { std::cerr << "нет такой стены\n"; return res;
  }*/
  const WallConfig& w0 = cfg.walls[cfg.emit_wall_index];
  double t0 = cfg.emit_wall_t;
  Point pos = pointOnWall(w0, cfg, t0);

  Point tan = tangentOnWall(w0, cfg, t0);
  double phi = deg2rad(cfg.emit_angle_deg);
  /*if (!(phi > 0 && phi < PI)) {
    std::cerr << "угол не от 0 до 180\n";
    return res;
  }*/
  double baseAng = angleOf(tan);
  Point dir = fromAngle(baseAng + phi);

  double intensity = 1.0;
  int reflections = 0;
  Point curP = pos;
  Point curDir = norm(dir);
  int last_hit_wall = -1;

  for (int iter = 0;
       iter < cfg.max_reflections && intensity >= THRESHOLD_INTENSITY; ++iter) {
    int hit_wall = -1;
    Point hit_pt;
    double tRay;
    bool isSpherical;
    bool found = findNearestIntersection(cfg, curP, curDir, last_hit_wall, t0,
                                         hit_wall, hit_pt, tRay, isSpherical);
    if (!found) {
      RaySegment seg;
      seg.a = curP;
      seg.b = curP + curDir * 10000.0;
      seg.intensity = intensity;
      res.segments.push_back(seg);
      break;
    }
    RaySegment seg;
    seg.a = curP;
    seg.b = hit_pt;
    seg.intensity = intensity;
    res.segments.push_back(seg);

    Point T = cfg.target_point;
    Point v = seg.b - seg.a;
    Point w = T - seg.a;
    double vv = dot(v, v);
    double proj = (vv < EPS) ? 0.0 : dot(w, v) / vv;
    if (proj >= -EPS && proj <= 1.0 + EPS) {
      Point closest = seg.a + v * std::max(0.0, std::min(1.0, proj));
      double dist = len(closest - T);
      if (dist <= cfg.target_radius + 1e-7) {
        res.hit = true;
        res.reflection_index_when_hit = reflections;
        if (cfg.return_on_hit) return res;
      }
    }
    Point normal;
    if (!isSpherical) {
      Point A = cfg.points[cfg.walls[hit_wall].idxA];
      Point B = cfg.points[cfg.walls[hit_wall].idxB];
      Point tangent = norm(B - A);
      Point n1 = Point(-tangent.y, tangent.x);
      Point n2 = Point(tangent.y, -tangent.x);
      if (dot(curDir, n1) < 0)
        normal = n1;
      else
        normal = n2;
      normal = norm(normal);
    } else {
      Point n = norm(hit_pt - cfg.walls[hit_wall].center);
      if (dot(curDir, n) > 0) n = n * -1.0;
      normal = norm(n);
    }
    Point refl = reflect(curDir, normal);
    refl = norm(refl);
    curP = hit_pt + refl * EPS_MOVE;
    curDir = refl;
    last_hit_wall = hit_wall;
    // уменьшение интенсивности
    intensity *= cfg.reflectivity;
    ++reflections;
    // условие: если интенсивность упала ниже порога — перестать отражать
    if (intensity < THRESHOLD_INTENSITY) break;
  }
  // Если цикл завершён и не попали — res.hit остаётся false
  return res;
}

// ----------- Пример конфига (12 точек, невыпуклый многоугольник) -----------
static Config buildExampleConfig() {
  Config cfg;
  cfg.points = {{100, 100}, {300, 80},  {500, 120}, {620, 220},
                {580, 360}, {420, 400}, {300, 350}, {220, 420},
                {120, 380}, {80, 260},  {60, 180},  {90, 140}};
  size_t n = cfg.points.size();
  cfg.walls.clear();
  for (size_t i = 0; i < n; ++i) {
    WallConfig w;
    w.idxA = (int)i;
    w.idxB = (int)((i + 1) % n);
    // назначим некоторые стены сферическими:
    if (i == 1) {
      w.type = WallType::SPHERICAL_CONVEX;
      w.radius = 160.0;
    }  // выпуклая (внутрь)
    else if (i == 3) {
      w.type = WallType::SPHERICAL_CONCAVE;
      w.radius = 200.0;
    }  // вогнутая наружу
    else if (i == 6) {
      w.type = WallType::SPHERICAL_CONVEX;
      w.radius = 120.0;
    } else if (i == 9) {
      w.type = WallType::SPHERICAL_CONCAVE;
      w.radius = 140.0;
    } else {
      w.type = WallType::FLAT;
    }
    cfg.walls.push_back(w);
  }
  // настройки луча
  cfg.emit_wall_index =
      1;  // начать с первой стенки (которая у нас сферическая выпуклая)
  cfg.emit_wall_t = 0.35;  // между 0 и 1, концах не включая
  cfg.emit_angle_deg = 40.0;  // от 0 до 180
  cfg.reflectivity = 0.8;     // коэф отражения
  cfg.target_point = Point(420, 300);
  cfg.target_radius = 2.0;
  cfg.max_reflections = MAX_REFLECTIONS_DEFAULT;
  cfg.return_on_hit = false;
  // инициализация центров сферы:
  bool ok = initializeWalls(cfg);
  if (!ok) std::cerr << "Ошибка инициализации стен\n";
  return cfg;
}

// ------------------- Сохранение конфигурации -------------------
static bool SaveConfig(const std::string& filename, const Config& cfg) {
  std::ofstream out("templates/" + filename);
  if (!out.is_open()) {
    std::cerr << "SaveConfig: не удалось открыть файл для записи: " << filename
              << "\n";
    return false;
  }

  // --- Точки ---
  out << cfg.points.size() << "\n";
  for (const auto& p : cfg.points) out << p.x << " " << p.y << "\n";

  // --- Стены (только базовые данные, без center/углов) ---
  out << cfg.walls.size() << "\n";
  for (const auto& w : cfg.walls)
    out << static_cast<int>(w.type) << " " << w.idxA << " " << w.idxB << " "
        << w.radius << "\n";

  // --- Остальные параметры ---
  out << cfg.emit_wall_index << " " << cfg.emit_wall_t << " "
      << cfg.emit_angle_deg << "\n";
  out << cfg.reflectivity << " " << cfg.target_point.x << " "
      << cfg.target_point.y << " " << cfg.target_radius << "\n";
  out << cfg.max_reflections << " " << cfg.return_on_hit << "\n";

  return true;
}

static bool LoadConfig(const std::string& filename, Config& cfg) {
  std::ifstream in("templates/" + filename);
  if (!in.is_open()) {
    // 🔹 Здесь можно вернуть false, но без выброса ошибок — место для проверки:
    // if (!LoadConfig(...)) { // обработка отсутствия файла }
    return false;
  }

  size_t pointsCount = 0;
  in >> pointsCount;
  cfg.points.resize(pointsCount);
  for (auto& p : cfg.points) in >> p.x >> p.y;

  size_t wallsCount = 0;
  in >> wallsCount;
  cfg.walls.resize(wallsCount);
  for (auto& w : cfg.walls) {
    int typeInt;
    in >> typeInt >> w.idxA >> w.idxB >> w.radius;
    w.type = static_cast<WallType>(typeInt);
    w.center_set = false;  // будет вычислено initializeWalls()
  }

  in >> cfg.emit_wall_index >> cfg.emit_wall_t >> cfg.emit_angle_deg;
  in >> cfg.reflectivity >> cfg.target_point.x >> cfg.target_point.y >>
      cfg.target_radius;
  in >> cfg.max_reflections >> cfg.return_on_hit;

  return true;
}

// ------------------- Визуализация SFML -------------------

int main() {
  Config cfg = buildExampleConfig();

  // Переменные, которые зависят от конфига и определяются динамически
  bool walls_changed = true;
  TraceResult trace;
  std::vector<sf::VertexArray> wall_draws;
  sf::CircleShape targetCircle((float)cfg.target_radius);
  std::vector<sf::VertexArray> ray_draws;
  sf::VertexArray dir_indicator(sf::Lines, 2);

  // Примеры для теста динамики
  std::vector<std::string> examples = {};
  int cur_example = 0;

  // SFML окно
  sf::RenderWindow window(sf::VideoMode(800, 600), "Ray Tracer SFML - Backend");
  window.setFramerateLimit(60);

  // основное окно
  while (window.isOpen()) {
    sf::Event e;
    while (window.pollEvent(e)) {
      if (e.type == sf::Event::Closed) window.close();
      if (e.KeyReleased && e.key.code == sf::Keyboard::Add) {  // Numpad plus
        walls_changed = true;
        cur_example = (cur_example + 1) % examples.size();

        LoadConfig(examples[cur_example], cfg);
      }
      // Здесь frontend ивенты, типа нажатия мыши и т.п.
    }

    // Динамический перерасчёт луча
    if (walls_changed) {
      trace = TraceResult();
      wall_draws = {};
      ray_draws = {};
      dir_indicator = sf::VertexArray(sf::Lines, 2);

      if (!initializeWalls(cfg)) {
        std::cerr << "Ошибка инициализации стен\n";
        return -1;
      }

      trace = traceRay(cfg);

      for (const WallConfig& w : cfg.walls) {
        if (w.type == WallType::FLAT) {
          sf::VertexArray va(sf::LinesStrip, 2);
          Point A = cfg.points[w.idxA], B = cfg.points[w.idxB];
          va[0].position = sf::Vector2f((float)A.x, (float)A.y);
          va[1].position = sf::Vector2f((float)B.x, (float)B.y);
          va[0].color = sf::Color::White;
          va[1].color = sf::Color::White;
          wall_draws.push_back(va);
        } else {
          // дуга как ломаная
          sf::VertexArray va(sf::LinesStrip);
          int steps = 40;
          for (int k = 0; k <= steps; ++k) {
            double t = double(k) / double(steps);
            double ang = w.angA + w.angDelta * t;
            Point p(w.center.x + w.radius * std::cos(ang),
                    w.center.y + w.radius * std::sin(ang));
            va.append(sf::Vertex(sf::Vector2f((float)p.x, (float)p.y),
                                 sf::Color::White));
          }
          wall_draws.push_back(va);
        }
      }

      targetCircle.setRadius((float)cfg.target_radius);
      targetCircle.setOrigin((float)cfg.target_radius,
                             (float)cfg.target_radius);
      targetCircle.setPosition((float)cfg.target_point.x,
                               (float)cfg.target_point.y);
      targetCircle.setFillColor(sf::Color(0, 255, 0, 130));
      targetCircle.setOutlineThickness(0.5f);
      targetCircle.setOutlineColor(sf::Color::Green);

      for (const RaySegment& s : trace.segments) {
        sf::VertexArray va(sf::LinesStrip, 2);
        va[0].position = sf::Vector2f((float)s.a.x, (float)s.a.y);
        va[1].position = sf::Vector2f((float)s.b.x, (float)s.b.y);
        // прозрачность зависит от intensity
        int alpha = (int)std::round(std::min(1.0, s.intensity) * 255.0);
        if (alpha < 1) alpha = 1;
        sf::Color c(255, 255, 255, (sf::Uint8)alpha);
        va[0].color = c;
        va[1].color = c;
        ray_draws.push_back(va);
        // маленький выделяющийся отрезок направления у начальной точки первого
        // сегмента
      }

      if (!trace.segments.empty()) {
        RaySegment first = trace.segments.front();
        Point dir = norm(first.b - first.a);
        Point p1 = first.a;
        Point p2 = first.a + dir * 18.0;
        dir_indicator[0].position = sf::Vector2f((float)p1.x, (float)p1.y);
        dir_indicator[1].position = sf::Vector2f((float)p2.x, (float)p2.y);
        dir_indicator[0].color = sf::Color::Red;
        dir_indicator[1].color = sf::Color::Red;
      }

      walls_changed = false;
    }

    window.clear(sf::Color(30, 30, 30));
    for (auto& va : wall_draws) window.draw(va);
    window.draw(targetCircle);
    for (auto& va : ray_draws) window.draw(va);
    if (trace.segments.size() > 0) window.draw(dir_indicator);
    window.display();
  }

  return 0;
}
