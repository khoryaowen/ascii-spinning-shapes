#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
typedef struct { int v[3]; } Tri;
typedef struct { Vec3 *verts; int nverts; Tri *tris; int ntris; } Mesh;
typedef Vec3 (*OutwardFn)(Vec3 p);

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

static Vec3 vsub(Vec3 a, Vec3 b) {
    Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static Vec3 cross(Vec3 a, Vec3 b) {
    Vec3 r = { a.y * b.z - a.z * b.y,
               a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x };
    return r;
}

static double dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 normalize(Vec3 v) {
    double len = sqrt(dot(v, v));
    if (len < 1e-12) return v;
    Vec3 r = { v.x / len, v.y / len, v.z / len };
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

/* ---- shading ---- */

static const char RAMP[] = ".,-~:;=!*#$@";
#define RAMP_LEN ((int)sizeof(RAMP) - 1)
#define AMBIENT  0.1

static const Vec3 LIGHT_RAW = { -0.4, 0.7, -0.6 };

static char shade_char(double brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 1) brightness = 1;
    int idx = (int)(brightness * (RAMP_LEN - 1) + 0.5);
    return RAMP[idx];
}

/* ---- meshes ---- */

static Mesh mesh_alloc(int nverts, int ntris) {
    Mesh m;
    m.nverts = nverts;
    m.ntris  = ntris;
    m.verts  = malloc(nverts * sizeof(Vec3));
    m.tris   = malloc(ntris  * sizeof(Tri));
    if (!m.verts || !m.tris) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return m;
}

static void mesh_free(Mesh *m) {
    free(m->verts);
    free(m->tris);
}

/* Flip any triangle whose normal points toward the inside. */
static void orient_outward(Mesh *m, OutwardFn outward) {
    Vec3 center = {0, 0, 0};
    for (int i = 0; i < m->nverts; i++) {
        center.x += m->verts[i].x;
        center.y += m->verts[i].y;
        center.z += m->verts[i].z;
    }
    center.x /= m->nverts;
    center.y /= m->nverts;
    center.z /= m->nverts;

    for (int t = 0; t < m->ntris; t++) {
        Vec3 a = m->verts[m->tris[t].v[0]];
        Vec3 b = m->verts[m->tris[t].v[1]];
        Vec3 c = m->verts[m->tris[t].v[2]];

        Vec3 n = cross(vsub(b, a), vsub(c, a));
        Vec3 cen = { (a.x + b.x + c.x) / 3.0,
                     (a.y + b.y + c.y) / 3.0,
                     (a.z + b.z + c.z) / 3.0 };
        Vec3 out = outward ? outward(cen) : vsub(cen, center);

        if (dot(n, out) < 0) {
            int tmp = m->tris[t].v[1];
            m->tris[t].v[1] = m->tris[t].v[2];
            m->tris[t].v[2] = tmp;
        }
    }
}

static Mesh mesh_cube(void) {
    static const Vec3 v[8] = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
    };
    static const int t[12][3] = {
        {0, 1, 2}, {0, 2, 3},   {4, 5, 6}, {4, 6, 7},
        {0, 3, 7}, {0, 7, 4},   {1, 2, 6}, {1, 6, 5},
        {0, 1, 5}, {0, 5, 4},   {3, 2, 6}, {3, 6, 7},
    };

    Mesh m = mesh_alloc(8, 12);
    memcpy(m.verts, v, sizeof v);
    for (int i = 0; i < 12; i++)
        m.tris[i] = (Tri){{ t[i][0], t[i][1], t[i][2] }};

    orient_outward(&m, NULL);
    return m;
}

static Mesh mesh_pyramid(void) {
    static const Vec3 v[5] = {
        {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, {0, 1.2, 0},
    };
    static const int t[6][3] = {
        {0, 1, 2}, {0, 2, 3},                          /* base  */
        {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4},    /* sides */
    };

    Mesh m = mesh_alloc(5, 6);
    memcpy(m.verts, v, sizeof v);
    for (int i = 0; i < 6; i++)
        m.tris[i] = (Tri){{ t[i][0], t[i][1], t[i][2] }};

    orient_outward(&m, NULL);
    return m;
}

#define SPHERE_RADIUS 1.5

static Mesh mesh_sphere(int stacks, int slices) {
    Mesh m = mesh_alloc((stacks + 1) * slices, 2 * stacks * slices);

    for (int i = 0; i <= stacks; i++) {
        double phi = M_PI * i / stacks;
        for (int j = 0; j < slices; j++) {
            double theta = 2.0 * M_PI * j / slices;
            m.verts[i * slices + j] = (Vec3){
                SPHERE_RADIUS * sin(phi) * cos(theta),
                SPHERE_RADIUS * cos(phi),
                SPHERE_RADIUS * sin(phi) * sin(theta) };
        }
    }

    int k = 0;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int j2 = (j + 1) % slices;
            int a = i * slices + j,       b = i * slices + j2;
            int c = (i + 1) * slices + j, d = (i + 1) * slices + j2;
            m.tris[k++] = (Tri){{ a, c, b }};
            m.tris[k++] = (Tri){{ b, c, d }};
        }
    }

    orient_outward(&m, NULL);
    return m;
}

