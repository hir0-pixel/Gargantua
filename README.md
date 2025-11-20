Here is a polished, corrected, GitHub-ready README that reflects **your exact project**, your **current progress**, your **actual physics accuracy**, and your **RTX 3070 performance**.
Tone is clean, professional, and precise. No em dashes.

---

# Gargantua

Gargantua is a **real time black hole raytracer** built using **Vulkan compute shaders**.
The goal is to visualize the curvature of spacetime around black holes with **physics based realism** and **interactive performance**.
The name comes from the black hole shown in *Interstellar*.

---

## 🎯 Project Goal

The aim is to simulate how light bends and propagates near a black hole using concepts from **general relativity**, executed entirely on the GPU.
This includes:

* The **Schwarzschild** metric for spacetime geometry and later the **Kerr** metric
* **Geodesic ray tracing** to compute photon paths
* **Gravitational lensing**, **redshift**, and **light deflection**
* A realistic **Novikov Thorne accretion disk model** (in progress)

Target performance is around **60 FPS at 1080p** on modern GPUs.

---

## ⚙️ Current Status (Phase 4)

So far, the project includes:

* A fully working **Vulkan compute pipeline**
* **Offscreen storage image** resolved to a swapchain
* A render loop that reaches **around 60 FPS at 1080p on an RTX 3070**
* Working **Schwarzschild ray tracing** with RK4 integration
* Basic **accretion disk and background sampling**
* Focused on the **Schwarzschild case**, with Kerr planned in later phases

Upcoming features include adding the **Novikov Thorne temperature gradient** and extending the simulation to the **Kerr metric** for rotating black holes.

---

## 🧠 Physics Overview

The renderer uses the **Schwarzschild metric**:

```
ds² = -(1 - Rₛ/r)c²dt² + (1 - Rₛ/r)⁻¹dr² + r²(dθ² + sin²θ dφ²)
```

Important regions:

* Event horizon at r = Rₛ
* Photon sphere at r = 1.5 Rₛ
* ISCO at r = 3 Rₛ

Photon paths are treated as **null geodesics** and integrated using **Runge Kutta 4th order** inside the compute shader. Accuracy is currently around **75 percent**, suitable for real time rendering.

---

## 🏗️ Architecture

```
CPU Main Loop
   ↓
Compute Shader (GPU)
   → Generates camera rays
   → Integrates photon geodesics
   → Samples disk and background
   ↓
Storage Image → Swapchain → Display
```

Main components:

* `VulkanContext` for device and queue setup
* `Window` for GLFW window and Vulkan surface
* `Swapchain` for presentation
* `ComputePipeline` for shader execution and dispatch

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

* Vulkan SDK 1.4 or newer
* CMake 3.20 or newer
* C++20 compatible compiler
* Tested on Windows 11 with an RTX 3070

Linux support planned.

---

## 📚 References

* James et al. 2015, Gravitational Lensing by Spinning Black Holes and Interstellar
* Kip Thorne, The Science of Interstellar
* DNEG, DNGR Renderer
* Misner, Thorne and Wheeler, Gravitation

---

## 🧭 Roadmap

* Phase 5: Novikov Thorne Disk with temperature gradients
* Phase 6: Kerr Metric for rotating black holes
* Phase 7: Interactive camera and UI