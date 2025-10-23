#include "MirrorRoomApp.h"
#include "Constants.h"
#include "WallUtils.h"
#include <cmath>
#include <algorithm>

MirrorRoomApp::MirrorRoomApp() : window(sf::VideoMode(1400, 900), "Mirror Room - Optical Experiments"),
wallsChanged(true), isDrawingPolygon(false), showHelp(false), showInstructions(true),
isMouseOverEmitter(false), isMiddleMousePressed(false), zoomLevel(1.0f), ctrlPressed(false) {
    window.setFramerateLimit(60);
    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Failed to initialize ImGui-SFML" << std::endl;
    }

    if (!font.loadFromFile("arial.ttf")) {
        std::cout << "Warning: Could not load font, using default" << std::endl;
    }

    roomView = sf::View(sf::FloatRect(0, 0, 1400, 900));
    uiView = sf::View(sf::FloatRect(0, 0, 1400, 900));
    viewCenter = sf::Vector2f(700 + roomOffsetX, 450);

    setupImGuiStyle();
    createExampleRoom();
    updateTrace();
}

MirrorRoomApp::~MirrorRoomApp() {
    ImGui::SFML::Shutdown();
}

void MirrorRoomApp::setupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.12f, 0.94f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.51f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.91f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
}

void MirrorRoomApp::createExampleRoom() {
    config.points.clear();
    config.walls.clear();

    config.points = {
        {350 + roomOffsetX, 200}, {550 + roomOffsetX, 150}, {750 + roomOffsetX, 180}, {850 + roomOffsetX, 300},
        {800 + roomOffsetX, 500}, {650 + roomOffsetX, 650}, {450 + roomOffsetX, 600}, {300 + roomOffsetX, 450},
        {280 + roomOffsetX, 300}
    };

    size_t n = config.points.size();
    for (size_t i = 0; i < n; ++i) {
        WallConfig w;
        w.idxA = (int)i;
        w.idxB = (int)((i + 1) % n);

        if (i % 4 == 0) {
            w.type = WallType::SPHERICAL_CONVEX;
            w.radius = 200.0;
            w.reflectivity = 0.85;
        }
        else if (i % 4 == 2) {
            w.type = WallType::SPHERICAL_CONCAVE;
            w.radius = 250.0;
            w.reflectivity = 0.9;
        }
        else {
            w.type = WallType::FLAT;
            w.reflectivity = 0.8;
        }
        config.walls.push_back(w);
    }

    config.target_point = Point(600 + roomOffsetX, 400);
    config.emit_wall_index = 3;
    config.emit_angle_deg = 75.0;

    initializeWalls(config);
    setStatus("Example room created. Configure parameters and try to hit the target!");
}

void MirrorRoomApp::run() {
    sf::Clock deltaClock;
    while (window.isOpen()) {
        handleEvents();
        ImGui::SFML::Update(window, deltaClock.restart());

        update();
        render();
    }
}

void MirrorRoomApp::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        ImGui::SFML::ProcessEvent(event);

        if (event.type == sf::Event::Closed) {
            window.close();
        }

        if (event.type == sf::Event::MouseButtonPressed && !ImGui::GetIO().WantCaptureMouse) {
            handleMouseClick(event.mouseButton);
        }

        if (event.type == sf::Event::MouseButtonReleased && !ImGui::GetIO().WantCaptureMouse) {
            handleMouseRelease(event.mouseButton);
        }

        if (event.type == sf::Event::KeyPressed) {
            handleKeyPress(event.key);
        }

        if (event.type == sf::Event::KeyReleased) {
            if (event.key.code == sf::Keyboard::LControl || event.key.code == sf::Keyboard::RControl) {
                ctrlPressed = false;
            }
        }

        if (event.type == sf::Event::MouseWheelScrolled && !ImGui::GetIO().WantCaptureMouse) {
            handleMouseWheel(event.mouseWheelScroll);
        }

        if (event.type == sf::Event::MouseMoved) {
            handleMouseMove(event.mouseMove);
        }
    }
}

