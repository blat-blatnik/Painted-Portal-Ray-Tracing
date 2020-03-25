# Painted-Portal-Ray-Tracing
Playing around with real time raytracing and non-photorealistic rendering. For the contest subission of the Computer Graphics course University of Groningen 2020.

What we have implemented:

- Phong lighting: self-explanatory nothing special.

- Raytracing: This runs on the GPU, and therefore you should have a dedicated GPU for it to be playable, otherwise you will just have a slideshow. It renders to a texture of 256x256 by default and is then upscaled. We found this to be the best compromise between performance and image quality. Even though 256x256 is very small, this is acceptable as any jagged egdes are masked by our painter shader. By default, the ray bounces two times

- A painting shader: This gives the render a painted look. Our shader is a modified and optimised version of this shader: (https://www.shadertoy.com/view/MtKcDG) However, the original shader was far too slow and GPU intensive. We achieve the look that we get by using a procedurally generated brush stroke and mapping it with a grid.

- Portals: We attempted to replicate the mechanics of portals as seen in the game "Portal 2". The player can shoot two portals wherever they like effectively connecting two points in space so that the player can pass through. Looking at a portal allows you to see what is on the other side. The portals also emit either blue or orange light, are coloured, and animated around the circumference.

- GPU syncing: By synchronising the GPU that enables us to be able to edit any part of the world from inside the game.

- Voxels
- Spheres
- Planes

- UV mapping
- Hand-made textures: All the textures used are original and hand made by us (some are Minecraft inspired).

- FPS camera: Allows the player to use the mouse and keyboard to look around freely.

- Scene: Two scenes are included in this project a simple one and a complex one. By default, the simple one is loaded in, for people to play around in. The complex scene contains about 1800 voxels which results in a massive slow-down in performance. We do not suggest loading in the complex scene unless you have a high-end enthusiast-grade graphics card and would like more than 3FPS.

- Scene building: Thanks to the power of GPU synchronising, we were able to introduce three modes of interacting with the game. These are described in the Controls section of the documentation.

- Collision system: This introduces collision between the player the objects in the world so that you can't un-immersively phase through objects.
- Movement system: Allows for running, jumping, and climbing

- Animated lights
- Crosshair

- Intersection code on CPU: You may notice that there is intersection code from the shaders on the CPU as well, this code is there to allow for determining object placement, removal, portal teleportation, the collision system, and many other things.



- Controls:

(P)Portal mode:
Portal mode is supposed to be similar to what a game would play like.
    Right-click: portal, on the surface you are aiming at
    Left-click: companion portal, on the surface you are aiming at
    Space: Jump, Double jump
    WASD: Movement
    
(B)Build mode (Flight enabled, collision disabled, portals disabled):
Build mode allows you to place voxels one at a time, wherever you want in order to build a scene/level or whatever you want really.
    Left-click: Add voxel where the crosshair is pointing
    Right-click: Remove the voxel you are aiming at
    Scroll: Change the material of voxel (Can be seen in the title bar)
    Middle-click: If you have a console open, this will return the function with the position of every voxel in the scene so that they may be pasted in the initialisation function.
    WASD: Movement
    
(E)Edit mode (Flight enabled, collision disabled, portals disabled):
Edit mode is similar in scope to Build mode; however, this mode is for generating a large number of voxels very quickly. Click on any two surfaces that are pointing in the same direction and the entirety of the space between them will be filled with voxels having the texture the was currently selected.
(Be very careful with this mode as adding many voxels to the scene will cause significant degradation to performance)
    Left/Right-click: sets the start/end positions between which voxels will be populated
    Middle-click: adds the voxels to the scene between the two set positions.
    Scroll: Change the material of voxel (Can be seen in the title bar)
    WASD: Movement
