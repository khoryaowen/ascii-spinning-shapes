#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#define WIDTH  100
#define HEIGHT 50

#define CAMERA_DIST 5.0
#define K           40.0
#define ASPECT      2.0

typedef struct { double x, y, z; } Vec3;
typedef struct { double x, y, inv_z; int visible; } ScreenPt;
typedef struct { int v[3]; char ch; } Tri;

static char   screen[HEIGHT][WIDTH];
static double zbuf[HEIGHT][WIDTH];
static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void clear_buffers(void) {
    memset(screen, ' ', sizeof screen);
    memset(zbuf, 0, sizeof zbuf);          /* all-zero bytes == 0.0 */
}

static void present(void) {
    printf("\x1b[H");
    for (int y = 0; y < HEIGHT; y++) {
        fwrite(screen[y], 1, WIDTH, stdout);
        putchar('\n');
    }
    fflush(stdout);
}

/* ---- 3D math ---- */

static Vec3 rotate_x(Vec3 p, double a) {
    Vec3 r = { p.x,
               p.y * cos(a) - p.z * sin(a),
               p.y * sin(a) + p.z * cos(a) };
    return r;
}

static Vec3 rotate_y(Vec3 p, double a) {
    Vec3 r = {  p.x * cos(a) + p.z * sin(a),
                p.y,
               -p.x * sin(a) + p.z * cos(a) };
    return r;
}

static Vec3 rotate_z(Vec3 p, double a) {
    Vec3 r = { p.x * cos(a) - p.y * sin(a),
               p.x * sin(a) + p.y * cos(a),
               p.z };
    return r;
}

static void project(Vec3 p, ScreenPt *out) {
    double depth = p.z + CAMERA_DIST;
    out->visible = depth > 0.1;
    if (!out->visible) return;
    double scale = K / depth;
    out->x     = WIDTH  / 2.0 + ASPECT * p.x * scale;
    out->y     = HEIGHT / 2.0 - p.y * scale;
    out->inv_z = 1.0 / depth;
}

/* ---- triangle rasterizer ---- */

static double edge_fn(const ScreenPt *a, const ScreenPt *b, double px, double py) {
    return (px - a->x) * (b->y - a->y) - (py - a->y) * (b->x - a->x);
}

static void fill_triangle(const ScreenPt *a, const ScreenPt *b,
                          const ScreenPt *c, char ch) {
    double area = edge_fn(a, b, c->x, c->y);
    if (fabs(area) < 1e-9) return;         /* degenerate (edge-on) triangle */

    int min_x = (int)floor(fmin(a->x, fmin(b->x, c->x)));
    int max_x = (int)ceil (fmax(a->x, fmax(b->x, c->x)));
    int min_y = (int)floor(fmin(a->y, fmin(b->y, c->y)));
    int max_y = (int)ceil (fmax(a->y, fmax(b->y, c->y)));

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > WIDTH  - 1) max_x = WIDTH  - 1;
    if (max_y > HEIGHT - 1) max_y = HEIGHT - 1;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            double px = x + 0.5, py = y + 0.5;

            double w0 = edge_fn(b, c, px, py) / area;
            double w1 = edge_fn(c, a, px, py) / area;
            double w2 = edge_fn(a, b, px, py) / area;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;   /* outside */

            double inv_z = w0 * a->inv_z + w1 * b->inv_z + w2 * c->inv_z;
            if (inv_z > zbuf[y][x]) {                   /* closer than before */
                zbuf[y][x]   = inv_z;
                screen[y][x] = ch;
            }
        }
    }
}

/* ---- cube data ---- */

static const Vec3 cube_vertices[8] = {
    {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
};

static const Tri cube_tris[12] = {
    {{0, 1, 2}, '.'}, {{0, 2, 3}, '.'},     /* back   (z = -1) */
    {{4, 5, 6}, '#'}, {{4, 6, 7}, '#'},     /* front  (z = +1) */
    {{0, 3, 7}, '+'}, {{0, 7, 4}, '+'},     /* left   (x = -1) */
    {{1, 2, 6}, '='}, {{1, 6, 5}, '='},     /* right  (x = +1) */
    {{0, 1, 5}, '%'}, {{0, 5, 4}, '%'},     /* bottom (y = -1) */
    {{3, 2, 6}, '@'}, {{3, 6, 7}, '@'},     /* top    (y = +1) */
};

int main(void) {
    signal(SIGINT, handle_sigint);
    printf("\x1b[2J\x1b[?25l");

    double ax = 0, ay = 0, az = 0;

    while (running) {
        clear_buffers();

        ScreenPt pts[8];
        for (int i = 0; i < 8; i++) {
            Vec3 p = cube_vertices[i];
            p = rotate_x(p, ax);
            p = rotate_y(p, ay);
            p = rotate_z(p, az);
            project(p, &pts[i]);
        }

        for (int t = 0; t < 12; t++) {
            const ScreenPt *a = &pts[cube_tris[t].v[0]];
            const ScreenPt *b = &pts[cube_tris[t].v[1]];
            const ScreenPt *c = &pts[cube_tris[t].v[2]];
            if (a->visible && b->visible && c->visible)
                fill_triangle(a, b, c, cube_tris[t].ch);
        }

        present();
        ax += 0.04;
        ay += 0.03;
        az += 0.02;
        usleep(33000);
    }

    printf("\x1b[?25h\n");
    return 0;
}