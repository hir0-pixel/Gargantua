# Gargantua

Real-time black hole raytracer implementing general relativity through Vulkan compute shaders. Named after the supermassive black hole from *Interstellar*.

---

## 🎯 Project Goal

Visualize spacetime curvature around black holes with scientific accuracy:
- Schwarzschild/Kerr metrics for curved spacetime geometry
- Geodesic ray tracing for light paths
- Gravitational lensing and redshift effects
- Novikov-Thorne accretion disk model
- Real-time performance (60 FPS target)

---

## ✨ Current Status: Phase 3 Complete

**Working:**
- ✅ Vulkan compute pipeline infrastructure
- ✅ GPU compute shader dispatch
- ✅ Offscreen storage image with swapchain presentation
- ✅ 60 FPS render loop at 1920×1080
- ✅ Clean validation layers

**Output:** Test gradient pattern confirming full GPU compute → display pipeline operational

---

## 🛠️ Technical Stack

- **Language:** C++20
- **Graphics API:** Vulkan 1.4
- **Shaders:** GLSL compute shaders (SPIR-V)
- **Build System:** CMake 3.20+
- **Libraries:** GLFW (windowing), GLM (math)
- **Platform:** Windows 11 (primary), macOS via MoltenVK (planned)
- **GPU:** NVIDIA RTX 3070 (development target)

---

## 📐 Physics Implementation Plan

### Schwarzschild Metric (Non-Rotating Black Hole)
```
ds² = -(1 - Rₛ/r)c²dt² + (1 - Rₛ/r)⁻¹dr² + r²(dθ² + sin²θ dφ²)
```

**Key Radii:**
- Event horizon: `r = Rₛ` (Schwarzschild radius)
- Photon sphere: `r = 1.5Rₛ` (unstable circular orbit)
- ISCO: `r = 3Rₛ` (innermost stable circular orbit for matter)

### Geodesic Integration
Photons follow null geodesics through spacetime:
```
d²xᵘ/dλ² + Γᵘₐᵦ (dxᵅ/dλ)(dxᵝ/dλ) = 0
```
- Integration method: RK4 (4th order Runge-Kutta)
- Ray state: 8D (position + 4-momentum in Schwarzschild coordinates)

---

## 🏗️ Architecture
```
Main Loop (CPU)
    ↓
Compute Shader (GPU)
    • Generate camera rays
    • Integrate geodesics
    • Sample accretion disk
    • Write to storage image
    ↓
Blit → Swapchain → Present
```

**Classes:**
- `VulkanContext`: Instance, device, queue management
- `Window`: GLFW window with Vulkan surface
- `Swapchain`: Image acquisition and presentation
- `ComputePipeline`: Shader dispatch and synchronization

---

## 🚀 Roadmap

- [x] **Phase 1:** Vulkan context and device initialization
- [x] **Phase 2:** Window and swapchain management
- [x] **Phase 3:** Compute pipeline infrastructure
- [ ] **Phase 4:** Schwarzschild geodesic ray tracing (next)
- [ ] **Phase 5:** Novikov-Thorne accretion disk model
- [ ] **Phase 6:** Kerr metric (rotating black holes)
- [ ] **Phase 7:** Interactive controls and UI

---

## 🔧 Building
```bash
# Prerequisites
# - Vulkan SDK 1.4+
# - CMake 3.20+
# - vcpkg (for GLFW)

# Clone
git clone https://github.com/hir0-pixel/Gargantua.git
cd Gargantua

# Configure
cmake -B build -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg.cmake

# Build
cmake --build build --config Release

# Run
./build/Release/Gargantua.exe
```

---

## 📚 References

**Scientific Papers:**
- *Gravitational lensing by spinning black holes in astrophysics, and in the movie Interstellar* - James et al. (2015)
- Novikov-Thorne accretion disk model

**Books:**
- *The Science of Interstellar* - Kip Thorne
- *Gravitation* - Misner, Thorne, Wheeler

**Inspiration:**
- DNEG's Double Negative Gravitational Renderer (DNGR) for *Interstellar*

---

## 📝 Development Notes

**Performance Target:** 60 FPS at 1920×1080 on RTX 3070
**Validation:** Enabled in debug builds for Vulkan API correctness
**Coordinate System:** Schwarzschild coordinates (t, r, θ, φ) for spacetime
**Numerical Stability:** Adaptive step sizing near event horizon (planned)