#define TORUS_MAJOR 1.2
#define TORUS_MINOR 0.5

static Vec3 torus_outward(Vec3 p) {
    double len = sqrt(p.x * p.x + p.z * p.z);
    if (len < 1e-9) len = 1e-9;
    Vec3 ring = { TORUS_MAJOR * p.x / len, 0, TORUS_MAJOR * p.z / len };
    return vsub(p, ring);
}

static Mesh mesh_torus(int rings, int sides) {
    Mesh m = mesh_alloc(rings * sides, 2 * rings * sides);

    for (int i = 0; i < rings; i++) {
        double theta = 2.0 * M_PI * i / rings;
        for (int j = 0; j < sides; j++) {
            double phi = 2.0 * M_PI * j / sides;
            double r = TORUS_MAJOR + TORUS_MINOR * cos(phi);
            m.verts[i * sides + j] = (Vec3){
                r * cos(theta), TORUS_MINOR * sin(phi), r * sin(theta) };
        }
    }

    int k = 0;
    for (int i = 0; i < rings; i++) {
        for (int j = 0; j < sides; j++) {
            int i2 = (i + 1) % rings, j2 = (j + 1) % sides;
            int a = i  * sides + j,  b = i2 * sides + j;
            int c = i  * sides + j2, d = i2 * sides + j2;
            m.tris[k++] = (Tri){{ a, b, c }};
            m.tris[k++] = (Tri){{ b, d, c }};
        }
    }

    orient_outward(&m, torus_outward);
    return m;
}

static Mesh build_mesh(int choice) {
    switch (choice) {
        case 2:  return mesh_pyramid();
        case 3:  return mesh_sphere(20, 40);
        case 4:  return mesh_torus(40, 20);
        default: return mesh_cube();
    }
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

static int read_choice(void) {
    printf("Choose a shape:\n"
           "  1) cube\n"
           "  2) pyramid\n"
           "  3) sphere\n"
           "  4) torus\n"
           "> ");
    fflush(stdout);

    char line[32];
    if (!fgets(line, sizeof line, stdin)) return 1;
    int c = atoi(line);
    return (c >= 1 && c <= 4) ? c : 1;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    Mesh mesh = build_mesh(read_choice());
    Vec3     *rot = malloc(mesh.nverts * sizeof(Vec3));
    ScreenPt *pts = malloc(mesh.nverts * sizeof(ScreenPt));
    if (!rot || !pts) return 1;

    Vec3 light = normalize(LIGHT_RAW);
    double ax = 0, ay = 0, az = 0;

    printf("\x1b[2J\x1b[?25l");

    while (running) {
        clear_buffers();

        for (int i = 0; i < mesh.nverts; i++) {
            Vec3 p = mesh.verts[i];
            p = rotate_x(p, ax);
            p = rotate_y(p, ay);
            p = rotate_z(p, az);
            rot[i] = p;
            project(p, &pts[i]);
        }

        for (int t = 0; t < mesh.ntris; t++) {
            int ia = mesh.tris[t].v[0];
            int ib = mesh.tris[t].v[1];
            int ic = mesh.tris[t].v[2];
            if (!pts[ia].visible || !pts[ib].visible || !pts[ic].visible)
                continue;

            Vec3 n = normalize(cross(vsub(rot[ib], rot[ia]),
                                     vsub(rot[ic], rot[ia])));
            double d = dot(n, light);
            if (d < 0) d = 0;
            double brightness = AMBIENT + (1.0 - AMBIENT) * d;

            fill_triangle(&pts[ia], &pts[ib], &pts[ic], shade_char(brightness));
        }

        present();
        ax += 0.04;
        ay += 0.03;
        az += 0.02;
        usleep(33000);
    }

    printf("\x1b[?25h\n");
    free(rot);
    free(pts);
    mesh_free(&mesh);
    return 0;
}