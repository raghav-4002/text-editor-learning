#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>


#define CTRL_KEY(k) ((k) & 0x1f)

#define ARROW_UP 1000
#define ARROW_DOWN 1001
#define ARROW_RIGHT 1002
#define ARROW_LEFT 1003

#define WELCOME_MSG "PlaceHolder"


struct editor_attributes{
    int cx, cy;     // cursor position
    int rows, cols;
    struct termios orig_termios;
};

struct editor_attributes attributes;


struct abuf {
    char *s;
    int len;
};

#define ABUF_INIT {NULL, 0}


void init_editor(void);
void get_window_size(void);
void refresh_screen(void);
void draw_rows(struct abuf *ab);
void process_input(void);
int read_input(void);
void move_cursor(int ch);
void append_buffer(struct abuf *ab, const char *s, int len);
void free_buffer(struct abuf *ab);
void enable_raw_mode(void);
void disable_raw_mode(void);
void die(const char *s);


int
main(void)
{
    enable_raw_mode();
    init_editor();

    while(1) {
        refresh_screen();
        process_input();
    }

    return 0;
}


void
init_editor(void)
{
    attributes.cx = 0, attributes.cy = 0; // set cursor position to top left
    get_window_size();
}


void
get_window_size(void)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) die("ioctl");

    attributes.rows = ws.ws_row;
    attributes.cols = ws.ws_col;    
}


void
refresh_screen(void)
{
    struct abuf ab = ABUF_INIT;

    /* hide cursor */
    append_buffer(&ab, "\x1b[?25l", 6);
    /* repostion cursor to the top */
    append_buffer(&ab, "\x1b[H", 3);

    draw_rows(&ab);

    /* reposition cursor */
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", attributes.cy + 1, attributes.cx + 1);
    append_buffer(&ab, buf, strlen(buf));

    /* unhide the cursor */
    append_buffer(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.s, ab.len);

    free(ab.s);
}


void
draw_rows(struct abuf *ab)
{
    int y;

    for(y = 0; y < attributes.rows; y++) {
        if(y == attributes.rows / 3) {
            char welcome[60];

            int welcome_len = snprintf(welcome, sizeof(welcome),
                                "%s -- A text editor written in C", WELCOME_MSG);
            
            if(welcome_len > attributes.cols) {
                welcome_len = attributes.cols;
            }

            int padding = (attributes.cols - welcome_len) / 2;

            if(padding != 0) {
                append_buffer(ab, "~", 1);
                padding--;
            }

            while(padding != 0) {
                append_buffer(ab, " ", 1);
                padding--;
            }

            append_buffer(ab, welcome, welcome_len);
        } else {
            append_buffer(ab, "~", 1);
        }

        append_buffer(ab, "\x1b[K", 3);

        if(y < attributes.rows - 1) {
            append_buffer(ab, "\r\n", 2);
        }
    }
}


void
process_input(void)
{
    int ch;

    if((ch = read_input()) == -1) die("read_input");

    switch(ch) {
        case CTRL_KEY('q'): case '\x1b':
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;

        case ARROW_UP: case ARROW_DOWN: case ARROW_RIGHT: case ARROW_LEFT:
            move_cursor(ch);
            break;
    }

}


int
read_input(void)
{
    char ch[4] = {0};   /* initialized with 0 to solve a particular bug */

    read(STDIN_FILENO, ch, 4);

    if(ch[0] == '\x1b' && ch[1] == '[') {
        switch(ch[2]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
        }
    }

    return ch[0];
}


void
move_cursor(int ch)
{
    switch(ch) {
        case ARROW_UP:
            attributes.cy--;
            break;

        case ARROW_DOWN:
            attributes.cy++;
            break;

        case ARROW_LEFT:
            attributes.cx--;
            break;

        case ARROW_RIGHT:
            attributes.cx++;
            break;
    }
}


void
append_buffer(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->s, ab->len + len);
    if(new == NULL) die("realloc");

    memcpy(&new[ab->len], s, len);

    ab->s = new;
    ab->len = ab->len + len;
}


void
free_buffer(struct abuf *ab)
{
    free(ab->s);
}


void
enable_raw_mode(void)
{
    /* get terminal attributes */
    if(tcgetattr(STDIN_FILENO, &attributes.orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = attributes.orig_termios;

    /* enable raw mode */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | CS8);
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);

    /* set the updated terminal attributes */
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}


void
disable_raw_mode(void)
{
    /* restore original terminal attributes */
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &attributes.orig_termios) == -1)
        die("tcsetattr");
}


void
die(const char *s)
{
    /* clear screen */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* reposition cursor to top */
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}