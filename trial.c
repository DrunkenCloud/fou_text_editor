/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)
#define FOU_VERSION "0.0.1"
#define FOU_TAB_STOP 8
#define FOU_QUIT_TIMES 3
enum editorKey {
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

/*** data ***/

typedef struct erow {
	int size;
	int indentation;
} erow;

struct editorConfig {
	int cx, cy, rx;
	int rowoff;
	int coloff;
	int screenrows;
	int screencols;
	int actual_x;
	int actual_indentation;
	int numrows;
	erow *rows;
	int dirty;
	int state;
	char * filename;
	char statusmsg[80];
	time_t statusmsg_time;
	struct termios orig_termios;
};

struct editorConfig E;

struct Piece {
	int start;
	int length;
	char **target;
};

struct PieceTable {
	char* content;
	char* add;
	size_t add_size;
	size_t size;
	struct Piece* p;
} pt;

struct details {
	size_t add_size;
	size_t size;
};

int undotop = -1;
int redotop = -1;

struct Piece **undostack;
struct details *undodetails;
struct Piece **redostack;
struct details *redodetails;

void die(const char *s) {
	perror(s);
	exit(1);
}

void undopush() {
	struct Piece **new_undostack = realloc(undostack, sizeof(struct Piece*) * (undotop + 2));
	struct details *new_undodetails = realloc(undodetails, sizeof(struct details) * (undotop + 2));

	if ((new_undostack == NULL || new_undodetails == NULL) && undotop < 0) {
		die("Malloc Error!");
	}

	undostack = new_undostack;
	undodetails = new_undodetails;
	undotop += 1;
	undodetails[undotop].add_size = pt.add_size;
	undodetails[undotop].size = pt.size;
	undostack[undotop] = malloc(sizeof(struct Piece) * (pt.size));
	for(int i = 0; i < pt.size; i++){
		undostack[undotop][i].start = pt.p[i].start;
		undostack[undotop][i].length = pt.p[i].length;
		undostack[undotop][i].target = pt.p[i].target;
	}
	if (redotop >= 0) {
		for (int i = 0; i <= redotop; i++){
			free(redostack[i]);
		}
		free(redostack);
		free(redodetails);
		redotop = -1;
		redostack = (struct Piece**)malloc(0 * sizeof(struct Piece*));
		redodetails = (struct details*)malloc(0 * sizeof(struct details));
	}
}

void redo() {
	if (redotop < 0){
		return;
	}
	struct Piece **new_undostack = realloc(undostack, sizeof(struct Piece*) * (undotop + 2));
	struct details *new_undodetails = realloc(undodetails, sizeof(struct details) * (undotop + 2));

	if ((new_undostack == NULL || new_undodetails == NULL) && undotop < 0) {
		die("Malloc Error!");
	}

	undostack = new_undostack;
	undodetails = new_undodetails;
	undotop += 1;
	undodetails[undotop].add_size = pt.add_size;
	undodetails[undotop].size = pt.size;
	undostack[undotop] = malloc(sizeof(struct Piece) * (pt.size));
	for(int i = 0; i < pt.size; i++){
		undostack[undotop][i].start = pt.p[i].start;
		undostack[undotop][i].length = pt.p[i].length;
		undostack[undotop][i].target = pt.p[i].target;
	}

	pt.add_size = redodetails[redotop].add_size;
	pt.size = redodetails[redotop].size;
	free(pt.p);
	pt.p = malloc(sizeof(struct Piece) * pt.size);
	for (int i = 0; i < pt.size; i++) {
		pt.p[i].start = redostack[redotop][i].start;
		pt.p[i].length = redostack[redotop][i].length;
		pt.p[i].target = redostack[redotop][i].target;
	}

	free(redostack[redotop]);
	struct Piece **new_redostack = realloc(redostack, sizeof(struct Piece*) * redotop);
	struct details *new_redodetails = realloc(redodetails, sizeof(struct details) * redotop);
	if ((new_redostack == NULL || new_redodetails == NULL) && redotop < 0) {
		die("Malloc problem");
	}
	redostack = new_redostack;
	redodetails = new_redodetails;
	redotop -= 1;
}

void undo() {
	if (undotop < 0) {
		return;
	}
	struct Piece **new_redostack = realloc(redostack, sizeof(struct Piece*) * (redotop + 2));
	struct details *new_redodetails = realloc(redodetails, sizeof(struct details) * (redotop + 2));
	if ((new_redostack == NULL || new_redodetails == NULL)) {
		die("Malloc problem");
	}

	redostack = new_redostack;
	redodetails = new_redodetails;
	redotop += 1;
	redodetails[redotop].add_size = pt.add_size;
	redodetails[redotop].size = pt.size;
	redostack[redotop] = malloc(sizeof(struct Piece) * (pt.size));
	for (int i = 0; i < pt.size; i++) {
		redostack[redotop][i].start = pt.p[i].start;
		redostack[redotop][i].length = pt.p[i].length;
		redostack[redotop][i].target = pt.p[i].target;
	}

	pt.add_size = undodetails[undotop].add_size;
	pt.size = undodetails[undotop].size;
	free(pt.p);
	pt.p = malloc(sizeof(struct Piece) * pt.size);
	for (int i = 0; i < pt.size; i++) {
		pt.p[i].start = undostack[undotop][i].start;
		pt.p[i].length = undostack[undotop][i].length;
		pt.p[i].target = undostack[undotop][i].target;
	}

	free(undostack[undotop]);
	struct Piece **new_undostack = realloc(undostack, sizeof(struct Piece*) * undotop);
	struct details *new_undodetails = realloc(undodetails, sizeof(struct details) * undotop);

	if ((new_undostack == NULL || new_undodetails == NULL) & undotop < 0) {
		die("Malloc Error!");
	}

	undostack = new_undostack;
	undodetails = new_undodetails;
	undotop -= 1;
}

void editorMoveCursor(int key);

/*** terminal ***/

void disableRawMode() {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
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

int editorReadKey() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}