void MirrorRoomApp::handleMouseMove(const sf::Event::MouseMoveEvent& mouse) {
    sf::Vector2f currentMousePos = window.mapPixelToCoords(sf::Vector2i(mouse.x, mouse.y), roomView);

    if (isMiddleMousePressed) {
        sf::Vector2f delta = lastMousePos - currentMousePos;
        viewCenter += delta;
        roomView.setCenter(viewCenter);
    }

    lastMousePos = currentMousePos;
    updateEmitterHoverState();
}

void MirrorRoomApp::handleMouseWheel(const sf::Event::MouseWheelScrollEvent& wheel) {
    if (ctrlPressed) {
        float zoomFactor = 1.1f;
        if (wheel.delta > 0) {
            zoomLevel *= zoomFactor;
        }
        else {
            zoomLevel /= zoomFactor;
        }

        zoomLevel = std::max(0.1f, std::min(5.0f, zoomLevel));

        roomView.setSize(1400 / zoomLevel, 900 / zoomLevel);
        roomView.setCenter(viewCenter);
    }
    else if (isMouseOverEmitter && !isDrawingPolygon) {
        double angleStep = 1.0;
        if (wheel.delta > 0) {
            config.emit_angle_deg += angleStep;
        }
        else {
            config.emit_angle_deg -= angleStep;
        }

        while (config.emit_angle_deg < 0) config.emit_angle_deg += 360;
        while (config.emit_angle_deg >= 360) config.emit_angle_deg -= 360;
        if (config.emit_angle_deg > 180) config.emit_angle_deg = 360 - config.emit_angle_deg;

        wallsChanged = true;
        setStatus("Ray angle: " + std::to_string(static_cast<int>(config.emit_angle_deg)) + "°");
    }
}

void MirrorRoomApp::updateEmitterHoverState() {
    const WallConfig& wall = config.walls[config.emit_wall_index];
    Point emitterPos = pointOnWall(wall, config, config.emit_wall_t);
    Point mousePoint(static_cast<double>(lastMousePos.x), static_cast<double>(lastMousePos.y));

    isMouseOverEmitter = (len(mousePoint - emitterPos) < 20.0);
}

void MirrorRoomApp::handleMouseClick(const sf::Event::MouseButtonEvent& mouse) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(mouse.x, mouse.y), roomView);

    if (mouse.button == sf::Mouse::Middle) {
        isMiddleMousePressed = true;
        mouseDragStart = mousePos;
        return;
    }

    Point clickPoint(static_cast<double>(mousePos.x), static_cast<double>(mousePos.y));

    if (mouse.button == sf::Mouse::Left) {
        if (isDrawingPolygon) {
            config.points.push_back(clickPoint);
            if (config.points.size() >= 3) {
                updateWallsFromPoints();
            }
            wallsChanged = true;
            setStatus("Added point #" + std::to_string(config.points.size()));
        }
        else {
            for (size_t i = 0; i < config.walls.size(); ++i) {
                const auto& wall = config.walls[i];
                if (isPointNearWall(clickPoint, wall, 15.0)) {
                    config.emit_wall_index = static_cast<int>(i);
                    config.emit_wall_t = getWallParameter(clickPoint, wall);
                    wallsChanged = true;
                    setStatus("Light source set on wall " + std::to_string(i + 1));
                    break;
                }
            }
        }
    }
    else if (mouse.button == sf::Mouse::Right) {
        if (!isDrawingPolygon) {
            config.target_point = clickPoint;
            wallsChanged = true;
            setStatus("Target moved to new position");
        }
    }
}

void MirrorRoomApp::handleMouseRelease(const sf::Event::MouseButtonEvent& mouse) {
    if (mouse.button == sf::Mouse::Middle) {
        isMiddleMousePressed = false;
    }
}

