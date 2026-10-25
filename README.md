# spinning-cube

A small software 3D renderer written in C that draws spinning, shaded shapes as ASCII characters in your terminal. No graphics libraries: just `math.h`, a character buffer, and ANSI escape codes.

```
        *
      *******
    *************
   *******************
  ***************,,,,,,,
   ************,,,,,,,,,
     *********,,,,,,,,
        ******,,,,,
```

*(Shape and shading vary frame to frame. This is just an illustration of the output style.)*

## Features

- Software rendering pipeline: rotation, perspective projection, triangle rasterization and a depth buffer
- Flat shading using surface normals and a brightness ramp of ASCII characters (`.,-~:;=!*#$@`)
- Mesh-based shapes: **cube**, **pyramid**, **sphere** and **torus**
- Flicker-free output using a single frame buffer and cursor-home escape codes
- Clean exit on Ctrl+C (the terminal cursor is restored)

## Requirements

- A C compiler (`gcc` or `clang`)
- A Linux, macOS or WSL terminal that supports ANSI escape codes
- A terminal window of at least **100 columns x 50 rows** (see [Configuration](#configuration))

## Build and run

```bash
git clone https://github.com/<your-username>/spinning-cube.git
cd spinning-cube
gcc -Wall -Wextra -o ascii main.c -lm
./ascii
```

Press **Ctrl+C** to quit.

> Avoid compiling with a strict `-std=c99`/`-std=c11` flag. `usleep` and `M_PI` are not exposed in strict mode. Plain `gcc` (or `-std=gnu11`) works.

### Choosing a shape

The shape is currently selected in `main()` by changing the argument to `build_mesh`:

```c
Mesh mesh = build_mesh(4);
```

| Value | Shape   |
|-------|---------|
| 1     | Cube    |
| 2     | Pyramid |
| 3     | Sphere  |
| 4     | Torus   |

Recompile and run after changing it.

## How it works

Every frame goes through the same pipeline:

1. **Mesh:** a shape is a list of vertices plus triangles that index into them.
2. **Rotate:** each vertex is rotated around the X, Y and Z axes by an angle that grows every frame.
3. **Project:** 3D points are mapped to 2D screen coordinates with perspective (`x * K / (z + camera_distance)`). Horizontal coordinates are stretched by 2x because terminal characters are roughly twice as tall as they are wide.
4. **Rasterize:** for each triangle, every character cell in its bounding box is tested using barycentric coordinates. Cells inside the triangle are candidates to be drawn.
5. **Depth test:** a z-buffer stores `1 / depth` per cell (the quantity that interpolates linearly across a perspective-projected triangle), so nearer surfaces overwrite farther ones.
6. **Shade:** the triangle's normal (from a cross product of its rotated edges) is dotted with a fixed light direction. The result, plus a small ambient term, picks a character from the brightness ramp.
7. **Present:** the whole frame is printed in one go, then the cursor jumps back to the top-left for the next frame.

### Triangle orientation

Normals only make sense if every triangle's corners are ordered consistently. After a mesh is built, `orient_outward()` flips any triangle whose normal points toward the inside of the shape. Convex shapes use the direction from the mesh centre to the triangle; the torus supplies its own "outward" function because it is not convex.

## Project structure

Everything lives in `main.c`, in this order:

| Section | Purpose |
|---------|---------|
| Types and buffers | `Vec3`, `ScreenPt`, `Tri`, `Mesh`, the character and depth buffers |
| 3D math | rotation functions, vector helpers (`cross`, `dot`, `normalize`), `project` |
| Shading | brightness ramp, light direction, `shade_char` |
| Meshes | allocation, outward orientation, builders for each shape |
| Rasterizer | `edge_fn` and `fill_triangle` (barycentric fill with z-buffer) |
| `main` | frame loop |

## Configuration

Constants at the top of `main.c`:

| Constant | Default | Meaning |
|----------|---------|---------|
| `WIDTH`, `HEIGHT` | 100, 50 | Size of the character buffer. Your terminal must be at least this large or the frame will wrap and flicker. |
| `CAMERA_DIST` | 5.0 | How far the shape sits from the camera |
| `K` | 40.0 | Overall scale of the projected image |
| `ASPECT` | 2.0 | Horizontal stretch to compensate for tall terminal cells (try 1.8 to 2.2 for your font) |
| `AMBIENT` | 0.1 | Minimum brightness so unlit faces stay visible |

The light direction is `LIGHT_RAW`, and the shading characters are in `RAMP`.

## Development log

The project is built in small milestones, one commit each:

1. Frame loop with a screen buffer and a moving dot
2. 3D rotation and perspective projection of the cube's vertices
3. Wireframe cube with Bresenham line drawing
4. Solid faces with a barycentric triangle rasterizer and z-buffer
5. Flat shading with surface normals and a brightness ramp
6. Mesh abstraction, with cube, pyramid, sphere and torus builders

## Known limitations

- Shading is flat (one character per triangle), so the sphere and torus look slightly faceted.
- Shape selection requires editing the source and recompiling.
- The canvas size is fixed at compile time.

## License

Add a license of your choice here (for example MIT).