# NinjaFruit 🍉🥷

A gesture-based 3D fruit-slicing game where you use your webcam and hand movements to slice projectiles flying through the air — no mouse or keyboard required.

## Overview

NinjaFruit is inspired by the classic Fruit Ninja game. Instead of touch controls, it uses real-time hand detection through your webcam to track your hand movements and translate them into a virtual sword in a 3D arena. Slice as many projectiles as you can before the timer runs out!

## Features

- **Gesture-based controls** — webcam detects your hand in real time using SIFT/FLANN feature matching and HSV skin segmentation
- **3D OpenGL rendering** — fully rendered arena with walls, floor grid, lighting, and decorative arch portals
- **4 projectile types** — Cone, Cylinder, Cube, and Pyramid, each with distinct colors and textures
- **Physics simulation** — gravity, realistic fall trajectories, and split effects when a projectile is sliced
- **60-second game rounds** with a 5-life system
- **Calibration system** — one-time hand calibration for improved detection accuracy across different lighting conditions
- **Audio feedback** — sword slice and UI click sound effects
- **Live camera preview** on the welcome screen

## Requirements

- **Qt 6** (with OpenGL and Multimedia support)
- **OpenCV** (with SIFT/FLANN feature matching enabled — `opencv_contrib` recommended)
- **C++17** compatible compiler (GCC, Clang, or MSVC)
- **Webcam** connected to your computer

## Building

1. Open the project in **Qt Creator** and configure it with a Qt 6 kit that includes OpenCV.
2. Alternatively, build from the command line using `qmake`:

   ```bash
   qmake NinjaFruit.pro
   make
   ```

3. Run the compiled executable:

   ```bash
   ./NinjaFruit
   ```

## How to Play

1. **Launch the application.** The welcome screen shows a live camera preview.
2. **Calibrate your hand** (recommended on first run):
   - Click **Calibrate**.
   - Hold your hand inside the highlighted square and follow the on-screen instructions to capture reference images.
   - Click **Done** when finished.
3. **Start the game** by clicking **Start Game**.
4. **Slice projectiles** by moving your hand in front of the camera. Your hand is rendered as a virtual sword in the 3D arena.
5. Each successful slice earns **10 points**. Missing a projectile costs **1 life**.
6. The game ends when the **60-second timer** expires or you run out of lives.

## Controls

| Action | How |
|--------|-----|
| Slice a projectile | Move your hand through a flying object |
| Navigate menus | Click buttons with mouse |

## Project Structure

```
NinjaFruit/
├── main.cpp                  # Application entry point
├── mainwindow.h/.cpp         # Main UI controller and camera frame processing
├── gameengine.h/.cpp         # Game logic: scoring, lives, timer, projectile spawning
├── gamewidget.h/.cpp         # OpenGL rendering widget
├── handdetector.h/.cpp       # Hand detection (SIFT/FLANN, HSV segmentation, ORB fallback)
├── projectile.h/.cpp         # Projectile data model and physics state
├── projectile_renderer.h/.cpp# 3D shape rendering for projectiles
├── scene_renderer.h/.cpp     # Arena environment rendering
├── hand_renderer.h/.cpp      # Virtual hand/sword rendering
├── calibrationwidget.h/.cpp  # Hand calibration UI
├── physicsutils.h/.cpp       # Physics helpers (gravity, collision detection)
├── resources.qrc             # Qt resource file (textures, sounds, images)
├── textures/                 # Projectile and environment textures
├── sounds/                   # Audio assets (sword.mp3, click.mp3)
└── image/                    # App icon and background image
```

## Gameplay Details

| Parameter | Value |
|-----------|-------|
| Game duration | 60 seconds |
| Starting lives | 5 |
| Points per slice | 10 |
| Projectile spawn rate | Every 2 seconds |
| Camera resolution | 640 × 480 @ 30 FPS |

## License

See [LICENSE](LICENSE) for details.