void MirrorRoomApp::handleKeyPress(const sf::Event::KeyEvent& key) {
    switch (key.code) {
    case sf::Keyboard::H:
        showHelp = !showHelp;
        break;
    case sf::Keyboard::I:
        showInstructions = !showInstructions;
        break;
    case sf::Keyboard::C:
        clearRoom();
        break;
    case sf::Keyboard::R:
        resetToExample();
        break;
    case sf::Keyboard::S:
        saveConfig("mirror_room_config.txt");
        break;
    case sf::Keyboard::L:
        loadConfig("mirror_room_config.txt");
        break;
    case sf::Keyboard::D:
        isDrawingPolygon = !isDrawingPolygon;
        if (isDrawingPolygon) {
            config.points.clear();
            setStatus("Drawing mode: Click LMB to add room points");
        }
        else {
            if (config.points.size() >= 3) {
                updateWallsFromPoints();
                setStatus("Room created! Now configure the mirrors.");
            }
            else {
                setStatus("Need at least 3 points to create a room");
                isDrawingPolygon = true;
            }
        }
        break;
    case sf::Keyboard::LControl:
    case sf::Keyboard::RControl:
        ctrlPressed = true;
        break;
    case sf::Keyboard::Space:
        viewCenter = sf::Vector2f(700 + roomOffsetX, 450);
        roomView.setCenter(viewCenter);
        zoomLevel = 1.0f;
        roomView.setSize(1400, 900);
        setStatus("Camera position reset");
        break;
    }
}

bool MirrorRoomApp::isPointNearWall(const Point& p, const WallConfig& wall, double threshold) {
    if (wall.type == WallType::FLAT) {
        Point A = config.points[wall.idxA];
        Point B = config.points[wall.idxB];
        Point AB = B - A;
        Point AP = p - A;
        double t = dot(AP, AB) / dot(AB, AB);
        if (t < 0 || t > 1) return false;
        Point projection = A + AB * t;
        return len(p - projection) < threshold;
    }
    else {
        double distToCenter = len(p - wall.center);
        return std::abs(distToCenter - wall.radius) < threshold;
    }
}

double MirrorRoomApp::getWallParameter(const Point& p, const WallConfig& wall) {
    if (wall.type == WallType::FLAT) {
        Point A = config.points[wall.idxA];
        Point B = config.points[wall.idxB];
        Point AB = B - A;
        Point AP = p - A;
        return dot(AP, AB) / dot(AB, AB);
    }
    else {
        double angle = angleOf(p - wall.center);
        return (angle - wall.angA) / wall.angDelta;
    }
}

void MirrorRoomApp::updateWallsFromPoints() {
    config.walls.clear();
    for (size_t i = 0; i < config.points.size(); ++i) {
        WallConfig wall;
        wall.idxA = static_cast<int>(i);
        wall.idxB = static_cast<int>((i + 1) % config.points.size());
        wall.type = WallType::FLAT;
        wall.reflectivity = 0.8;
        config.walls.push_back(wall);
    }
    initializeWalls(config);
}

void MirrorRoomApp::update() {
    if (wallsChanged) {
        updateTrace();
        wallsChanged = false;
    }

    if (statusTimer.getElapsedTime().asSeconds() > 4.0f) {
        statusMessage.clear();
    }
}

void MirrorRoomApp::updateTrace() {
    if (config.points.size() >= 3 && initializeWalls(config)) {
        traceResult = traceRay(config);
    }
}

void MirrorRoomApp::render() {
    window.clear(bgColor);

    roomView.setViewport(sf::FloatRect(0, 0, 1, 1));
    window.setView(roomView);

    drawRoom();
    drawTrace();

    window.setView(uiView);
    drawUI();

    window.display();
}

