#pragma once

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include "Config.h"
#include "TraceResult.h"
#include "RayTracing.h"
#include "WallInitialization.h"

class MirrorRoomApp {
private:
    sf::RenderWindow window;
    Config config;
    TraceResult traceResult;
    bool wallsChanged;
    bool isDrawingPolygon;
    bool showHelp;
    bool showInstructions;
    std::string statusMessage;
    sf::Clock statusTimer;

    sf::Color bgColor = sf::Color(20, 25, 35);
    sf::Color flatWallColor = sf::Color(120, 150, 200);
    sf::Color convexWallColor = sf::Color(200, 150, 120);
    sf::Color concaveWallColor = sf::Color(150, 200, 150);
    sf::Color targetColor = sf::Color(152, 195, 121, 180);
    sf::Color targetHitColor = sf::Color(224, 108, 117, 180);
    sf::Color rayColor = sf::Color(255, 220, 150);
    sf::Color emitterColor = sf::Color(224, 108, 117);
    sf::Color intersectionColor = sf::Color(86, 156, 214);

    bool isMouseOverEmitter;
    bool isMiddleMousePressed;
    sf::Vector2f lastMousePos;
    sf::Vector2f mouseDragStart;

    float zoomLevel;
    sf::View roomView;
    sf::View uiView;
    sf::Vector2f viewCenter;
    bool ctrlPressed;

    sf::Font font;

    const float roomOffsetX = 200.0f;

public:
    MirrorRoomApp();
    ~MirrorRoomApp();

    void run();

private:
    void setupImGuiStyle();
    void createExampleRoom();
    void handleEvents();
    void handleMouseMove(const sf::Event::MouseMoveEvent& mouse);
    void handleMouseWheel(const sf::Event::MouseWheelScrollEvent& wheel);
    void updateEmitterHoverState();
    void handleMouseClick(const sf::Event::MouseButtonEvent& mouse);
    void handleMouseRelease(const sf::Event::MouseButtonEvent& mouse);
    void handleKeyPress(const sf::Event::KeyEvent& key);
    bool isPointNearWall(const Point& p, const WallConfig& wall, double threshold);
    double getWallParameter(const Point& p, const WallConfig& wall);
    void updateWallsFromPoints();
    void update();
    void updateTrace();
    void render();
    void drawRoom();
    void drawFlatWall(const WallConfig& wall, const sf::Color& color);
    void drawSphericalWall(const WallConfig& wall, const sf::Color& color);
    void drawTarget();
    void drawEmitter();
    void drawTrace();
    void drawUI();
    void setStatus(const std::string& message);
    void clearRoom();
    void resetToExample();
    bool saveConfig(const std::string& filename);
    bool loadConfig(const std::string& filename);
};