	if (c == '\x1b') {
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '~'){
					switch (seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}

		return '\x1b';
	} else {
		if (E.state == 0){
			switch (c) {
				case 'w': return ARROW_UP;
				case 'a': return ARROW_LEFT;
				case 's': return ARROW_DOWN;
				case 'd': return ARROW_RIGHT;
			}
		}
		return c;
	}
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
		if (write(STDOUT_FILENO, "\x1b[10000000C\x1b[1000000B", 12) != 12) return -1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** piece table operations***/

void destroyer() {
	for (int i = 0; i <= undotop; i++) {
		free(undostack[i]);
	}
	for (int i = 0; i <= redotop; i++) {
		free(redostack[i]);
	}
	free(undostack);
	free(redostack);
	free(undodetails);
	free(redodetails);
	undotop = -1;
	redotop = -1;
	free(pt.p);
	free(pt.content);
	free(pt.add);
	free(E.rows);
	free(E.filename);
}

void printPieces() {
	for (int i = 0; i < pt.size; i++) {
		printf("\n%d,%d %.7s\n", pt.p[i].start, pt.p[i].length, *pt.p[i].target == pt.content ? "Content" : "Add");
	}
	printf("\n%d, %d\n", E.cx, E.cy);
	for(int i = 0; i < E.numrows; i++) {
		printf("%d,%d,%d\n", E.numrows, E.rows[i].size, E.rows[i].indentation);
	}
}

void insertInBetween(int x, char c, int insert_index) {
	pt.p = realloc(pt.p, sizeof(struct Piece) * (pt.size + 2));
	if (pt.p == NULL) {
		perror("Memory Allocation Failed!");
		exit(0);
	}

	for (int i = pt.size + 1; i > insert_index + 1; i--) {
		pt.p[i] = pt.p[i - 2];
	}

	char *new_add = realloc(pt.add, pt.add_size + sizeof(char));
	if (new_add == NULL) {
		perror("Memory Allocation Failed!");
		exit(0);
	}
	new_add[pt.add_size] = c;
	pt.add = new_add;

	pt.p[insert_index + 1].start = pt.add_size;
	pt.p[insert_index + 1].length = 1;
	pt.p[insert_index + 1].target = &pt.add;

	pt.p[insert_index + 2].start = pt.p[insert_index].start + x;
	pt.p[insert_index + 2].length = pt.p[insert_index].length - x;
	pt.p[insert_index + 2].target = pt.p[insert_index].target;
	pt.p[insert_index].length = x;

	pt.size += 2;
	pt.add_size += 1;
}

void insertAtEnd(char c, int insert_index) {
	pt.p = realloc(pt.p, sizeof(struct Piece) * (pt.size + 1));
	if (pt.p == NULL) {
		perror("Memory Allocation Failed!");
		exit(0);
	}

	for (int i = pt.size; i > insert_index + 1; i--) {
		pt.p[i] = pt.p[i - 1];
	}

	char *new_add = realloc(pt.add, pt.add_size + sizeof(char));
	if (new_add == NULL) {
		perror("Memory Allocation Failed!");
		exit(0);
	}
	new_add[pt.add_size] = c;
	pt.add = new_add;

	pt.p[insert_index + 1].start = pt.add_size;
	pt.p[insert_index + 1].length = 1;
	pt.p[insert_index + 1].target = &pt.add;

	pt.size += 1;
	pt.add_size += 1;
}

void insertCharacter(char c) {
	int x = 0;
	for(int i = 0; i < E.cy; i++) {
		x += (E.rows[i].size + E.rows[i].indentation + 1);
	}
	x += E.cx;
	E.cx += 1;
	int insert_index = -1;
	int curr_length = 0;
	for (int i = 0; i < pt.size; i++) {
		curr_length += pt.p[i].length;
		if (x < curr_length) {
			x -= (curr_length - pt.p[i].length);
			insert_index = i;
			insertInBetween(x, c, insert_index);
			break;
		} else if (x == curr_length) {
			if (*pt.p[i].target == pt.add && (pt.add_size == pt.p[i].start + pt.p[i].length)){
				char *new_add = realloc(pt.add, pt.add_size + sizeof(char));
				if (new_add == NULL) {
					perror("Memory Allocation Failed!");
					exit(0);
				}
				new_add[pt.add_size] = c;
				pt.add = new_add;
				pt.p[i].length += 1;
				pt.add_size += 1;
				return;
			} else {
			 	insert_index = i;
			 	insertAtEnd(c, insert_index);
			 	break;
			 }
		}
	}
	if (insert_index == -1) {
		printf("Out of bounds index %d", x);
		return;
	}
}

void deleteInBetween(int x, int insert_index) {
	pt.p = realloc(pt.p, sizeof(struct Piece) * (pt.size + 1));
	if (pt.p == NULL) {
		perror("Memory Allocation Failed!");
		exit(0);
	}
	for(int i = pt.size; (i - 1) > insert_index; i--) {
		pt.p[i] = pt.p[i - 1];
	}
	pt.p[insert_index + 1].length = pt.p[insert_index].length;
	pt.p[insert_index].length = x;
	pt.p[insert_index + 1].start = pt.p[insert_index].start + x + 1;
	pt.p[insert_index + 1].length -= (x + 1);
	pt.p[insert_index + 1].target = pt.p[insert_index].target;
	pt.size += 1;
}

void deleteAtEnd(int insert_index) {
	if (pt.p[insert_index].length == 1) {
		for (int i = insert_index; i < pt.size - 1; i++){
			pt.p[i] = pt.p[i + 1];
		}
		pt.p = realloc(pt.p, sizeof(struct Piece) * (pt.size - 1));
		pt.size -= 1;
	} else {
		pt.p[insert_index].length -= 1;
	}
}

void deleteAtBeginning(int insert_index) {
	if (pt.p[insert_index].length == 1) {
		for (int i = insert_index; i < pt.size - 1; i++){
			pt.p[i] = pt.p[i + 1];
		}
		pt.p = realloc(pt.p, sizeof(struct Piece) * (pt.size - 1));
		pt.size -= 1;
	} else {
		pt.p[insert_index].start += 1;
		pt.p[insert_index].length -= 1;
	}
}

void deleteCharacter() {
	int x = 0;
	for(int i = 0; i < E.cy; i++) {
		x += (E.rows[i].size + E.rows[i].indentation + 1);
	}
	x += E.cx;
	E.cx -= 1;
	int curr_length = 0;
	for (int i = 0; i < pt.size; i++) {
		curr_length += pt.p[i].length;
		if (x < (curr_length - 1)) {
			x -= (curr_length - pt.p[i].length);
			if (x == 0) {
				deleteAtBeginning(i);
				break;
			}
			deleteInBetween(x, i);
			break;
		} else if (x == (curr_length - 1)) {
			deleteAtEnd(i);
			break;
		} else if (x == (curr_length)) {
			deleteAtBeginning(i + 1);
			break;
		}
	}
}

/*** file i/o ***/

void createPieceTable(char* file_name) {
	FILE* file = fopen(file_name, "r");
	if (file == NULL) {
		perror("Error Opening file");
		exit(0);
	}

	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	rewind(file);

	pt.content = malloc(file_size + 1);
	if (pt.content == NULL){		
		perror("Memory Allocation for PieceTable Failed");
		fclose(file);
		exit(0);
	}

	size_t read_size = fread(pt.content, 1, file_size, file);
	fclose(file);

	if (read_size != file_size) {
		perror("Error loading file into memory");
		free(pt.content);
		exit(0);
	}

	pt.content[file_size] = '\0';
	pt.add = malloc(0);
	pt.add_size = 0;
	pt.size = 1;
	pt.p = malloc(sizeof(struct Piece));
	pt.p[0].start = 0;
	pt.p[0].length = file_size;
	pt.p[0].target = &pt.content;
	if (atexit(destroyer) != 0) {
		perror("Failed to register atexit handler");
		free(pt.content);
		free(pt.p);
		exit(0);
	}
}

void initialiseconfig() {
	int j = 0;
	int size = 0;
	int indentation = 0;
	E.rows = malloc(1 * sizeof(erow));
	for(int i = 0; i < pt.p[0].length; i++) {
		if ((*pt.p[0].target)[i] == '\n') {
			if (j != 0){
				E.rows = realloc(E.rows, sizeof(erow) * (j + 1));
			}
			E.rows[j].size = size;
			E.rows[j].indentation = indentation;
			size = 0;
			indentation = 0;
			j += 1;
		} else if ((*pt.p[0].target)[i] == '\t') {
			indentation += 1;
		} else {
			size += 1;
		}
	}

	if (size != 0 || indentation != 0) {
		E.rows = realloc(E.rows, sizeof(erow) * (j + 1));
		E.rows[j].size = size + 1;
		E.rows[j].indentation = indentation;
	} else {
		E.rows = realloc(E.rows, sizeof(erow) * (j + 1));
		E.rows[j].size = 1;
	}
	j += 1;

	E.numrows = j;
}

void remakeconfig() {
	int j = 0;
	int size = 0;
	int indentation = 0;
	if (E.rows != NULL){
		free(E.rows);
		E.rows = NULL;	
	}
	E.rows = malloc(sizeof(erow));
	for(int k = 0; k < pt.size; k++){
		for(int i = 0; i < pt.p[k].length; i++) {
			if ((*pt.p[k].target)[pt.p[k].start + i] == '\n') {
				if (j != 0) {
					E.rows = realloc(E.rows, sizeof(erow) * (j + 1));
				}
				E.rows[j].size = size;
				E.rows[j].indentation = indentation;
				size = 0;
				indentation = 0;
				j += 1;
			} else if ((*pt.p[k].target)[pt.p[k].start + i] == '\t') {
				indentation += 1;
			} else {
				size += 1;
			}
		}
	}

	if (size != 0 || indentation != 0) {
		E.rows = realloc(E.rows, sizeof(erow) * (j + 1));
		E.rows[j].size = size;
		E.rows[j].indentation = indentation;
		j += 1;
	}
	E.numrows = j;
}

/*** append buffer ***/

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
	if (len > 0 && s != NULL) {
		if (ab->len > INT_MAX - len) return;

		char *new_buffer = realloc(ab->b, ab->len + len);
		
		if (new_buffer == NULL) return;

		memcpy(&new_buffer[ab->len], s, len);
		ab->b = new_buffer;
		ab->len += len;
	}
}