void MirrorRoomApp::drawRoom() {
    for (const auto& wall : config.walls) {
        sf::Color wallColor = flatWallColor;
        if (wall.type == WallType::SPHERICAL_CONVEX) wallColor = convexWallColor;
        else if (wall.type == WallType::SPHERICAL_CONCAVE) wallColor = concaveWallColor;

        if (wall.type == WallType::FLAT) {
            drawFlatWall(wall, wallColor);
        }
        else {
            drawSphericalWall(wall, wallColor);
        }
    }

    drawTarget();
    drawEmitter();

    if (isDrawingPolygon) {
        for (const auto& point : config.points) {
            sf::CircleShape dot(6.0f);
            dot.setFillColor(sf::Color::Yellow);
            dot.setOutlineColor(sf::Color::Black);
            dot.setOutlineThickness(1.0f);
            dot.setOrigin(6.0f, 6.0f);
            dot.setPosition(static_cast<float>(point.x), static_cast<float>(point.y));
            window.draw(dot);
        }

        if (config.points.size() > 1) {
            for (size_t i = 1; i < config.points.size(); ++i) {
                sf::VertexArray line(sf::LinesStrip, 2);
                line[0].position = sf::Vector2f(static_cast<float>(config.points[i - 1].x),
                    static_cast<float>(config.points[i - 1].y));
                line[1].position = sf::Vector2f(static_cast<float>(config.points[i].x),
                    static_cast<float>(config.points[i].y));
                line[0].color = sf::Color(255, 255, 255, 150);
                line[1].color = sf::Color(255, 255, 255, 150);
                window.draw(line);
            }
        }
    }
}

void MirrorRoomApp::drawFlatWall(const WallConfig& wall, const sf::Color& color) {
    sf::VertexArray line(sf::LinesStrip, 2);
    Point A = config.points[wall.idxA];
    Point B = config.points[wall.idxB];

    line[0].position = sf::Vector2f(static_cast<float>(A.x), static_cast<float>(A.y));
    line[1].position = sf::Vector2f(static_cast<float>(B.x), static_cast<float>(B.y));
    line[0].color = color;
    line[1].color = color;

    window.draw(line);
}

void MirrorRoomApp::drawSphericalWall(const WallConfig& wall, const sf::Color& color) {
    sf::VertexArray arc(sf::LineStrip);
    int steps = 60;

    for (int i = 0; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double angle = wall.angA + wall.angDelta * t;
        Point p(wall.center.x + wall.radius * std::cos(angle),
            wall.center.y + wall.radius * std::sin(angle));
        arc.append(sf::Vertex(sf::Vector2f(static_cast<float>(p.x), static_cast<float>(p.y)), color));
    }

    window.draw(arc);
}

void MirrorRoomApp::drawTarget() {
    sf::CircleShape target(static_cast<float>(config.target_radius));
    if (traceResult.hit) {
        target.setFillColor(targetHitColor);
    }
    else {
        target.setFillColor(targetColor);
    }
    target.setOutlineColor(sf::Color(255, 255, 255, 200));
    target.setOutlineThickness(2.0f);
    target.setOrigin(static_cast<float>(config.target_radius), static_cast<float>(config.target_radius));
    target.setPosition(static_cast<float>(config.target_point.x), static_cast<float>(config.target_point.y));
    window.draw(target);

    sf::VertexArray cross(sf::Lines, 4);
    float size = static_cast<float>(config.target_radius * 0.7f);
    cross[0].position = sf::Vector2f(static_cast<float>(config.target_point.x - size),
        static_cast<float>(config.target_point.y));
    cross[1].position = sf::Vector2f(static_cast<float>(config.target_point.x + size),
        static_cast<float>(config.target_point.y));
    cross[2].position = sf::Vector2f(static_cast<float>(config.target_point.x),
        static_cast<float>(config.target_point.y - size));
    cross[3].position = sf::Vector2f(static_cast<float>(config.target_point.x),
        static_cast<float>(config.target_point.y + size));
    for (int i = 0; i < 4; ++i) cross[i].color = sf::Color(255, 255, 255, 200);
    window.draw(cross);
}

