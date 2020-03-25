# Painted Portal real-time Ray Tracing

Playing around with real time raytracing and non-photorealistic rendering. For the contest subission of the Computer Graphics course University of Groningen 2020. Since this is a _ray tracer_ running in real time _with_ OpenGL, this is a submission for both of the contests.

## Things to note

This program..

1. Does not run using Qt. It uses a couple of different libraries (see below) however even all of them combined have way fewer features than the massive Qt library, so we hope you will still judge this to be fair game for the competition.
2. Is a **massive** performance hog. You will definitely need a decent dedicated graphics card in order to run this smoothly.
3. Requires **at least** OpenGL 4.3 since we use [shader storage buffers](https://www.khronos.org/opengl/wiki/Shader_Storage_Buffer_Object).
4. Relies on a good GLSL compiler so **you need a good OpenGL driver** that can optimize our shaders and perform register allocation.

With those uh.. minor quirks.. out of the way, I think our results speak for themselves.

## Reflections

![reflections](/screenshots/reflections.png)

## Shadows

![shadows](/screenshots/shadows.png)

## Portals

![portals](/screenshots/portal.png)

And, ofcourse, portals are recursive.

![portal recursion](/screenshots/recursion.png)

## Impressionist painting style

![painting style](painted-portal.png)

## Dynamic world

![dynamic world](/screenshots/scene3.png)

## Collision detection

## Hot-patch shader loading

## How to compile

### Requirements

- C++03 compiler
- OpenGL 4.3 capable GPU
- Good GPU drivers
- GLFW library installed on your system (or use the one we provide)

### Libraries used
- [GLFW](https://www.glfw.org/) for opening windows and OpenGL context creation.
- [GLAD](https://glad.dav1d.de/) for loading OpenGL functions.
- [GLM](https://glm.g-truc.net/0.9.9/index.html) for math.
- [STB Image](https://github.com/nothings/stb) for opening .png files.

### Controls
- (P)lay mode
    - Right-click: portal, on the surface you are aiming at
    - Left-click: companion portal, on the surface you are aiming at
    - Space: Jump, Double jump
    - WASD: Movement
    
- (B)uild mode (flight enabled, collision disabled, portals disabled):
Build mode allows you to place voxels one at a time, wherever you want in order to build a scene/level or whatever you want really.
    - Left-click: Add voxel where the crosshair is pointing
    - Right-click: Remove the voxel you are aiming at
    - Scroll: Change the material of voxel (Can be seen in the title bar)
    - Middle-click: If you have a console open, this will return the function with the position of every voxel in the scene so that they may be pasted in the initialisation function.
    - WASD: Movement