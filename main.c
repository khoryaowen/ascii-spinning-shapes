#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#define WIDTH  100
#define HEIGHT 50
#define CAMERA_DIST 5.0
#define K 40.0
#define ASPECT 2.0

typedef struct { double x, y, z; } Vec3;

static char screen[HEIGHT][WIDTH];
static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void clear_buffer(void) {
    memset(screen, ' ', sizeof screen);
}

static void present(void) {
    printf("\x1b[H");
    for (int y = 0; y < HEIGHT; y++) {
        fwrite(screen[y], 1, WIDTH, stdout);
        putchar('\n');
    }
    fflush(stdout);
}

static void put_char(int x, int y, char c) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        screen[y][x] = c;
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

/* Returns 1 and fills sx, sy if the point is in front of the camera. */
static int project(Vec3 p, int *sx, int *sy) {
    double depth = p.z + CAMERA_DIST;
    if (depth <= 0.1) return 0;
    double scale = K / depth;
    *sx = (int)lround(WIDTH  / 2.0 + ASPECT * p.x * scale);
    *sy = (int)lround(HEIGHT / 2.0 - p.y * scale);
    return 1;
}


/* ---- line drawing (Bresenham) ---- */
static char line_char(int x0, int y0, int x1, int y1) {
    double dx = fabs((double)(x1 - x0));
    double dy = fabs((double)(y1 - y0)) * ASPECT;   /* visual height */

    if (dx == 0 && dy == 0) return '+';
    if (dy < dx * 0.4)      return '-';
    if (dy > dx * 2.4)      return '|';
    return ((x1 - x0) * (y1 - y0) > 0) ? '\\' : '/';
}

static void draw_line(int x0, int y0, int x1, int y1) {
    char c = line_char(x0, y0, x1, y1);

    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        put_char(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---- cube data ---- */

static const Vec3 cube_vertices[8] = {
    {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
};

static const int cube_edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},     /* back face  */
    {4, 5}, {5, 6}, {6, 7}, {7, 4},     /* front face */
    {0, 4}, {1, 5}, {2, 6}, {3, 7},     /* connecting */
};

int main(void) {
    signal(SIGINT, handle_sigint);
    printf("\x1b[2J\x1b[?25l");

    double ax = 0, ay = 0, az = 0;

    while (running) {
        clear_buffer();

        int px[8], py[8], visible[8];

        for (int i = 0; i < 8; i++) {
            Vec3 p = cube_vertices[i];
            p = rotate_x(p, ax);
            p = rotate_y(p, ay);
            p = rotate_z(p, az);
            visible[i] = project(p, &px[i], &py[i]);
        }

        for (int e = 0; e < 12; e++) {
            int a = cube_edges[e][0];
            int b = cube_edges[e][1];
            if (visible[a] && visible[b])
                draw_line(px[a], py[a], px[b], py[b]);
        }

        for (int i = 0; i < 8; i++)
            if (visible[i])
                put_char(px[i], py[i], '@');

        present();
        ax += 0.04;
        ay += 0.03;
        az += 0.02;
        usleep(33000);
    }

    printf("\x1b[?25h\n");
    return 0;
}