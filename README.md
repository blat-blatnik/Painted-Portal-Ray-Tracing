# Painted Portal real-time Ray Tracing

Playing around with real time raytracing and non-photorealistic rendering. For the contest subission of the Computer Graphics course University of Groningen 2020. There were two categories for the competition: OpenGL realtime rendering, and ray tracing. Since this is a _ray tracer_ running in real time _with_ OpenGL, this is a submission for both contest categories.

## Things to note

This program..

1. Does not run using Qt. It uses a couple of different libraries (see below) however even all of them combined have way fewer features than the massive Qt library, so we hope you will still judge this to be fair game for the competition.
2. Is a **massive** performance hog. You will definitely need a decent dedicated graphics card in order to run this smoothly.
3. Requires **at least** OpenGL 4.3 (2012 or newer hardware) since we use [shader storage buffers](https://www.khronos.org/opengl/wiki/Shader_Storage_Buffer_Object).
4. Relies on a good OpenGL **driver** to compile our shaders. Some open-source linux drivers that we tested couldn't do this..

With those uh.. minor quirks.. out of the way, I think our results speak for themselves.

## Reflections

![reflections](/screenshots/reflections.png)

We obviously get very nice reflections from the ray-tracing. The ray-tracer supports 3 basic shapes: planes, spheres, and voxels. Ray tracing is the first of our 2 render passes, and it is obviously _very_ expensive - especially since we don't have any spatial acceleration structures (yet), so every ray is tested against every object each frame. To mitigate this cost, we normally render to a small 256 x 256 texture. This is later upsampled to the whole screen in the second render pass. On (very) powerfull hardware this intermediary texture can be made larger, thats why the screenshots look so crisp.

## Shadows

![shadows](/screenshots/shadows.png)

Same as the reflections, the nice dynamic shadows are a result of the ray-tracing. We use basic [Phong lighting](https://en.wikipedia.org/wiki/Phong_reflection_model) to light the scene, nothing special. Take a look at the [shader code](/shaders/rayfrag.glsl).

## Portals

![portals](/screenshots/portal.png)

Portals are the star of this show. We introduced them as soon as we realized how easy it would be to move and distort rays as they hit objects. There are always 2 portals in the scene, an orange portal and a blue portal, when we detect a ray hitting a portal we simply teleport the ray to the other portal and we keep on tracing it - easy. And, ofcourse, portals are recursive.

![portal recursion](/screenshots/recursion.png)

## Impressionist painting style

![painting style](/screenshots/painted-portal.png)

Since the ray tracing output texture is very low-res, we thought we could use some paint to cover up the edges! After a trip to the Groningen Museum we got inspired by some of [Johan Dijkstra's paintings](https://klaasamulder.wordpress.com/2005/09/20/wat-leuk-weer-een-johan-dijkstra-gevondendagblad-vh-noorden/) and so we looked for some shaders on [Shadertoy](https://www.shadertoy.com/). We found [this shader](https://www.shadertoy.com/view/MtKcDG) made by Florian Berger. It looks great! The only problem is that its extremely slow - even slower than the ray tracing.. So we optimized it heavily and removed all but the necessary features to get a very similar effect. This painting shader is applied during the second rendering pass.

## Dynamic world

![dynamic world](/screenshots/scene3.png)

We made a simple datastructure that synchronizes data between the CPU and GPU. Using that, modifying the scene at runtime is trivial. Objects and lights can be added to scene using point and click editing at runtime - that's how we made the default scene.

## Collision detection

We re-used the ray tracing code from the shader in our gameplay code to implement player collisions with the environment. We then added gravity and got a relatively fun first person platforming game out of it. Ofcourse, we also implemented camera portal-travel, and you can shoot the portals anywhere!

## Hot-patch shader loading

We keep track of changes made to the shader files during runtime. When you change a shader, it will _immediately_ be recompiled and injected into the program _at runtime_, so you can instantly get feedback on what changed. This makes it very easy and fun to experiment with our shaders while running - try it out for yourself. For example, the painting shader is disabled by default from the [`paintfrag.glsl`](/shaders/paintfrag.glsl) file. If you comment out the `return` statement near the beggning of `main` the painting effect will be enabled.

## How to compile..

#### .. with Visual Studio 

download the Visual Studio project file in the [`/bin`](/bin) directory. Everything should already be set-up.

#### .. with GCC or clang

Make sure to install [GLFW](https://www.glfw.org/download.html) with your package manager. You will need to statically link against the appropriate library for your operating system.

```bash
$ g++ -O2 src/*.cpp -lm -lglfw
```

```bash
$ clang++ -O2 src/*.cpp -lm -lglfw
```

Version of GLFW for [Windows](/lib/glfw3.lib), [Linux](/lib/libglfw3.so), and [Mac](/lib/libglfw3.a) are provided in the [`/lib`](/lib) directory.

### Requirements

- C++03 compiler
- OpenGL 4.3 capable GPU
- Good GPU drivers
- Decently powerful computer
- GLFW library installed on your system (or use the one we provide)

### Running

Just run the executable you compiled and make sure the [`/shaders`](/shaders) and [`/textures`](/textures) directories are in the same directory as the executable. Also make sure not to rename or delete any of the files.

If you couldn't get the code to compile for whatever reason you can try running the pre-compiled exectables in the [`/bin`](/bin) directory. One is for 64-bit windows, and the other is for 64-bit linux.

### Libraries used
- [GLFW](https://www.glfw.org/) for opening a window and creating an OpenGL context.
- [GLAD](https://glad.dav1d.de/) for loading OpenGL functions.
- [GLM](https://glm.g-truc.net/0.9.9/index.html) for math.
- [STB Image](https://github.com/nothings/stb) for opening .png files.

### Controls

You can _move_ around with `WASD`, and _look_ around with the mouse. You can _jump_ with `spacebar` and also _double jump_ if you jump while in the air. The left and right mouse buttons will place the two portals to the surface you are looking at.

You can press `B` to go into _build-mode_. While in build mode you aren't affected by gravity, and you don't collide with the geometry. Instead you can press `spacebar` to _go up_, and `control` to _go down_. `Left-click` will _place a block_ instead of a portal. You can _choose the material_ of the block being placed with the `mouse-wheel` or numbers `0`..`9`. Pressing `P` will _take you out_ of build mode.
    
Have fun! :)
