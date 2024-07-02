/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)
#define FOU_VERSION "0.0.1"

/*** data ***/

struct editorConfig {
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
		die("tcsetattr");
	}
}

void enableRawMode() {
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcsetattr");
	atexit(disableRawMode);

	struct termios raw = E.orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
 	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

int getCursorPosition(int *rows, int *cols){
	char buff[32];
	unsigned int i = 0;
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buff) - 1) {
		if (read(STDIN_FILENO, &buff[i], 1) != 1) break;
		if (buff[i] == 'R') break;
		i++;
	}
	buff[i] = '\0';

	if (buff[0] != '\xb' || buff[1] != '[') return -1;
	if (sscanf(&buff[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int getWindowSize(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** append buffer ***/

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab){
	free(ab->b);
}

/*** input ***/

void editorMoveCursor(char key) {
	switch (key) {
		case 'a':
			if(E.cx != 0) E.cx--;
			else if((E.cy != 0) & (E.cx == 0)){
				E.cx = E.screencols - 1;
				E.cy--;
			} 
			break;
		case 's':
			if(E.cy < (E.screenrows - 1)) E.cy++;
			break;
		case 'w':
			if(E.cy != 0) E.cy--;
			break;
		case 'd':
			if(E.cx < (E.screencols - 1)) E.cx++;
			else if((E.cy < (E.screenrows - 1)) & (E.cx == (E.screencols - 1))){
				E.cx = 0;
				E.cy++;
			}
			break;
	}
}

void editorProcessKeypress() {
	char c = editorReadKey();

	switch (c) {
		case CTRL_KEY('c'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
     		write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case 'w':
		case 'a':
		case 's':
		case 'd':
			editorMoveCursor(c);
			break;
	}
}

/*** ouput ***/

void editorDrawRows(struct abuf *ab) {
	int y;
	for (y = 0; y < E.screenrows; y++) {
		if (y == E.screenrows/3){
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Fou Editor -- version %s", FOU_VERSION);
			if (welcomelen > E.screenrows) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		} else {
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
	    if (y < E.screenrows - 1) {
	    	abAppend(ab, "\r\n", 2);
	    }
	}
}

void editorRefreshScreen() {
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	char buff[32];
	snprintf(buff, sizeof(buff), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buff, strlen(buff));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

/*** init ***/

void initEdior() {
	E.cx = 0;
	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
	enableRawMode();
	initEdior();
	
	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}
