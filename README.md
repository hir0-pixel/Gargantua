# Gargantua

Gargantua is a **real-time black hole raytracer** built using **Vulkan compute shaders**.
The goal of this project is to visualize the curvature of spacetime around black holes with both **scientific accuracy** and **real-time performance**.
The name comes from the black hole shown in *Interstellar*.

---

## 🎯 Project Goal

The main idea is to simulate how light bends and behaves near a black hole by implementing concepts from **general relativity** on the GPU.
This includes:

* The **Schwarzschild** and later **Kerr** metrics for spacetime geometry
* **Geodesic ray tracing** to calculate light paths
* **Gravitational lensing** and **redshift** effects
* A realistic **Novikov–Thorne accretion disk model** (in progress)

The long-term target is to reach around **60 FPS at 1080p** on modern GPUs.

---

## ⚙️ Current Status (Phase 4)

So far, the project has:

* A fully working **Vulkan compute pipeline**
* **Offscreen storage image** rendered through a swapchain
* A **stable 60 FPS** render loop
* Working **Schwarzschild ray tracing** with RK4 integration
* Basic **accretion disk and background rendering**

Next steps include adding the **Novikov–Thorne temperature gradient** and then extending the simulation to the **Kerr metric** for rotating black holes.

---

## 🧠 Physics Overview

The simulation currently uses the **Schwarzschild metric**:

```
ds² = -(1 - Rₛ/r)c²dt² + (1 - Rₛ/r)⁻¹dr² + r²(dθ² + sin²θ dφ²)
```

Key regions:

* Event horizon → r = Rₛ
* Photon sphere → r = 1.5 Rₛ
* ISCO (stable orbit) → r = 3 Rₛ

Photon paths are traced using the **Runge-Kutta 4th order** integrator to approximate null geodesics.

---

## 🏗️ Architecture

```
Main Loop (CPU)
   ↓
Compute Shader (GPU)
   → Generates camera rays
   → Integrates photon geodesics
   → Samples the disk and background
   ↓
Storage Image → Swapchain → Display
```

Main components:

* `VulkanContext` – device and queue setup
* `Window` – GLFW window + Vulkan surface
* `Swapchain` – image presentation
* `ComputePipeline` – shader dispatch system

---

## 🧰 Build Instructions

```bash
git clone https://github.com/hir0-pixel/Gargantua.git
cd Gargantua
cmake -B build
cmake --build build --config Release
./build/Release/Gargantua.exe
```

Requirements:

* Vulkan SDK 1.4+
* CMake 3.20+
* C++20 compiler
* Tested on Windows 11 + RTX 3070

---

## 📚 References

* James et al. (2015) – *Gravitational Lensing by Spinning Black Holes and Interstellar*
* Kip Thorne – *The Science of Interstellar*
* DNEG – DNGR Renderer (used in *Interstellar*)
* *Gravitation* – Misner, Thorne & Wheeler

---

### 🧭 Roadmap

* Phase 5 – Novikov-Thorne Disk (temperature gradients)
* Phase 6 – Kerr Metric (rotating black holes)
* Phase 7 – Interactive camera and UI