void abFree(struct abuf *ab){
	free(ab->b);
	ab->len = 0;
}

/*** input ***/

void editorMoveCursor(int key) {
	switch (key) {
		case ARROW_LEFT:
			if(E.cx != 0) E.cx--;
			else if((E.cy > 0) & (E.cx == 0)){
				E.cy--;
				E.cx = E.rows[E.cy].size + E.rows[E.cy].indentation - 1;
			}
			E.actual_indentation = E.rows[E.cy].indentation;
			E.actual_x = E.cx - E.rows[E.cy].indentation;
			break;
		case ARROW_RIGHT:
			if(E.cx < E.rows[E.cy].size - 1) E.cx++;
			else if(E.cy < (E.numrows - 1)){
				E.cx = 0;
				E.cy++;
			}
			E.actual_indentation = E.rows[E.cy].indentation;
			E.actual_x = E.cx - E.rows[E.cy].indentation;
			break;
		case ARROW_UP:
			if(E.cy > 0) {
				E.cy--;
				if(E.actual_x + E.actual_indentation > E.rows[E.cy].size + E.rows[E.cy].indentation){
					E.cx = E.rows[E.cy].size + E.rows[E.cy].indentation -1;
				} else {
					E.cx = E.actual_x + E.actual_indentation;
				}
			}
			break;
		case ARROW_DOWN:
			if(E.cy < E.numrows - 1) {
				E.cy++;
				if(E.actual_x + E.actual_indentation > E.rows[E.cy].size + E.rows[E.cy].indentation){
					E.cx = E.rows[E.cy].size + E.rows[E.cy].indentation - 1;
				} else {
					E.cx = E.actual_x + E.actual_indentation;
				}
			}
			break;
	}
}