void MirrorRoomApp::drawEmitter() {
    const WallConfig& wall = config.walls[config.emit_wall_index];
    Point emitterPos = pointOnWall(wall, config, config.emit_wall_t);
    Point tangent = tangentOnWall(wall, config, config.emit_wall_t);
    double angle = deg2rad(config.emit_angle_deg);
    Point direction = fromAngle(angleOf(tangent) + angle);

    sf::CircleShape emitter(8.0f);
    emitter.setFillColor(isMouseOverEmitter ? sf::Color(255, 150, 150) : emitterColor);
    emitter.setOutlineColor(sf::Color::White);
    emitter.setOutlineThickness(isMouseOverEmitter ? 2.5f : 1.5f);
    emitter.setOrigin(8.0f, 8.0f);
    emitter.setPosition(static_cast<float>(emitterPos.x), static_cast<float>(emitterPos.y));
    window.draw(emitter);

    sf::VertexArray dirLine(sf::Lines, 2);
    dirLine[0].position = sf::Vector2f(static_cast<float>(emitterPos.x), static_cast<float>(emitterPos.y));
    dirLine[1].position = sf::Vector2f(static_cast<float>(emitterPos.x + direction.x * 25.0),
        static_cast<float>(emitterPos.y + direction.y * 25.0));
    dirLine[0].color = isMouseOverEmitter ? sf::Color(255, 150, 150) : emitterColor;
    dirLine[1].color = isMouseOverEmitter ? sf::Color(255, 150, 150) : emitterColor;
    window.draw(dirLine);

    if (isMouseOverEmitter) {
        sf::Text angleText;
        angleText.setFont(font);
        angleText.setString(std::to_string(static_cast<int>(config.emit_angle_deg)) + "°");
        angleText.setCharacterSize(12);
        angleText.setFillColor(sf::Color::White);
        angleText.setStyle(sf::Text::Bold);
        angleText.setPosition(static_cast<float>(emitterPos.x) + 15.0f, static_cast<float>(emitterPos.y) - 20.0f);

        sf::RectangleShape textBg(sf::Vector2f(40.0f, 20.0f));
        textBg.setFillColor(sf::Color(0, 0, 0, 150));
        textBg.setPosition(static_cast<float>(emitterPos.x) + 10.0f, static_cast<float>(emitterPos.y) - 25.0f);
        window.draw(textBg);
        window.draw(angleText);
    }
}

void MirrorRoomApp::drawTrace() {
    for (const auto& segment : traceResult.segments) {
        sf::VertexArray line(sf::LinesStrip, 2);
        line[0].position = sf::Vector2f(static_cast<float>(segment.a.x), static_cast<float>(segment.a.y));
        line[1].position = sf::Vector2f(static_cast<float>(segment.b.x), static_cast<float>(segment.b.y));

        sf::Uint8 alpha = static_cast<sf::Uint8>(segment.intensity * 255.0);
        float thickness = 1.0f + segment.intensity * 2.0f;

        for (float offset = -thickness / 2; offset <= thickness / 2; offset += 0.5f) {
            sf::VertexArray thickLine(sf::LinesStrip, 2);
            thickLine[0].position = sf::Vector2f(
                static_cast<float>(segment.a.x) + offset,
                static_cast<float>(segment.a.y) + offset
            );
            thickLine[1].position = sf::Vector2f(
                static_cast<float>(segment.b.x) + offset,
                static_cast<float>(segment.b.y) + offset
            );
            sf::Color color(rayColor.r, rayColor.g, rayColor.b, alpha / 3);
            thickLine[0].color = color;
            thickLine[1].color = color;
            window.draw(thickLine);
        }

        line[0].color = sf::Color(rayColor.r, rayColor.g, rayColor.b, alpha);
        line[1].color = sf::Color(rayColor.r, rayColor.g, rayColor.b, alpha);
        window.draw(line);
    }

    for (const auto& point : traceResult.intersection_points) {
        sf::CircleShape dot(4.0f);
        dot.setFillColor(intersectionColor);
        dot.setOutlineColor(sf::Color::White);
        dot.setOutlineThickness(1.0f);
        dot.setOrigin(4.0f, 4.0f);
        dot.setPosition(static_cast<float>(point.x), static_cast<float>(point.y));
        window.draw(dot);
    }
}

