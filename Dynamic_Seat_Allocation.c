#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- CONFIGURATION --- */
#define MAX_ROWS 100
#define MAX_COLS 100
#define MAX_STUDENTS 500
#define DATA_FILE "students.dat"
#define LOG_FILE "allocation_log.txt"

typedef struct {
    int roll;
    char name[50];
    int row, col;
} Student;

typedef struct {
    int occupied;
    int roll;
} Seat;

Seat **hall = NULL;
Student **list = NULL;
int count = 0;
int rows = 0, cols = 0;

/* --- Helper Functions --- */

void log_action(const char *action, int roll, int r, int c) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(fp, "%02d-%02d-%04d %02d:%02d:%02d - %s: Roll=%d at (%d,%d)\n",
            tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900,
            tm->tm_hour, tm->tm_min, tm->tm_sec, action, roll, r, c);
    fclose(fp);
}

void save_binary() {
    FILE *fp = fopen(DATA_FILE, "wb");
    if (!fp) return;
    fwrite(&rows, sizeof(int), 1, fp);
    fwrite(&cols, sizeof(int), 1, fp);
    fwrite(&count, sizeof(int), 1, fp);
    for (int i = 0; i < count; i++) fwrite(list[i], sizeof(Student), 1, fp);
    fclose(fp);
}

void reset_system(const char *reason) {
    printf("<div style='color:red; border:1px solid red; padding:10px;'>System Reset: %s</div>", reason);
    rows = 0; cols = 0; count = 0;
    if (hall) { free(hall); hall = NULL; }
    if (list) { free(list); list = NULL; }
    remove(DATA_FILE); 
}

void ensure_hall_allocated() {
    if (rows <= 0 || cols <= 0) return;
    if (hall) return; 
    
    if (rows > MAX_ROWS || cols > MAX_COLS) {
        reset_system("Dimensions too large (corrupt file).");
        return;
    }

    hall = malloc(rows * sizeof(Seat*));
    if (!hall) { reset_system("Memory allocation failed."); return; }
    
    for (int i = 0; i < rows; i++) {
        hall[i] = calloc(cols, sizeof(Seat));
    }
}

/* This function rebuilds the grid if the user changes rows/cols */
void rebuild_hall_map() {
    if (!hall) { ensure_hall_allocated(); return; }
    
    /* Clear old seat data in the grid to avoid ghosts */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            hall[i][j].occupied = 0;
            hall[i][j].roll = 0;
        }
    }

    /* Place students back into the hall based on the list */
    for (int i = 0; i < count; i++) {
        int r = list[i]->row;
        int c = list[i]->col;
        /* Only place them if they fit in the current dimensions */
        if (r >= 0 && r < rows && c >= 0 && c < cols) {
            hall[r][c].occupied = 1;
            hall[r][c].roll = list[i]->roll;
        }
    }
}

