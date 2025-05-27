# Ninja Fruit - Hand Gesture Recognition Game

A real-time hand gesture recognition game built with Qt and OpenCV, featuring advanced computer vision techniques for precise hand tracking and collision detection.

## Table of Contents
- [Overview](#overview)
- [FLANN Matching Technology](#flann-matching-technology)
- [Features](#features)
- [Installation](#installation)
- [How to Play](#how-to-play)
- [Technical Architecture](#technical-architecture)
- [Build Requirements](#build-requirements)

## Overview

Ninja Fruit is an interactive 3D game that uses computer vision to track hand movements in real-time. Players use their hands as virtual swords to slice projectiles that appear on screen. The game employs sophisticated computer vision algorithms, particularly FLANN (Fast Library for Approximate Nearest Neighbors) matching for robust hand detection and tracking.

## FLANN Matching Technology

### What is FLANN?

FLANN (Fast Library for Approximate Nearest Neighbors) is a library for performing fast approximate nearest neighbor searches in high dimensional spaces. In our hand detection system, FLANN is used for feature matching between detected hand landmarks and reference patterns.

### How FLANN Works in Hand Detection

#### 1. Feature Extraction
```cpp
// HandDetector extracts key features from hand landmarks
std::vector<cv::KeyPoint> extractHandFeatures(const cv::Mat& handRegion) {
    cv::Ptr<cv::ORB> detector = cv::ORB::create();
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    detector->detectAndCompute(handRegion, cv::noArray(), keypoints, descriptors);
    return keypoints;
}
```

#### 2. FLANN Index Building
The system builds a FLANN index from reference hand patterns:
```cpp
// Build FLANN matcher for fast feature matching
cv::FlannBasedMatcher matcher(
    new cv::flann::KDTreeIndexParams(5),  // 5 trees for better accuracy
    new cv::flann::SearchParams(50)       // Maximum leaf nodes to visit
);
```

#### 3. Fast Matching Process
FLANN performs approximate nearest neighbor searches to match detected features:
```cpp
// Match features using FLANN
std::vector<std::vector<cv::DMatch>> knnMatches;
matcher.knnMatch(queryDescriptors, trainDescriptors, knnMatches, 2);

// Apply Lowe's ratio test for robust matching
std::vector<cv::DMatch> goodMatches;
for (size_t i = 0; i < knnMatches.size(); i++) {
    if (knnMatches[i][0].distance < 0.7f * knnMatches[i][1].distance) {
        goodMatches.push_back(knnMatches[i][0]);
    }
}
```

### Why FLANN for Hand Tracking?

1. **Speed**: FLANN provides O(log n) search complexity compared to O(n) for brute force
2. **Accuracy**: Approximate matching is sufficient for real-time hand tracking
3. **Robustness**: Handles partial occlusions and varying hand orientations
4. **Memory Efficiency**: Compact data structures for large feature sets

### FLANN Configuration in Our System

#### Index Parameters
- **Algorithm**: KD-Tree (optimal for our feature dimensions)
- **Trees**: 5 trees for balanced speed/accuracy trade-off
- **Branching**: 32 (default for KD-trees)

#### Search Parameters
- **Checks**: 50 (maximum leaf nodes to examine)
- **EPS**: 0.0 (exact search within approximation bounds)
- **Sorted**: True (return matches sorted by distance)

```cpp
// Optimized FLANN parameters for hand tracking
cv::flann::KDTreeIndexParams indexParams(5);
cv::flann::SearchParams searchParams(50, 0.0, true);
cv::FlannBasedMatcher flannMatcher(indexParams, searchParams);
```

### Hand Detection Pipeline

1. **Frame Capture**: Capture video frame from camera
2. **Preprocessing**: Apply Gaussian blur and color space conversion
3. **Feature Detection**: Extract ORB features from hand regions
4. **FLANN Matching**: Match features against reference hand patterns
5. **Pose Estimation**: Calculate hand position and orientation
6. **Kalman Filtering**: Smooth tracking results for stable gameplay

## Features

- **Real-time Hand Tracking**: 30+ FPS hand detection using optimized FLANN matching
- **3D Game Environment**: OpenGL-rendered 3D scene with dynamic lighting
- **Collision Detection**: Precise sword-projectile collision using geometric algorithms
- **Audio System**: Immersive sound effects using Qt Multimedia
- **Calibration System**: User-specific hand tracking calibration
- **Score System**: Points and lives tracking with game over conditions

## Installation

### Prerequisites

- Qt 6.x with OpenGL and Multimedia modules
- OpenCV 4.x with contrib modules
- C++17 compatible compiler
- OpenGL 3.3+ support

### Build Steps

1. **Clone the repository**:
```bash
git clone <repository-url>
cd NinjaFruit
```

2. **Install dependencies**:
```bash
# Windows (vcpkg)
vcpkg install opencv4[contrib]:x64-windows qt6:x64-windows

# Ubuntu/Debian
sudo apt-get install libopencv-dev qt6-base-dev qt6-multimedia-dev

# macOS (Homebrew)
brew install opencv qt6
```

3. **Configure OpenCV path** in `NinjaFruit.pro`:
```qmake
OPENCV_DIR = C:/opencv/build/install  # Adjust path as needed
```

4. **Build the project**:
```bash
qmake NinjaFruit.pro
make  # or nmake on Windows
```

## How to Play

1. **Launch**: Run the executable and ensure your camera is connected
2. **Calibrate**: Click "Calibrate" and follow the on-screen instructions
3. **Start Game**: Click "Start Game" to begin
4. **Play**: Move your hand to control the virtual sword and slice projectiles
5. **Scoring**: Earn points by slicing projectiles, lose lives by missing them

### Controls
- **Hand Movement**: Move your hand in front of the camera to control the sword
- **Slicing**: Fast hand movements trigger slice detection
- **Menu Navigation**: Use mouse to navigate menus

## Technical Architecture

### Core Components

#### HandDetector Class
- **FLANN-based matching** for feature correspondence
- **Kalman filtering** for smooth tracking
- **Calibration system** for user adaptation

#### GameEngine Class
- **Physics simulation** for projectile trajectories
- **Collision detection** between sword and projectiles
- **Game state management** (score, lives, timer)

#### GameWidget Class
- **OpenGL rendering** of 3D scene
- **Shader programs** for lighting and textures
- **Real-time animation** system

### Performance Optimizations

1. **FLANN Indexing**: Pre-built indices for fast feature matching
2. **Object Pooling**: Reuse projectile objects to reduce allocation overhead
3. **Frustum Culling**: Only render visible objects
4. **Texture Atlasing**: Reduce texture binding calls
5. **Threaded Processing**: Separate threads for CV processing and rendering

### Computer Vision Pipeline

```
Camera Frame → Preprocessing → Feature Detection → FLANN Matching → 
Pose Estimation → Kalman Filtering → Game Logic → Rendering
```

## Build Requirements

### Minimum System Requirements
- **OS**: Windows 10+, Ubuntu 18.04+, macOS 10.15+
- **RAM**: 4GB minimum, 8GB recommended
- **GPU**: OpenGL 3.3+ compatible graphics card
- **Camera**: USB webcam with 640x480+ resolution

### Development Tools
- **Qt Creator** (recommended IDE)
- **CMake 3.16+** or qmake
- **Git** for version control

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Implement your changes with proper testing
4. Submit a pull request with detailed description

## Troubleshooting

### Common Issues

1. **Camera not detected**: Check camera permissions and USB connection
2. **Poor hand tracking**: Ensure good lighting and clear background
3. **Low FPS**: Reduce camera resolution or adjust FLANN parameters
4. **Build errors**: Verify OpenCV and Qt installation paths

### Performance Tuning

Adjust FLANN parameters in `handdetector.cpp`:
```cpp
// For better accuracy (slower)
cv::flann::KDTreeIndexParams indexParams(8);
cv::flann::SearchParams searchParams(100);

// For better speed (less accurate)
cv::flann::KDTreeIndexParams indexParams(3);
cv::flann::SearchParams searchParams(25);
```

## Acknowledgments

- **OpenCV Team** for computer vision libraries
- **Qt Project** for the application framework
- **FLANN Authors** for the fast nearest neighbor search library