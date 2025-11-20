# C Demo Engine: Audio-Visual Synthesis

This project is a minimal "demo scene" implementation written in C, combining low-level graphics programming (OpenGL) with procedural FM audio synthesis (Core Audio). It was developed as a final demonstration piece for a **Software Systems** course.

## Overview

The core goal of this engine is to manage and synchronize two distinct hardware subsystems, the **GPU** and the **Sound Card**, using systems programming concepts:

1.  **3D Graphics:** A minimal OpenGL 3.3 pipeline for rendering rotating, texture-mapped cubes.
2.  **Procedural Audio:** A low-level FM synthesizer engine that generates a funky, percussive bassline on a separate, high-priority thread.

## Technology Stack

| Component | Library / API | Role in the System |
| :--- | :--- | :--- |
| **Windowing & Context** | GLFW | Handles window creation and input events. |
| **Graphics API** | GLAD (OpenGL Loader) | Manages core 3.3 OpenGL function pointers. |
| **Linear Algebra** | CGLM | Provides optimized vector and matrix math for 3D projection/model transformations. |
| **Texture Loading** | stb\_image | Simple, single-file image loading for the texture. |
| **Audio** | AudioToolbox / CoreAudio (macOS) | Manages the asynchronous ring buffer for low-latency audio playback. |
| **Concurrency** | OS Threads (Implicit) | The audio queue runs concurrently to the main rendering loop. |

## Key Features and SoftSys Competencies

This project demonstrates proficiency in several Software Systems competencies, including:

* **Concurrency (`comm.conc`):** The program manages a shared global state (`AudioState`) accessed by the main rendering thread and the high-priority, OS-managed audio callback thread.
* **External Libraries (`sw.lib`):** Successful integration and use of multiple complex external libraries (GLFW, CGLM, GLAD, CoreAudio).
* **Performance (`perf.bneck`):** Mitigation of potential bottlenecks through GPU data uploading (VBOs) and a triple-buffering system in the audio engine to prevent buffer starvation (audio crackling).

## Build and Run

*Note: This project relies on the **CoreAudio** framework and is currently configured for a **macOS** environment.*

1.  **Dependencies:** Ensure you have the required graphics, math, and audio libraries installed.
2.  **Compilation:** Compile the `main.c` file, linking against the necessary libraries (`-lglfw`, `-framework OpenGL`, `-framework AudioToolbox`).
3.  **Execution:** `./demo_engine`

## How it Works

The core audio engine uses a combination of **FM Synthesis** and a percussive **Envelope Generator**.

1.  **Carrier Wave:** Sine wave set to the note frequency.
2.  **Modulator Wave:** A separate sine wave running at 2x the carrier frequency.
3.  **Synthesis:** The phase of the carrier is distorted by the modulator, creating the signature "growl" or harmonic-rich texture: `sin(phase + (sin(mod_phase) * amount))`
4.  **Rhythm:** The main loop sends `audio_slap` events at a synchronized, high tempo (8 ticks/sec), triggering a fast Attack / Exponential Decay envelope to simulate a slap bass rhythm.