void MirrorRoomApp::drawUI() {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
    ImGui::SetNextWindowSize(ImVec2(380.0f, 550.0f));

    if (ImGui::Begin("Mirror Room Lab", nullptr, ImGuiWindowFlags_NoResize)) {

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.20f, 0.25f, 1.0f));
        if (ImGui::BeginChild("Status", ImVec2(0, 80), true)) {
            if (!statusMessage.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", statusMessage.c_str());
            }
            else {
                if (traceResult.hit) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Target hit!");
                    ImGui::SameLine();
                    ImGui::Text("Reflections: %d", traceResult.reflection_index_when_hit);
                }
                else {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Target not reached");
                }
            }
            ImGui::Text("Zoom: %.1fx | Ctrl + Mouse Wheel to zoom", zoomLevel);
            ImGui::Text("Middle Mouse: Move view | Space: Reset view");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Quick Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Instructions (I)")) showInstructions = true;
            ImGui::SameLine();
            if (ImGui::Button("Clear (C)")) clearRoom();
            ImGui::SameLine();
            if (ImGui::Button("Example (R)")) resetToExample();

            ImGui::Spacing();

            if (ImGui::Button(isDrawingPolygon ? "Finish Drawing (D)" : "Draw Room (D)")) {
                isDrawingPolygon = !isDrawingPolygon;
                if (!isDrawingPolygon && config.points.size() >= 3) {
                    updateWallsFromPoints();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Save (S)")) saveConfig("mirror_room_config.txt");
            ImGui::SameLine();
            if (ImGui::Button("Load (L)")) loadConfig("mirror_room_config.txt");

            ImGui::Spacing();
            if (ImGui::Button("Reset View (Space)")) {
                viewCenter = sf::Vector2f(700 + roomOffsetX, 450);
                roomView.setCenter(viewCenter);
                zoomLevel = 1.0f;
                roomView.setSize(1400, 900);
                setStatus("Camera position reset");
            }
        }

        if (ImGui::CollapsingHeader("Ray Parameters")) {
            ImGui::Text("Light Source:");
            int emit_wall = config.emit_wall_index;
            if (ImGui::SliderInt("Wall##emitwall", &emit_wall, 0, static_cast<int>(config.walls.size()) - 1)) {
                config.emit_wall_index = emit_wall;
                wallsChanged = true;
            }

            float emit_t = static_cast<float>(config.emit_wall_t);
            if (ImGui::SliderFloat("Position##emitpos", &emit_t, 0.0f, 1.0f)) {
                config.emit_wall_t = static_cast<double>(emit_t);
                wallsChanged = true;
            }

            float emit_angle = static_cast<float>(config.emit_angle_deg);
            if (ImGui::SliderFloat("Emission Angle##angle", &emit_angle, 0.0f, 180.0f)) {
                config.emit_angle_deg = static_cast<double>(emit_angle);
                wallsChanged = true;
            }

            ImGui::Text("Ray Behavior:");
            int max_reflect = config.max_reflections;
            if (ImGui::SliderInt("Max Reflections##maxref", &max_reflect, 1, 1000)) {
                config.max_reflections = max_reflect;
                wallsChanged = true;
            }

            bool return_on_hit = config.return_on_hit;
            if (ImGui::Checkbox("Stop on hit##stoponhit", &return_on_hit)) {
                config.return_on_hit = return_on_hit;
                wallsChanged = true;
            }
        }

        if (ImGui::CollapsingHeader("Target")) {
            float target_x = static_cast<float>(config.target_point.x);
            float target_y = static_cast<float>(config.target_point.y);
            if (ImGui::SliderFloat("Target X##targetx", &target_x, 0.0f, 1400.0f)) {
                config.target_point.x = static_cast<double>(target_x);
                wallsChanged = true;
            }
            if (ImGui::SliderFloat("Target Y##targety", &target_y, 0.0f, 900.0f)) {
                config.target_point.y = static_cast<double>(target_y);
                wallsChanged = true;
            }

            float target_rad = static_cast<float>(config.target_radius);
            if (ImGui::SliderFloat("Target Radius##targetrad", &target_rad, 1.0f, 50.0f)) {
                config.target_radius = static_cast<double>(target_rad);
                wallsChanged = true;
            }
        }

        if (ImGui::CollapsingHeader("Wall Management") && !config.walls.empty()) {
            ImGui::Text("Legend:");
            ImGui::BulletText("Flat Mirrors");
            ImGui::BulletText("Convex Mirrors");
            ImGui::BulletText("Concave Mirrors");
            ImGui::Separator();

            for (size_t i = 0; i < config.walls.size(); ++i) {
                std::string wallLabel = "Wall " + std::to_string(i + 1);
                if (ImGui::TreeNode(wallLabel.c_str())) {
                    WallConfig& wall = config.walls[i];

                    int type = static_cast<int>(wall.type);
                    const char* types[] = { "Flat", "Convex", "Concave" };
                    if (ImGui::Combo("Mirror Type##type", &type, types, 3)) {
                        wall.type = static_cast<WallType>(type);
                        wallsChanged = true;
                    }

                    if (wall.type != WallType::FLAT) {
                        float radius = static_cast<float>(wall.radius);
                        if (ImGui::SliderFloat("Radius##radius", &radius, 50.0f, 500.0f)) {
                            wall.radius = static_cast<double>(radius);
                            wallsChanged = true;
                        }
                    }

                    float reflectivity = static_cast<float>(wall.reflectivity);
                    if (ImGui::SliderFloat("Reflectivity##refl", &reflectivity, 0.1f, 1.0f)) {
                        wall.reflectivity = static_cast<double>(reflectivity);
                        wallsChanged = true;
                    }

                    ImGui::TreePop();
                }
            }
        }

        if (ImGui::CollapsingHeader("Help")) {
            if (ImGui::Button("Show Instructions")) showInstructions = true;
            ImGui::SameLine();
            if (ImGui::Button("Hotkeys")) showHelp = true;

            ImGui::Spacing();
            ImGui::Text("New Features:");
            ImGui::BulletText("Ctrl + Mouse Wheel: Zoom room");
            ImGui::BulletText("Middle Mouse Button: Move view");
            ImGui::BulletText("Space: Reset camera position");
            ImGui::BulletText("Mouse Wheel on emitter: Change angle (1° steps)");
            ImGui::BulletText("Rays can now hit target directly");
            ImGui::BulletText("Target turns red when hit");
            ImGui::BulletText("Room shifted to the right");
        }
    }
    ImGui::End();

    if (showInstructions) {
        ImGui::SetNextWindowSize(ImVec2(500, 650), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("User Guide", &showInstructions)) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Welcome to Mirror Room Lab!");
            ImGui::Separator();

            ImGui::TextWrapped("This is an optical experiment simulator in a mirror room. Your goal is to configure parameters so that the light ray reaches the target.");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Basic Steps:");
            ImGui::BulletText("Create room: Press 'D' and click LMB to draw a polygon");
            ImGui::BulletText("Set source: Click LMB on any wall");
            ImGui::BulletText("Set target: Click RMB at desired location");
            ImGui::BulletText("Configure mirrors: Choose type and parameters for each wall");
            ImGui::BulletText("Run experiment: Ray automatically recalculates");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Camera Controls:");
            ImGui::BulletText("Middle Mouse Button: Drag to move view");
            ImGui::BulletText("Ctrl + Mouse Wheel: Zoom in/out");
            ImGui::BulletText("Space: Reset camera to default position");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Mouse Controls:");
            ImGui::BulletText("LMB on wall: Set light source");
            ImGui::BulletText("RMB: Set target");
            ImGui::BulletText("Mouse wheel on emitter: Change ray direction (1° steps)");
            ImGui::BulletText("Drawing mode (D): LMB adds room points");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Hotkeys:");
            ImGui::BulletText("D: Toggle drawing mode");
            ImGui::BulletText("C: Clear room");
            ImGui::BulletText("R: Load example");
            ImGui::BulletText("S: Save configuration");
            ImGui::BulletText("L: Load configuration");
            ImGui::BulletText("H: Show help");
            ImGui::BulletText("I: Show instructions");
            ImGui::BulletText("Space: Reset camera");

            ImGui::Spacing();
            if (ImGui::Button("Close")) showInstructions = false;
        }
        ImGui::End();
    }

    if (showHelp) {
        ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Hotkeys", &showHelp)) {
            ImGui::Columns(2, "hotkeys");
            ImGui::Text("Key"); ImGui::NextColumn();
            ImGui::Text("Action"); ImGui::NextColumn();
            ImGui::Separator();

            ImGui::Text("D"); ImGui::NextColumn();
            ImGui::Text("Drawing mode"); ImGui::NextColumn();

            ImGui::Text("C"); ImGui::NextColumn();
            ImGui::Text("Clear room"); ImGui::NextColumn();

            ImGui::Text("R"); ImGui::NextColumn();
            ImGui::Text("Example room"); ImGui::NextColumn();

            ImGui::Text("S"); ImGui::NextColumn();
            ImGui::Text("Save"); ImGui::NextColumn();

            ImGui::Text("L"); ImGui::NextColumn();
            ImGui::Text("Load"); ImGui::NextColumn();

            ImGui::Text("H"); ImGui::NextColumn();
            ImGui::Text("Help"); ImGui::NextColumn();

            ImGui::Text("I"); ImGui::NextColumn();
            ImGui::Text("Instructions"); ImGui::NextColumn();

            ImGui::Text("Space"); ImGui::NextColumn();
            ImGui::Text("Reset camera"); ImGui::NextColumn();

            ImGui::Text("Ctrl+Wheel"); ImGui::NextColumn();
            ImGui::Text("Zoom"); ImGui::NextColumn();

            ImGui::Text("Middle Mouse"); ImGui::NextColumn();
            ImGui::Text("Move view"); ImGui::NextColumn();

            ImGui::Columns(1);
            ImGui::Spacing();
            if (ImGui::Button("Close")) showHelp = false;
        }
        ImGui::End();
    }

    ImGui::SFML::Render(window);
}

