#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Piece {
	int start;
	int length;
	char* target;
};

struct PieceTable {
	char* content;
	char* add;
	size_t add_size;
	size_t size;
	struct Piece* p;
} pt;

void destroyer() {
	free(pt.content);
	free(pt.add);
	free(pt.p);
}

void printPieces() {
	printf("%s, %s\n", pt.content, pt.add);
	for (int i = 0; i < pt.size; i++) {
		printf("%d,%d %.7s\n", pt.p[i].start, pt.p[i].length, pt.p[i].target == pt.content ? "Content" : "Add");
	}
}

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
	pt.add = malloc(1);
	pt.add[0] = '\0';
	pt.add_size = 1;
	pt.size = 1;
	pt.p = malloc(sizeof(struct Piece));
	pt.p[0].start = 0;
	pt.p[0].length = file_size - 1;
	pt.p[0].target = pt.content;
	if (atexit(destroyer) != 0) {
		perror("Failed to register atexit handler");
		free(pt.content);
		free(pt.p);
		exit(0);
	}
}

void insertInBetween(int x, char c, int insert_index) {
	pt.p = realloc(pt.p, sizeof(struct Piece) * (pt.size + 2));
	if (pt.p == NULL) {
		perror("Memory Allocation Failed!");
		exit(0);
	}

	for(int i = pt.size + 1; (i - 2) > insert_index; i--) {
		pt.p[i] = pt.p[i - 2];
	}

	pt.add = realloc(pt.add, pt.add_size + 1);
	pt.add[pt.add_size - 1] = c;
	pt.add[pt.add_size] = '\0';

	pt.p[insert_index + 1].start = pt.add_size - 1;
	pt.p[insert_index + 1].length = 1;
	pt.p[insert_index + 1].target = pt.add;

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

	for(int i = pt.size; (i - 1) > insert_index; i--) {
		pt.p[i] = pt.p[i - 1];
	}

	pt.add = realloc(pt.add, pt.add_size + 1);
	pt.add[pt.add_size - 1] = c;
	pt.add[pt.add_size] = '\0';

	pt.p[insert_index + 1].start = pt.add_size - 1;
	pt.p[insert_index + 1].length = 1;
	pt.p[insert_index + 1].target = pt.add;

	pt.size += 1;
	pt.add_size += 1;
}

void insertCharacter(int x, char c) {
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
			if (pt.p[i].target == pt.add && (pt.add_size - 1 == pt.p[i].start + pt.p[i].length)){
				pt.add = realloc(pt.add, pt.add_size + 1);
				pt.add[pt.add_size - 1] = c;
				pt.add[pt.add_size] = '\0';
				pt.p[i].length += 1;
				pt.add_size += 1;
				return;
			}
			 else {
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

void deleteInBetwen(int x, int insert_index) {
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

void deleteCharacter(int x) {
	int curr_length = 0;
	for (int i = 0; i < pt.size; i++) {
		curr_length += pt.p[i].length;
		if (x < (curr_length - 1)) {
			x -= (curr_length - pt.p[i].length);
			if (x == 0) {
				deleteAtBeginning(i);
				break;
			}
			deleteInBetwen(x, i);
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

void printMenu() {
	printf("\n========= Menu =========\n");
	printf("1) Add Characters\n");
	printf("2) Del Characters\n");
	printf("3) Exit\n\n");
	printf("Enter your choice: ");
}

int main (int argc, char **argv) {
	if (argc < 2) {
		perror("No file argument given");
		exit(0);
	}
	createPieceTable(argv[1]);
	int choice = -1;
	while(choice != 3) {
		printPieces();
		printMenu();
		scanf("%d", &choice);
		if (choice == 1){
			int pos;
			char c;
			printf("\nEnter the position: ");
			scanf(" %d", &pos);
			printf("Enter the character: ");
			scanf(" %c", &c);
			insertCharacter(pos, c);
		} else if (choice == 2) {
			int pos;
			printf("\nEnter the position: ");
			scanf(" %d", &pos);
			deleteCharacter(pos);
		}
	}
}