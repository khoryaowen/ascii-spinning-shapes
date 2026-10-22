#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#define WIDTH  80
#define HEIGHT 40

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

int main(void) {
    signal(SIGINT, handle_sigint);
    printf("\x1b[2J\x1b[?25l");

    for (int frame = 0; running; frame++) {
        clear_buffer();

        double t = frame * 0.1;
        int x = WIDTH / 2  + (int)(20 * cos(t));
        int y = HEIGHT / 2 + (int)(10 * sin(t));
        put_char(x, y, '#');

        present();
        usleep(33000);
    }

    printf("\x1b[?25h\n");
    return 0;
}