void MirrorRoomApp::setStatus(const std::string& message) {
    statusMessage = message;
    statusTimer.restart();
}

void MirrorRoomApp::clearRoom() {
    config.points.clear();
    config.walls.clear();
    wallsChanged = true;
    isDrawingPolygon = false;
    setStatus("Room cleared. Start by drawing a new room (D)");
}

void MirrorRoomApp::resetToExample() {
    createExampleRoom();
    wallsChanged = true;
    setStatus("Example room loaded. Configure parameters and try to hit the target!");
}

bool MirrorRoomApp::saveConfig(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << config.points.size() << "\n";
    for (const auto& p : config.points) {
        file << p.x << " " << p.y << "\n";
    }

    file << config.walls.size() << "\n";
    for (const auto& w : config.walls) {
        file << static_cast<int>(w.type) << " " << w.idxA << " " << w.idxB << " "
            << w.radius << " " << w.reflectivity << "\n";
    }

    file << config.emit_wall_index << " " << config.emit_wall_t << " "
        << config.emit_angle_deg << " " << config.target_point.x << " " << config.target_point.y << " "
        << config.target_radius << " " << config.max_reflections << " "
        << config.return_on_hit << "\n";

    setStatus("Configuration saved: " + filename);
    return true;
}

bool MirrorRoomApp::loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        setStatus("Error: File " + filename + " not found");
        return false;
    }

    Config newConfig;
    size_t count;

    file >> count;
    newConfig.points.resize(count);
    for (auto& p : newConfig.points) {
        file >> p.x >> p.y;
    }

    file >> count;
    newConfig.walls.resize(count);
    for (auto& w : newConfig.walls) {
        int type;
        file >> type >> w.idxA >> w.idxB >> w.radius >> w.reflectivity;
        w.type = static_cast<WallType>(type);
    }

    file >> newConfig.emit_wall_index >> newConfig.emit_wall_t
        >> newConfig.emit_angle_deg >> newConfig.target_point.x >> newConfig.target_point.y
        >> newConfig.target_radius >> newConfig.max_reflections
        >> newConfig.return_on_hit;

    config = newConfig;
    wallsChanged = true;
    setStatus("Configuration loaded: " + filename);
    return true;
}