void load_binary() {
    FILE *fp = fopen(DATA_FILE, "rb");
    if (!fp) return;
    
    int r, c, cnt;
    if (fread(&r, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    if (fread(&c, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    if (fread(&cnt, sizeof(int), 1, fp) != 1) { fclose(fp); return; }

    if (r < 0 || r > MAX_ROWS || c < 0 || c > MAX_COLS || cnt < 0 || cnt > MAX_STUDENTS) {
        fclose(fp);
        reset_system("Corrupt data detected in file. Starting fresh.");
        return;
    }

    rows = r; cols = c; count = cnt;
    ensure_hall_allocated();

    if (count > 0 && hall) {
        list = malloc(count * sizeof(Student*));
        for (int i = 0; i < count; i++) {
            list[i] = malloc(sizeof(Student));
            if (fread(list[i], sizeof(Student), 1, fp) != 1) break;
        }
        rebuild_hall_map(); /* Populate the seats */
    }
    fclose(fp);
}

Student *find_student(int roll) {
    for (int i = 0; i < count; i++)
        if (list[i]->roll == roll) return list[i];
    return NULL;
}

void allocate_random(int roll, char *name, char *msg) {
    if (rows == 0 || cols == 0) {
        strcpy(msg, "Please set Rows and Columns first.");
        return;
    }
    if (!hall) ensure_hall_allocated();
    
    if (find_student(roll)) {
        sprintf(msg, "Roll %d is already allocated!", roll);
        return;
    }

    int total = rows * cols;
    /* Basic random placement logic */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (!hall[i][j].occupied) {
                Student *s = malloc(sizeof(Student));
                s->roll = roll;
                strcpy(s->name, name);
                s->row = i;
                s->col = j;
                
                list = realloc(list, (count + 1) * sizeof(Student*));
                list[count++] = s;
                hall[i][j].occupied = 1;
                hall[i][j].roll = roll;
                
                save_binary();
                log_action("ALLOCATED", roll, i, j);
                sprintf(msg, "Allocated %s (%d) at (%d, %d)", name, roll, i, j);
                return;
            }
        }
    }
    strcpy(msg, "Hall is full! Increase rows/cols to add more.");
}

void deallocate_seat(int roll, char *msg) {
    Student *s = find_student(roll);
    if (!s) {
        strcpy(msg, "Roll number not found!");
        return;
    }
    int r = s->row, c = s->col;
    if (hall && r >= 0 && r < rows && c >= 0 && c < cols) {
        hall[r][c].occupied = 0;
        hall[r][c].roll = 0;
    }
    log_action("DEALLOCATED", roll, r, c);
    
    for (int i = 0; i < count; i++) {
        if (list[i]->roll == roll) {
            free(list[i]);
            for (int j = i; j < count - 1; j++) list[j] = list[j + 1];
            count--;
            break;
        }
    }
    
    if (count == 0) {
        free(list);
        list = NULL;
    } else {
        list = realloc(list, count * sizeof(Student*));
    }
    save_binary();
    sprintf(msg, "Deallocated Roll %d from (%d, %d).", roll, r, c);
}

/* --- HTML RENDERERS --- */

void print_header() {
    printf("Content-Type: text/html\n\n");
    printf("<html><head><title>Seat Manager</title>");
    printf("<style>"
           "body{background:#ffffff;color:#000000;font-family:sans-serif;padding:20px;}"
           ".seat{width:70px;height:50px;border:1px solid #7b7b7bff;margin:4px;"
           "border-radius:6px;display:flex;flex-direction:column;justify-content:center;"
           "align-items:center;font-size:16px;}"
           ".occupied{background:#06b6d430;border-color:#06b6d4;}"
           ".row{display:flex;}"
           "</style></head><body>");
    printf("<h1>Seat Allocation System</h1>");
}

void print_menu() {
    printf("<form method='GET'>"
           "<select name='action' style='padding:12px; font-size:18px; width:200px'>"
           "<option value='' selected disabled>Choose an action</option>"
           "<option value='allocate'>Allocate Seat</option>"
           "<option value='deallocate'>Deallocate Seat</option>"
           "<option value='search'>Search Student</option>"
           "<option value='hall'>Display Hall</option>"
           "<option value='log'>View Log</option>"
           "</select>"
           "<button type='submit' style='padding:12px; font-size:18px; width:50px'>Go</button>"
           "</form><br>");
}

void print_forms(char *action, char *msg) {
    if (msg && strlen(msg) > 0)
        printf("<div style='padding:10px;background:#909090ff;border-left:4px solid #06b6d4;'>%s</div><br>", msg);

    if (action && strcmp(action, "allocate") == 0) {
        printf("<form method='GET'>");
        printf("<input type='hidden' name='action' value='allocate'>");
        
        /* FIX: Always allow editing rows and columns. Uses current values as default. */
        printf("Rows: <input name='rows' type='number' value='%d' required style='width:60px'> ", rows > 0 ? rows : 0);
        printf("Cols: <input name='cols' type='number' value='%d' required style='width:60px'> ", cols > 0 ? cols : 0);

        printf("<br><br>Roll: <input name='roll' type='number' required> ");
        printf("Name: <input name='name' type='text' required> ");
        printf("<button type='submit'>Submit</button>");
        printf("</form><br>");
        printf("<small>Note: You can edit Rows/Cols to resize the hall.</small><br><br>");
    } 
    else if (action && (strcmp(action, "deallocate") == 0 || strcmp(action, "search") == 0)) {
        printf("<form method='GET'>");
        printf("<input type='hidden' name='action' value='%s'>", action);
        printf("Roll: <input name='roll' type='number' required> ");
        printf("<button type='submit'>Submit</button>");
        printf("</form><br>");
    }
}

void print_hall_view(char *action) {
    if (action && strcmp(action, "hall") == 0) {
        if (rows <= 0 || cols <= 0) {
            printf("<div>Hall not initialized. Allocate a student to start.</div>");
        } else {
            printf("<h3>Hall (%d x %d)</h3>", rows, cols);
            for (int i = 0; i < rows; i++) {
                printf("<div class='row'>");
                for (int j = 0; j < cols; j++) {
                    if (hall && hall[i][j].occupied) {
                        Student *s = find_student(hall[i][j].roll);
                        if (s) printf("<div class='seat occupied'>%s<br>%d</div>", s->name, s->roll);
                        else printf("<div class='seat occupied'>Unknown<br>%d</div>", hall[i][j].roll);
                    } else printf("<div class='seat'></div>");
                }
                printf("</div>");
            }
        }
    }
}

void print_search_result(char *action) {
    if (action && strcmp(action, "search") == 0) {
        char *qs = getenv("QUERY_STRING");
        if (qs && strstr(qs, "roll=")) {
            char *p = strstr(qs, "roll=");
            int foundRoll = atoi(p + 5);
            Student *s = find_student(foundRoll);
            if (s) {
                printf("<div style='padding:10px;background:#909090ff;border-left:4px solid #0f0;'>"
                       "Found: %s (Roll %d) at Row %d, Col %d</div><br>", 
                       s->name, s->roll, s->row, s->col);
            } else {
                printf("<div style='padding:10px;background:#ffdede;border-left:4px solid #f00;'>Student not found</div><br>");
            }
        }
    }
}

void print_log(char *action) {
    if (action && strcmp(action, "log") == 0) {
        printf("<h3>Log</h3><pre>");
        FILE *fp = fopen(LOG_FILE, "r");
        if (!fp) printf("Log empty!\n");
        else { char c; while ((c = fgetc(fp)) != EOF) putchar(c); fclose(fp); }
        printf("</pre>");
    }
}

int main() {
    print_header();
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 1. Load existing data */
    load_binary();

    char query[512] = "";
    char action[50] = "";
    char rollStr[20] = "";
    char name[50] = "";
    char rowsStr[20] = "";
    char colsStr[20] = "";
    char msg[200] = "";
    msg[0] = '\0';

    char *qs = getenv("QUERY_STRING");
    if (qs) strncpy(query, qs, sizeof(query)-1);

    /* 2. Parse all inputs */
    char *p;
    if ((p = strstr(query, "action="))) sscanf(p + 7, "%[^&]", action);
    if ((p = strstr(query, "rows=")))   sscanf(p + 5, "%[^&]", rowsStr);
    if ((p = strstr(query, "cols=")))   sscanf(p + 5, "%[^&]", colsStr);
    if ((p = strstr(query, "roll=")))   sscanf(p + 5, "%[^&]", rollStr);
    if ((p = strstr(query, "name=")))   sscanf(p + 5, "%[^&]", name);

    /* 3. Handling Resizing (EDIT Rows/Cols) */
    int newRows = rows;
    int newCols = cols;
    
    if (strlen(rowsStr) > 0) newRows = atoi(rowsStr);
    if (strlen(colsStr) > 0) newCols = atoi(colsStr);

    /* If dimensions changed, we must re-allocate the hall */
    if (newRows > 0 && newCols > 0 && (newRows != rows || newCols != cols)) {
        if (hall) {
            /* Free old hall completely */
            for(int i=0; i<rows; i++) free(hall[i]);
            free(hall);
            hall = NULL;
        }
        rows = newRows;
        cols = newCols;
        
        /* Save new dimensions so they stick next time */
        save_binary(); 
        
        /* Rebuild the map with new dimensions (existing students stay in list) */
        ensure_hall_allocated();
        rebuild_hall_map();
        
        strcat(msg, "Hall dimensions updated. ");
    }
    else if (rows > 0 && cols > 0) {
        /* Just ensure it's there if we didn't resize */
        ensure_hall_allocated();
    }

    int roll = atoi(rollStr);

    if (strcmp(action, "allocate") == 0 && roll > 0) {
        allocate_random(roll, name, msg);
    } else if (strcmp(action, "deallocate") == 0 && roll > 0) {
        deallocate_seat(roll, msg);
    }

    print_menu();
    print_forms(action, msg);
    print_search_result(action);
    print_hall_view(action);
    print_log(action);

    printf("</body></html>");
    return 0;
}