void convertCxToRx() {
	E.rx = 0;
	for(int i = 0; i < E.cx; i++) {
		if (i < E.rows[E.cy].indentation) {
			E.rx += FOU_TAB_STOP;
			E.rx += 1;
		} else {
			E.rx += 1;
		}
	}
}

void editorProcessKeypress() {
	static int quit_times = FOU_QUIT_TIMES;

	int c = editorReadKey();

	switch (c) {

		case CTRL_KEY('c'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
	 		write(STDOUT_FILENO, "\x1b[H", 3);
	 		disableRawMode();
	 		write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
			exit(0);
			break;

		case CTRL_KEY('y'):
			redo();
			break;

		case CTRL_KEY('z'):
			undo();
			break;

		case CTRL_KEY('s'):
			break;

		case '\r':
			undopush();
			insertCharacter('\r');
			insertCharacter('\n');
			E.cy += 1;
			E.cx = 0;
			E.actual_x = 0;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				if (c == PAGE_UP) {
					E.cy = E.rowoff;
				} else if (c == PAGE_DOWN) {
					E.cy = E.rowoff + E.screenrows - 1;
					if (E.cy > E.numrows) {
						E.cy = E.numrows;
					}
				}
				int times = E.screenrows;
				while (times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			break;

		case BACKSPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
			undopush();
			deleteCharacter();
			remakeconfig();
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
   			editorMoveCursor(c);
			break;

		case CTRL_KEY('l'):
		case '\x1b':
			E.state ^= 1;
			break;

		default:
			undopush();
			insertCharacter(c);
			break;
	}

	quit_times = FOU_QUIT_TIMES;
}

/*** ouput ***/

void pieceTabletoBuffer(struct abuf *ab) {
	int final_size = 0;
	for (int i = 0; i < E.numrows; i++) {
		final_size += E.rows[i].size;
		final_size += E.rows[i].indentation * (FOU_TAB_STOP);
	}
	char* final = malloc(final_size + E.numrows + 3*E.numrows);
	int pos = 0;
	for (int i = 0; i < pt.size; i++) {
		for (int j = 0; j < pt.p[i].length; j++) {
			if ((*pt.p[i].target)[pt.p[i].start + j] == '\t') {
				for (int k = 0; k < FOU_TAB_STOP; k++) {
					final[pos] = ' ';
					pos += 1;
				}
			} else if ((*pt.p[i].target)[pt.p[i].start + j] == '\n') {
				final[pos] = '\n';
				final[pos + 1] = '\x1b';
				final[pos + 2] = '[';
				final[pos + 3] = 'K';
				pos += 4;
			} else {
				final[pos] = (*pt.p[i].target)[pt.p[i].start + j];
				pos += 1;
			}
		}
	}
	abAppend(ab, final, pos);
	free(final);
}

void scrollTabletoBuffer(struct abuf *ab) {

}

void editorDrawRows(struct abuf *ab) {
	int y;
	for (y = 0; y < E.screenrows; y++) {
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows){
			if (E.numrows == 0 && y == E.screenrows/3){
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome), "Fou Editor -- version %s", FOU_VERSION);
				if (welcomelen > E.screencols) welcomelen = E.screencols;
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
		} else {
			abAppend(ab, "\x1b[K", 4);
			pieceTabletoBuffer(ab);
			break;
		}
		abAppend(ab, "\x1b[K", 3);
		if (y < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen() {
	remakeconfig();
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	convertCxToRx(&E.rx);
	char buff[32];
	snprintf(buff, sizeof(buff), "\x1b[%d;%dH", E.cy + 1, E.rx + 1);
	abAppend(&ab, buff, strlen(buff));

	abAppend(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

/*** init ***/

void initEditor() {
	E.cy = 0;
	E.cx = 0;
	E.rx = 0;
	E.numrows = 0;
	E.actual_x = 0;
	E.actual_indentation = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.rows = NULL;
	E.dirty = 0;
	E.state = 0;
	E.filename = NULL;
	E.statusmsg[0] = '\0';
	E.statusmsg_time = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
	E.screenrows -= 2;
}

void initData(char* file_name) {
	if (E.filename != NULL){
		free(E.filename);
		E.filename = NULL;
	}
	E.filename = malloc(strlen(file_name) + 1);
	if (E.filename == NULL) {
		perror("malloc E.filename");
		exit(0);
	}

	strcpy(E.filename, file_name);
	createPieceTable(file_name);
	initialiseconfig();
	E.dirty = 0;
	undostack = (struct Piece**)malloc(0 * sizeof(struct Piece*));
	undodetails = (struct details*)malloc(0 * sizeof(struct details));
	redostack = (struct Piece**)malloc(0 * sizeof(struct Piece*));
	redodetails = (struct details*)malloc(0 * sizeof(struct details));
}

int main(int argc, char *argv[]) {
	enableRawMode();
	initEditor();
	if (argc >= 2) {
		write(STDOUT_FILENO, "\x1b[2J", 4);
		initData(argv[1]);
	}

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}