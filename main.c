/**
 * @file main.c
 * @brief Unied - A basic terminal text editor with language-independent syntax highlighting,
 * custom cursor, an extensible "Command Puzzle System", and adaptive keyboard modes.
 *
 * This program provides a nano-like text editing experience in the terminal,
 * featuring file loading/saving, cursor navigation, basic text insertion/deletion,
 * a heuristic-based syntax highlighter, and an innovative command input system.
 * It uses the ncurses library for terminal interaction. The traditional blinking
 * cursor is hidden, and the current character at the cursor position is highlighted.
 *
 * @author Ferki
 * @date 2025-07-20
 */

#include <ncurses.h>   // For terminal UI
#include <stdio.h>     // For file I/O
#include <stdlib.h>    // For malloc, free, exit
#include <string.h>    // For string manipulation
#include <stdbool.h>   // For bool type
#include <ctype.h>     // For isalnum, isdigit, isspace, isxdigit, tolower, toupper
#include <time.h>      // For status message timestamp
#include <stdarg.h>    // For variadic functions (set_status_message)
#include <errno.h>     // For strerror
#include <sys/stat.h>  // For stat()
#include <unistd.h>    // For usleep

// --- Editor Configuration Constants ---
#define TAB_STOP 4
#define MAX_LINE_LENGTH_BUFFER 256
#define MAX_STATUS_MESSAGE_LENGTH 256
#define BORDER_WIDTH 1
#define HINT_ROWS 2
#define SUGGESTION_ROWS 3
#define MAX_RECENT_FILES 10
#define MAX_UNDO_HISTORY 100
#define COMMAND_TIMEOUT_MS 1500
#define MAX_COMMAND_SEQUENCE_LENGTH 10
#define MAX_MACROS 50
#define MAX_MACRO_ACTION_LENGTH 50

// --- Ncurses Color Pair Definitions ---
#define COLOR_PAIR_DEFAULT 1
#define COLOR_PAIR_COMMENT 2
#define COLOR_PAIR_STRING 3
#define COLOR_PAIR_NUMBER 4
#define COLOR_PAIR_OPERATOR 5
#define COLOR_PAIR_KEYWORD 6
#define COLOR_PAIR_STATUS_BAR 8
#define COLOR_PAIR_CURSOR 9
#define COLOR_PAIR_HINTS 11
#define COLOR_PAIR_SUGGESTIONS 12
#define COLOR_PAIR_SELECTION 13
#define COLOR_PAIR_BORDER 14

// --- Macro to convert a character to its Ctrl-key equivalent ---
#define CTRL(c) ((c) & 0x1f)

// --- Highlighting Type Enum ---
typedef enum {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_STRING,
    HL_NUMBER,
    HL_OPERATOR,
    HL_KEYWORD,
} HighlightType;

// --- Editor Line Structure ---
typedef struct {
    char *chars;
    int size;
    int allocated;
    HighlightType *hl;
    int hl_revision;
} EditorLine;

// --- Command State Structure ---
typedef struct {
    char sequence[MAX_COMMAND_SEQUENCE_LENGTH];
    int length;
    time_t last_key_time;
    bool active;
    bool show_help;
} CommandState;

// --- Keyboard Mode Enum ---
typedef enum {
    NORMAL_KB_MODE,
    ANDROID_KB_MODE // Kept for completeness, but WASD no longer moves cursor
} KeyboardMode;

// --- Undo/Redo Type Enum ---
typedef enum {
    UNDO_INSERT_CHAR,
    UNDO_DELETE_CHAR,
    UNDO_INSERT_EMPTY_LINE,
    UNDO_SPLIT_LINE,
    UNDO_JOIN_LINES,
    UNDO_INSERT_BLOCK,
    UNDO_DELETE_BLOCK,
    UNDO_MODIFY_LINE_CASE
} UndoType;

// --- Undo/Redo Action Structure ---
typedef struct {
    UndoType type;
    int y;
    int x;
    char char_val;
    char *text_content;
    int text_len;
    int num_lines_affected;
} UndoAction;

// --- Editor Macro Structure ---
typedef struct {
    char sequence[MAX_COMMAND_SEQUENCE_LENGTH];
    char action[MAX_MACRO_ACTION_LENGTH];
} EditorMacro;

// --- Global Editor State Structure ---
typedef struct {
    EditorLine *lines;
    int num_lines;
    int allocated_lines;

    int cursor_x;
    int cursor_y;

    int screen_rows;
    int screen_cols;
    int total_screen_rows;

    int scroll_y;
    int scroll_x;

    char *filename;
    bool dirty;

    char status_message[MAX_STATUS_MESSAGE_LENGTH];
    time_t status_message_time;

    bool in_multiline_comment_global;

    int dirty_line_start;
    int dirty_line_end;

    CommandState cmd;

    char *clipboard_buffer;
    int clipboard_size;
    int clipboard_allocated;

    EditorMacro macros[MAX_MACROS];
    int macro_count;
    bool creative_mode;

    KeyboardMode keyboard_mode;
    bool is_code_file;

    bool visual_mode;
    int visual_start_x, visual_start_y;

    char last_search_query[MAX_STATUS_MESSAGE_LENGTH];
    int last_search_found_y;
    int last_search_found_x;
    bool search_active;

    bool show_line_numbers;

    char *recent_files[MAX_RECENT_FILES];
    int num_recent_files;

    UndoAction undo_history[MAX_UNDO_HISTORY];
    int undo_head;
    UndoAction redo_history[MAX_UNDO_HISTORY];
    int redo_head;

} EditorState;

// Global instance of the editor state
EditorState E;

// --- Function Prototypes (all functions declared before main) ---
// Core Editor Functions
void init_editor(void);
void deinit_editor(void);
int editor_read_key(void);
void editor_move_cursor(int key);
void editor_line_insert_char(EditorLine *line, int at, int c);
void editor_line_delete_char(EditorLine *line, int at);
void editor_insert_line(int at, const char *s, int len);
void editor_delete_line(int at);
void editor_insert_char(int c);
void editor_delete_char(void);
void editor_insert_newline(void);
void editor_load_file(const char *filename);
void editor_save_file(void);
int editor_row_cx_to_rx(const EditorLine *row, int cx);
int editor_row_rx_to_cx(const EditorLine *row, int cx);

// UI Functions
void set_status_message(const char *fmt, ...);
bool is_operator_char(char c);
bool is_double_operator(const char *str, int index);
void mark_lines_dirty(int start, int end);
void update_highlighting(EditorLine *line);
static void print_char_with_highlight(int c, int base_color_pair, bool is_cursor_char);
static int get_color_pair_for_highlight_type(HighlightType type);
void editor_draw_line_highlighted(const EditorLine *line, int line_idx, int screen_y, int line_num_offset_x);
void clear_suggestion_area(void);
void show_command_suggestions(void);
void editor_draw_hints(void);
void editor_refresh_screen(void);
char *editor_prompt(const char *prompt_msg, char *buffer, size_t buf_size);
bool show_confirmation_dialog(const char *prompt_msg);
void display_loading_screen(void);
void editor_set_file_type(bool is_code);
void prompt_file_type(void);
void show_command_help_screen(void);

// Command and Extended Functions
void reset_command_mode(void);
void execute_custom_command(const char* action);
void execute_command_sequence(void);
void handle_command_mode_input(int key);
void autocomplete_command(void);
void editor_quit(bool force_quit);
void editor_save_as(void);
void editor_open_file(void);
void editor_find(void);
void editor_find_next(void);
void editor_find_prev(void);
void editor_find_replace(void);
void editor_copy_line(void);
void editor_cut_line(void);
void editor_paste_line(void);
void editor_show_file_info(void);
void editor_goto_line(void);
void editor_toggle_line_numbers(void);
void editor_show_recent_files(void);
void enter_creative_mode(void);
void editor_duplicate_line(void);
void editor_change_line_case(bool to_upper);
void editor_set_keyboard_mode(KeyboardMode mode);
void move_to_word_start(void);
void move_to_word_end(void);
void editor_toggle_visual_mode(void);
bool is_char_in_selection(int row, int col);
void editor_copy_selection(void);
void editor_cut_selection(void);
void editor_delete_selection(void);
void get_normalized_selection_coords(int *sy, int *sx, int *ey, int *ex);
char *get_selection_content(int sy, int sx, int ey, int ex, int *num_lines);
void editor_insert_text_block(int y, int x, const char *text, int text_len);
void editor_delete_text_block(int sy, int sx, int ey, int ex);
void add_to_recent_files(const char *filename);
void init_undo_redo(void);
void free_undo_action(UndoAction *action);
void push_undo_action(UndoAction action);
void push_redo_action(UndoAction action);
void clear_redo_history(void);
void editor_undo(void);
void editor_redo(void);
void editor_select_all(void);
void editor_process_keypress(void);

// --- Core Editor Function Implementations ---

/**
 * @brief Initializes the ncurses environment and editor state.
 *
 * Sets up raw mode, enables keypad, initializes color pairs, and sets initial
 * editor state values. Hides the traditional terminal cursor.
 */
void init_editor(void) {
    initscr();             // Initialize the curses screen
    raw();                 // Line buffering disabled. Pass everything to the program.
    noecho();              // Don't echo input characters
    keypad(stdscr, TRUE);  // Enable special keys (arrow keys, F-keys)
    curs_set(1);           // Set cursor to visible (1 for underline, 2 for block)

    // Check for color support
    if (has_colors()) {
        start_color();
        // Define color pairs: FOREGROUND, BACKGROUND
        init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_COMMENT, COLOR_GREEN, COLOR_BLACK);
        init_pair(COLOR_PAIR_STRING, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COLOR_PAIR_NUMBER, COLOR_CYAN, COLOR_BLACK);
        init_pair(COLOR_PAIR_OPERATOR, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_PAIR_KEYWORD, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(COLOR_PAIR_STATUS_BAR, COLOR_BLACK, COLOR_CYAN);
        init_pair(COLOR_PAIR_CURSOR, COLOR_BLACK, COLOR_WHITE);
        init_pair(COLOR_PAIR_HINTS, COLOR_WHITE, COLOR_BLUE);
        init_pair(COLOR_PAIR_SUGGESTIONS, COLOR_BLACK, COLOR_GREEN);
        init_pair(COLOR_PAIR_SELECTION, COLOR_BLACK, COLOR_YELLOW);
        init_pair(COLOR_PAIR_BORDER, COLOR_WHITE, COLOR_BLACK);
    }

    E.num_lines = 0;
    E.allocated_lines = 0;
    E.lines = NULL;
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.scroll_y = 0;
    E.scroll_x = 0;
    E.filename = NULL;
    E.dirty = false;
    E.status_message[0] = '\0';
    E.status_message_time = 0;
    E.in_multiline_comment_global = false;
    E.dirty_line_start = -1;
    E.dirty_line_end = -1;

    // Command State Init
    E.cmd.active = false;
    E.cmd.length = 0;
    E.cmd.sequence[0] = '\0';
    E.cmd.last_key_time = 0;
    E.cmd.show_help = false;

    // Clipboard Init
    E.clipboard_buffer = NULL;
    E.clipboard_size = 0;
    E.clipboard_allocated = 0;

    // Macro System Init
    E.macro_count = 0;
    E.creative_mode = false;

    // Keyboard Mode Init
    E.keyboard_mode = NORMAL_KB_MODE;
    E.is_code_file = false;

    // Visual Mode Init
    E.visual_mode = false;
    E.visual_start_x = 0;
    E.visual_start_y = 0;

    // Search state init
    E.last_search_query[0] = '\0';
    E.last_search_found_y = -1;
    E.last_search_found_x = -1;
    E.search_active = false;

    E.show_line_numbers = false;

    // Recent Files Init
    E.num_recent_files = 0;
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        E.recent_files[i] = NULL;
    }

    // Undo/Redo Init
    init_undo_redo();

    getmaxyx(stdscr, E.total_screen_rows, E.screen_cols);
    E.screen_rows = E.total_screen_rows - HINT_ROWS - 1 - SUGGESTION_ROWS - 2 * BORDER_WIDTH;

    // Add explicit hint for new users
    set_status_message("Help: Ctrl+S = Save | Ctrl+O = Open | Ctrl+Q = Quit | Ctrl+H = Help");
}

/**
 * @brief Deinitializes the ncurses environment and frees allocated memory.
 */
void deinit_editor(void) {
    endwin();

    for (int i = 0; i < E.num_lines; i++) {
        free(E.lines[i].chars);
        free(E.lines[i].hl);
    }
    free(E.lines);
    free(E.filename);
    free(E.clipboard_buffer);

    for (int i = 0; i < E.num_recent_files; i++) {
        free(E.recent_files[i]);
    }

    for (int i = 0; i < E.undo_head; i++) {
        free_undo_action(&E.undo_history[i]);
    }
    for (int i = 0; i < E.redo_head; i++) {
        free_undo_action(&E.redo_history[i]);
    }
}

/**
 * @brief Reads a single character/key from ncurses input.
 *
 * @return The integer value of the key pressed.
 */
int editor_read_key(void) {
    int c = getch();
    if (c == KEY_RESIZE) {
        getmaxyx(stdscr, E.total_screen_rows, E.screen_cols);
        E.screen_rows = E.total_screen_rows - HINT_ROWS - 1 - SUGGESTION_ROWS - 2 * BORDER_WIDTH;
        editor_refresh_screen(); // Trigger redraw on resize
    }
    return c;
}

/**
 * @brief Moves the editor cursor based on the key pressed.
 *
 * @param key The ncurses key code (e.g., KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT).
 */
void editor_move_cursor(int key) {
    mark_lines_dirty(E.cursor_y, E.cursor_y); // Mark current line dirty before move

    EditorLine *line = (E.cursor_y >= E.num_lines) ? NULL : &E.lines[E.cursor_y];

    switch (key) {
        case KEY_LEFT:
            if (E.cursor_x > 0) {
                E.cursor_x--;
            } else if (E.cursor_y > 0) {
                E.cursor_y--;
                E.cursor_x = E.lines[E.cursor_y].size;
            }
            break;
        case KEY_RIGHT:
            if (line && E.cursor_x < line->size) {
                E.cursor_x++;
            } else if (line && E.cursor_x == line->size && E.cursor_y < E.num_lines - 1) {
                E.cursor_y++;
                E.cursor_x = 0;
            }
            break;
        case KEY_UP:
            if (E.cursor_y > 0) {
                E.cursor_y--;
            }
            break;
        case KEY_DOWN:
            if (E.cursor_y < E.num_lines) {
                E.cursor_y++;
            }
            break;
    }

    line = (E.cursor_y >= E.num_lines) ? NULL : &E.lines[E.cursor_y];
    int line_len = line ? line->size : 0;
    if (E.cursor_x > line_len) {
        E.cursor_x = line_len;
    }
    mark_lines_dirty(E.cursor_y, E.cursor_y); // Mark new line dirty after move
}

/**
 * @brief Inserts a character into the current line at the cursor position.
 *
 * Dynamically resizes the line buffer if necessary.
 *
 * @param line Pointer to the EditorLine to modify.
 * @param at The character index where the character should be inserted.
 * @param c The character to insert.
 */
void editor_line_insert_char(EditorLine *line, int at, int c) {
    if (at < 0 || at > line->size) return;

    if (line->size + 2 > line->allocated) {
        int new_allocated = line->allocated == 0 ? MAX_LINE_LENGTH_BUFFER : line->allocated * 2;
        line->chars = realloc(line->chars, (size_t)new_allocated);
        line->hl = realloc(line->hl, sizeof(HighlightType) * (size_t)new_allocated);
        if (line->chars == NULL || line->hl == NULL) {
            set_status_message("Error: Failed to reallocate line memory.");
            return;
        }
        line->allocated = new_allocated;
    }

    memmove(&line->chars[at + 1], &line->chars[at], (size_t)(line->size - at + 1));
    memmove(&line->hl[at + 1], &line->hl[at], sizeof(HighlightType) * (size_t)(line->size - at + 1));
    line->chars[at] = (char)c;
    line->size++;
    line->chars[line->size] = '\0';
    line->hl_revision++;
}

/**
 * @brief Deletes a character from the current line at the specified position.
 *
 * @param line Pointer to the EditorLine to modify.
 * @param at The character index to delete.
 */
void editor_line_delete_char(EditorLine *line, int at) {
    if (at < 0 || at >= line->size) return;

    memmove(&line->chars[at], &line->chars[at + 1], (size_t)(line->size - at));
    memmove(&line->hl[at], &line->hl[at + 1], sizeof(HighlightType) * (size_t)(line->size - at));
    line->size--;
    line->chars[line->size] = '\0';
    line->hl_revision++;
}

/**
 * @brief Inserts a new line into the editor at the specified row index.
 *
 * @param at The row index where the new line should be inserted.
 * @param s Optional string to initialize the new line with.
 * @param len Length of the initial string.
 */
void editor_insert_line(int at, const char *s, int len) {
    if (at < 0 || at > E.num_lines) return;

    if (E.num_lines + 1 > E.allocated_lines) {
        int new_allocated = E.allocated_lines == 0 ? 16 : E.allocated_lines * 2;
        EditorLine *new_lines = realloc(E.lines, sizeof(EditorLine) * (size_t)new_allocated);
        if (new_lines == NULL) {
            set_status_message("Error: Failed to reallocate lines array.");
            return;
        }
        E.lines = new_lines;
        E.allocated_lines = new_allocated;
    }

    memmove(&E.lines[at + 1], &E.lines[at], sizeof(EditorLine) * (size_t)(E.num_lines - at));

    E.lines[at].size = len;
    E.lines[at].allocated = len + 1 < MAX_LINE_LENGTH_BUFFER ? MAX_LINE_LENGTH_BUFFER : len + 1;
    E.lines[at].chars = malloc((size_t)E.lines[at].allocated);
    E.lines[at].hl = NULL; // Will be allocated by update_highlighting
    E.lines[at].hl_revision = 0;
    if (E.lines[at].chars == NULL) {
        set_status_message("Error: Failed to allocate new line memory.");
        return;
    }
    memcpy(E.lines[at].chars, s, (size_t)len);
    E.lines[at].chars[len] = '\0';
    E.num_lines++;
    E.dirty = true;
    mark_lines_dirty(at, E.num_lines);
}

/**
 * @brief Deletes a line from the editor at the specified row index.
 *
 * @param at The row index of the line to delete.
 */
void editor_delete_line(int at) {
    if (at < 0 || at >= E.num_lines) return;

    free(E.lines[at].chars);
    free(E.lines[at].hl);
    memmove(&E.lines[at], &E.lines[at + 1], sizeof(EditorLine) * (size_t)(E.num_lines - at - 1));
    E.num_lines--;
    E.dirty = true;
    mark_lines_dirty(at, E.num_lines);
}

/**
 * @brief Inserts a character at the current cursor position.
 *
 * Handles regular characters and tab characters.
 *
 * @param c The character to insert.
 */
void editor_insert_char(int c) {
    UndoAction ua = { .type = UNDO_DELETE_CHAR, .y = E.cursor_y, .x = E.cursor_x, .char_val = (char)c, .text_content = NULL, .text_len = 0, .num_lines_affected = 0 };
    push_undo_action(ua);

    if (E.cursor_y == E.num_lines) {
        editor_insert_line(E.num_lines, "", 0);
    }
    editor_line_insert_char(&E.lines[E.cursor_y], E.cursor_x, c);
    E.cursor_x++;
    E.dirty = true;
    mark_lines_dirty(E.cursor_y, E.cursor_y);
}

/**
 * @brief Deletes a character at or before the current cursor position.
 *
 * Handles backspace (deletes char before cursor) and delete (deletes char at cursor).
 * Also handles joining lines if backspace is pressed at the beginning of a line.
 */
void editor_delete_char(void) {
    if (E.cursor_y == E.num_lines) return;
    if (E.cursor_x == 0 && E.cursor_y == 0) return;

    mark_lines_dirty(E.cursor_y, E.cursor_y);

    EditorLine *line = &E.lines[E.cursor_y];
    if (E.cursor_x > 0) {
        UndoAction ua = { .type = UNDO_INSERT_CHAR, .y = E.cursor_y, .x = E.cursor_x - 1, .char_val = line->chars[E.cursor_x - 1], .text_content = NULL, .text_len = 0, .num_lines_affected = 0 };
        push_undo_action(ua);

        editor_line_delete_char(line, E.cursor_x - 1);
        E.cursor_x--;
    } else {
        char *merged_line_content = strdup(line->chars);
        int prev_line_size = E.lines[E.cursor_y - 1].size;

        UndoAction ua = {
            .type = UNDO_JOIN_LINES,
            .y = E.cursor_y - 1,
            .x = prev_line_size,
            .char_val = '\0',
            .text_content = merged_line_content,
            .text_len = line->size,
            .num_lines_affected = 0
        };
        push_undo_action(ua);

        EditorLine *prev_line = &E.lines[E.cursor_y - 1];
        if ((size_t)prev_line->size + (size_t)line->size + 1 > (size_t)prev_line->allocated) {
            int new_allocated = prev_line->allocated + line->size + 1;
            prev_line->chars = realloc(prev_line->chars, (size_t)new_allocated);
            prev_line->hl = realloc(prev_line->hl, sizeof(HighlightType) * (size_t)new_allocated);
            if (prev_line->chars == NULL || prev_line->hl == NULL) {
                set_status_message("Error: Failed to reallocate line memory for join.");
                return;
            }
            prev_line->allocated = new_allocated;
        }
        memcpy(&prev_line->chars[prev_line->size], line->chars, (size_t)line->size + 1);
        prev_line->size += line->size;
        prev_line->hl_revision++;
        
        editor_delete_line(E.cursor_y);
        E.cursor_y--;
        mark_lines_dirty(E.cursor_y, E.cursor_y + 1);
    }
    E.dirty = true;
}

/**
 * @brief Inserts a new line at the current cursor position.
 *
 * Splits the current line into two if the cursor is not at the end of the line.
 */
void editor_insert_newline(void) {
    if (E.cursor_x == 0) {
        UndoAction ua = {
            .type = UNDO_INSERT_EMPTY_LINE,
            .y = E.cursor_y,
            .x = 0,
            .char_val = '\0',
            .text_content = NULL,
            .text_len = 0,
            .num_lines_affected = 0
        };
        push_undo_action(ua);
        editor_insert_line(E.cursor_y, "", 0);
    } else {
        char *split_off_content = strdup(&E.lines[E.cursor_y].chars[E.cursor_x]);
        int split_off_len = (int)strlen(split_off_content);

        UndoAction ua = {
            .type = UNDO_SPLIT_LINE,
            .y = E.cursor_y,
            .x = E.cursor_x,
            .char_val = '\0',
            .text_content = split_off_content,
            .text_len = split_off_len,
            .num_lines_affected = 0
        };
        push_undo_action(ua);

        EditorLine *line = &E.lines[E.cursor_y];
        editor_insert_line(E.cursor_y + 1, &line->chars[E.cursor_x], line->size - E.cursor_x);
        line = &E.lines[E.cursor_y];
        line->size = E.cursor_x;
        line->chars[line->size] = '\0';
        line->hl_revision++;
    }
    E.cursor_y++;
    E.cursor_x = 0;
    E.dirty = true;
    mark_lines_dirty(E.cursor_y -1, E.cursor_y);
}

/**
 * @brief Loads the content of a file into the editor's memory.
 *
 * @param filename The path to the file to load.
 */
void editor_load_file(const char *filename) {
    for (int i = 0; i < E.num_lines; i++) {
        free(E.lines[i].chars);
        free(E.lines[i].hl);
    }
    free(E.lines);
    E.lines = NULL;
    E.num_lines = 0;
    E.allocated_lines = 0;
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.scroll_x = 0;
    E.scroll_y = 0;

    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        set_status_message("Error: Could not open file %s: %s", filename, strerror(errno));
        editor_insert_line(0, "", 0);
        E.dirty = false;
        prompt_file_type();
        return;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        if (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }
        editor_insert_line(E.num_lines, line, (int)linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = false;
    set_status_message("File loaded: %s (%d lines)", E.filename, E.num_lines);
    mark_lines_dirty(0, E.num_lines - 1);
    prompt_file_type();

    add_to_recent_files(filename);
    init_undo_redo();
}

/**
 * @brief Saves the current editor content to the file.
 *
 * If no filename is set, it prompts the user for one.
 */
void editor_save_file(void) {
    if (E.filename == NULL) {
        // Автоматичний виклик "Save As" для нових файлів
        editor_save_as();
        // Якщо editor_save_as() скасовано або не вдалося, E.dirty залишиться true
        if (E.dirty) {
            set_status_message("Save cancelled or failed.");
        }
        return;
    } else {
        // Звичайне збереження
        FILE *fp = fopen(E.filename, "w");
        if (!fp) {
            set_status_message("Error saving: %s", strerror(errno));
            return;
        }
        
        for (int i = 0; i < E.num_lines; i++) {
            fputs(E.lines[i].chars, fp);
            fputc('\n', fp);
        }
        
        fclose(fp);
        E.dirty = false;
        set_status_message("Saved %s (%d lines)", E.filename, E.num_lines);
    }
}

/**
 * @brief Converts a character index (cx) within a line to a rendered column index (rx).
 *
 * Accounts for tab characters expanding to multiple spaces.
 *
 * @param row Pointer to the EditorLine.
 * @param cx The character index (0-indexed).
 * @return The rendered column index.
 */
int editor_row_cx_to_rx(const EditorLine *row, int cx) {
    int rx = 0;
    if (!row) return 0;
    for (int i = 0; i < cx; i++) {
        if (row->chars[i] == '\t') {
            rx += (TAB_STOP - (rx % TAB_STOP));
        } else {
            rx++;
        }
    }
    return rx;
}

/**
 * @brief Converts a rendered column index (rx) to a character index (cx) within a line.
 *
 * Accounts for tab characters. This is useful for placing the cursor accurately.
 *
 * @param row Pointer to the EditorLine.
 * @param rx The rendered column index (0-indexed).
 * @return The character index.
 */
int editor_row_rx_to_cx(const EditorLine *row, int rx) {
    int cur_rx = 0;
    int cx = 0;
    if (!row) return 0;
    for (cx = 0; cx < row->size; cx++) {
        if (row->chars[cx] == '\t') {
            cur_rx += (TAB_STOP - (cur_rx % TAB_STOP));
        } else {
            cur_rx++;
        }
        if (cur_rx > rx) return cx;
    }
    return cx;
}

// --- UI Function Implementations ---

/**
 * @brief Sets a message to be displayed in the status bar.
 *
 * The message will be displayed for a short period.
 *
 * @param fmt Format string for the message (like printf).
 * @param ... Variable arguments for the format string.
 */
void set_status_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_message, sizeof(E.status_message), fmt, ap);
    va_end(ap);
    E.status_message_time = time(NULL);
}

/**
 * @brief Checks if a given character is part of a common operator or delimiter set.
 *
 * @param c The character to check.
 * @return true if the character is an operator/delimiter, false otherwise.
 */
bool is_operator_char(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '=' || c == '<' || c == '>' || c == '!' || c == '&' ||
            c == '|' || c == '^' || c == '~' || c == '?' || c == ':' ||
            c == ';' || c == ',' || c == '.' || c == '(' || c == ')' ||
            c == '[' || c == ']' || c == '{' || c == '}');
}

/**
 * @brief Checks if the characters at the given index form a common two-character operator.
 *
 * @param str The string to check within.
 * @param index The starting index of the potential operator.
 * @return true if a two-character operator is found, false otherwise.
 */
bool is_double_operator(const char *str, int index) {
    if (str[index + 1] == '\0') return false;

    char c1 = str[index];
    char c2 = str[index + 1];

    if ((c1 == '=' && c2 == '=') || (c1 == '!' && c2 == '=') ||
        (c1 == '&' && c2 == '&') || (c1 == '|' && c2 == '|') ||
        (c1 == '+' && c2 == '+') || (c1 == '-' && c2 == '-') ||
        (c1 == '<' && c2 == '=') || (c1 == '>' && c2 == '=') ||
        (c1 == '<' && c2 == '<') || (c1 == '>' && c2 == '>') ||
        (c1 == '+' && c2 == '=') || (c1 == '-' && c2 == '=') ||
        (c1 == '*' && c2 == '=') || (c1 == '/' && c2 == '=') ||
        (c1 == '%' && c2 == '=') || (c1 == '&' && c2 == '=') ||
        (c1 == '|' && c2 == '=') || (c1 == '^' && c2 == '=') ||
        (c1 == '-' && c2 == '>')) {
        return true;
    }
    return false;
}

/**
 * @brief Marks a range of lines as dirty, requiring a redraw.
 *
 * @param start The starting line index (inclusive).
 * @param end The ending line index (inclusive).
 */
void mark_lines_dirty(int start, int end) {
    if (start < 0) start = 0;
    if (end >= E.num_lines) end = E.num_lines - 1;

    if (E.dirty_line_start == -1 || start < E.dirty_line_start) {
        E.dirty_line_start = start;
    }
    if (end > E.dirty_line_end) {
        E.dirty_line_end = end;
    }
}

/**
 * @brief Updates the highlighting information for a given line.
 *
 * This function performs a DFA-like analysis to determine the HighlightType
 * for each character in the line. It also manages the global multi-line
 * comment state.
 *
 * @param line Pointer to the EditorLine to update.
 */
void update_highlighting(EditorLine *line) {
    if (line->hl == NULL || (size_t)line->allocated < (size_t)line->size + 1) {
        if (line->hl) free(line->hl);
        line->hl = malloc(sizeof(HighlightType) * (size_t)(line->allocated));
        if (line->hl == NULL) {
            set_status_message("Error: Failed to allocate highlighting memory.");
            return;
        }
    }

    if (!E.is_code_file) {
        for (int i = 0; i < line->size; i++) {
            line->hl[i] = HL_NORMAL;
        }
        line->hl_revision++;
        E.in_multiline_comment_global = false;
        return;
    }

    for (int i = 0; i < line->size; i++) {
        line->hl[i] = HL_NORMAL;
    }

    bool in_multiline_comment_this_line = E.in_multiline_comment_global;
    bool in_string = false;
    char string_quote_char = '\0';
    bool first_word_on_line_highlighted = false;

    for (int i = 0; i < line->size; i++) {
        char c = line->chars[i];

        if (in_multiline_comment_this_line) {
            line->hl[i] = HL_COMMENT;
            if (c == '*' && i + 1 < line->size && line->chars[i+1] == '/') {
                line->hl[i+1] = HL_COMMENT;
                in_multiline_comment_this_line = false;
                i++;
            }
            continue;
        }

        if (in_string) {
            line->hl[i] = HL_STRING;
            if (c == '\\' && i + 1 < line->size) {
                line->hl[i+1] = HL_STRING;
                i++;
            } else if (c == string_quote_char) {
                in_string = false;
                string_quote_char = '\0';
            }
            continue;
        }

        if (c == '\'' || c == '"' || c == '`') {
            in_string = true;
            string_quote_char = c;
            line->hl[i] = HL_STRING;
        } else if (c == '/' && i + 1 < line->size && line->chars[i+1] == '/') {
            for (int k = i; k < line->size; k++) {
                line->hl[k] = HL_COMMENT;
            }
            break;
        } else if (c == '#') {
            for (int k = i; k < line->size; k++) {
                line->hl[k] = HL_COMMENT;
            }
            break;
        } else if (c == '/' && i + 1 < line->size && line->chars[i+1] == '*') {
            for (int k = i; k < line->size; k++) {
                line->hl[k] = HL_COMMENT;
            }
            in_multiline_comment_this_line = true;
            i++;
        } else if (isdigit(c)) {
            int start_num = i;
            while (i < line->size && (isdigit(line->chars[i]) || line->chars[i] == '.' ||
                                tolower((unsigned char)line->chars[i]) == 'x' ||
                                (i > start_num && tolower((unsigned char)line->chars[i-1]) == 'x' && isxdigit((unsigned char)line->chars[i])) ||
                                tolower((unsigned char)line->chars[i]) == 'e' || tolower((unsigned char)line->chars[i]) == 'f' ||
                                (i > start_num && (line->chars[i-1] == 'e' || line->chars[i-1] == 'E') && (line->chars[i] == '+' || line->chars[i] == '-'))
                                )) {
                line->hl[i] = HL_NUMBER;
                i++;
            }
            i--;
        } else if (is_double_operator(line->chars, i)) {
            line->hl[i] = HL_OPERATOR;
            line->hl[i+1] = HL_OPERATOR;
            i++;
        } else if (is_operator_char(c)) {
            line->hl[i] = HL_OPERATOR;
        } else if (isalnum((unsigned char)c) || c == '_') {
            int start = i;
            while (i < line->size && (isalnum((unsigned char)line->chars[i]) || line->chars[i] == '_')) {
                i++;
            }
            
            if (!first_word_on_line_highlighted) {
                int prev_char_idx = start - 1;
                bool only_whitespace_before = true;
                while (prev_char_idx >= 0) {
                    if (!isspace((unsigned char)line->chars[prev_char_idx])) {
                        only_whitespace_before = false;
                        break;
                    }
                    prev_char_idx--;
                }
                if (only_whitespace_before) {
                    for (int k = start; k < i; k++) {
                        line->hl[k] = HL_KEYWORD;
                    }
                    first_word_on_line_highlighted = true;
                }
            }
            
            for (int k = start; k < i; k++) {
                if (line->hl[k] == HL_NORMAL) {
                    line->hl[k] = HL_NORMAL;
                }
            }
            i--;
        }
    }
    line->hl_revision++;
    E.in_multiline_comment_global = in_multiline_comment_this_line;
}

/**
 * @brief Prints a character to the ncurses window with the specified color pair,
 * handling tab expansion and custom cursor highlighting.
 *
 * @param c The character to print.
 * @param base_color_pair The base color pair for syntax highlighting.
 * @param is_cursor_char True if this character is at the cursor position, false otherwise.
 */
static void print_char_with_highlight(int c, int base_color_pair, bool is_cursor_char) {
    int final_color_pair = base_color_pair;
    int attrs = A_NORMAL;

    int current_screen_y, current_screen_x;
    getyx(stdscr, current_screen_y, current_screen_x);

    int file_y = current_screen_y - BORDER_WIDTH + E.scroll_y;
    int file_x_rendered = current_screen_x - BORDER_WIDTH;
    int line_num_width = 0;
    if (E.show_line_numbers) {
        line_num_width = snprintf(NULL, 0, "%d", E.num_lines > 0 ? E.num_lines : 1) + 1;
        if (line_num_width < 4) line_num_width = 4;
        file_x_rendered -= line_num_width;
    }
    int file_x = editor_row_rx_to_cx((file_y >=0 && file_y < E.num_lines) ? &E.lines[file_y] : NULL, file_x_rendered + E.scroll_x);

    if (E.visual_mode && is_char_in_selection(file_y, file_x)) {
        final_color_pair = COLOR_PAIR_SELECTION;
        attrs = A_NORMAL;
    }

    if (is_cursor_char) {
        final_color_pair = COLOR_PAIR_CURSOR;
        attrs = A_REVERSE;
    }
    
    attron(COLOR_PAIR(final_color_pair) | attrs);

    if (c == '\t') {
        waddnstr(stdscr, "    ", TAB_STOP);
    } else {
        waddch(stdscr, (chtype)c);
    }

    attroff(COLOR_PAIR(final_color_pair) | attrs);
}


/**
 * @brief Maps a HighlightType to its corresponding ncurses color pair.
 *
 * @param type The HighlightType enum value.
 * @return The ncurses color pair ID.
 */
static int get_color_pair_for_highlight_type(HighlightType type) {
    switch (type) {
        case HL_COMMENT: return COLOR_PAIR_COMMENT;
        case HL_STRING: return COLOR_PAIR_STRING;
        case HL_NUMBER: return COLOR_PAIR_NUMBER;
        case HL_OPERATOR: return COLOR_PAIR_OPERATOR;
        case HL_KEYWORD: return COLOR_PAIR_KEYWORD;
        default: return COLOR_PAIR_DEFAULT;
    }
}

/**
 * @brief Draws a single line of text to the ncurses window with syntax highlighting.
 *
 * @param line Pointer to the EditorLine to highlight.
 * @param line_idx The 0-indexed index of the current line being drawn.
 * @param screen_y The screen row where this line should be drawn.
 * @param line_num_offset_x The horizontal offset for the actual line content due to line numbers.
 */
void editor_draw_line_highlighted(const EditorLine *line, int line_idx, int screen_y, int line_num_offset_x) {
    update_highlighting((EditorLine *)line);

    wmove(stdscr, screen_y, BORDER_WIDTH + line_num_offset_x);

    int chars_skipped = 0;
    int rendered_x_at_scroll = 0;
    for (int i = 0; i < line->size; i++) {
        int char_width = (line->chars[i] == '\t') ? (TAB_STOP - (rendered_x_at_scroll % TAB_STOP)) : 1;
        if (rendered_x_at_scroll + char_width > E.scroll_x) {
            break;
        }
        rendered_x_at_scroll += char_width;
        chars_skipped++;
    }

    int current_render_x = rendered_x_at_scroll - E.scroll_x;
    int editor_content_cols = E.screen_cols - 2 * BORDER_WIDTH;
    if (E.show_line_numbers) {
        editor_content_cols -= (snprintf(NULL, 0, "%d", E.num_lines > 0 ? E.num_lines : 1) + 1);
    }

    for (int i = chars_skipped; i < line->size; i++) {
        if (current_render_x >= editor_content_cols) {
            break;
        }

        bool is_cursor = (line_idx == E.cursor_y && i == E.cursor_x);
        int color_pair = get_color_pair_for_highlight_type(line->hl[i]);
        
        if (line->chars[i] == '\t') {
            int tab_width = TAB_STOP - (current_render_x % TAB_STOP);
            for (int k = 0; k < tab_width; k++) {
                if (current_render_x + k >= editor_content_cols) break;
                print_char_with_highlight(' ', color_pair, is_cursor && k == 0);
            }
            current_render_x += tab_width;
        } else {
            print_char_with_highlight(line->chars[i], color_pair, is_cursor);
            current_render_x++;
        }
    }

    if (line_idx == E.cursor_y && E.cursor_x == line->size && current_render_x < editor_content_cols) {
        print_char_with_highlight(' ', COLOR_PAIR_DEFAULT, true);
        current_render_x++;
    }

    for (int i = current_render_x; i < editor_content_cols; i++) {
        waddch(stdscr, ' ');
    }
}

/**
 * @brief Clears the suggestion area at the bottom of the screen.
 */
void clear_suggestion_area(void) {
    int start_y = E.screen_rows + BORDER_WIDTH;
    for (int y = 0; y < SUGGESTION_ROWS; y++) {
        mvhline(start_y + y, 0, ' ', (chtype)(E.screen_cols + 2 * BORDER_WIDTH));
    }
}

/**
 * @brief Displays command suggestions in a dedicated area.
 */
void show_command_suggestions(void) {
    clear_suggestion_area();
    int start_y = E.screen_rows + BORDER_WIDTH;
    wattron(stdscr, COLOR_PAIR(COLOR_PAIR_SUGGESTIONS));

    mvprintw(start_y, BORDER_WIDTH, "Suggestions:");
    
    char buffer[MAX_STATUS_MESSAGE_LENGTH];
    int current_col = BORDER_WIDTH;
    int current_row = start_y + 1;

    // Updated list of commands for suggestions
    const char *common_commands[] = {
        "S (Save)", "SA (Save As)", "F (Find)", "FN (Find Next)", "FP (Find Prev)",
        "R (Replace)", "G (Go to Line)", "LN (Line Numbers)",
        "DU (Duplicate Line)", "UL (Uppercase Line)", "LL (Lowercase Line)",
        "DL (Delete Line)", "QW (Quit Without Save)", "I (Info)", "R (Recent Files)",
        "KN (Normal KB Mode)", "TC (Text to Code)", "CT (Code to Text)",
        "Z (Undo)", "Y (Redo)", ":: (Create Macro)", "? (Help)",
        "h (Left)", "j (Down)", "k (Up)", "l (Right)" // Vim-like movements
    };
    int num_built_in = sizeof(common_commands) / sizeof(common_commands[0]);

    for (int i = 0; i < num_built_in; i++) {
        // Use strncasecmp for case-insensitive comparison
        if (E.cmd.length == 0 || strncasecmp(common_commands[i], E.cmd.sequence, (size_t)E.cmd.length) == 0) {
            int len = (int)strlen(common_commands[i]);
            if (current_col + len + 2 > E.screen_cols + BORDER_WIDTH) {
                current_row++;
                current_col = BORDER_WIDTH;
                if (current_row >= start_y + SUGGESTION_ROWS) break;
            }
            mvprintw(current_row, current_col, "%s  ", common_commands[i]);
            current_col += len + 2;
        }
    }

    for (int i = 0; i < E.macro_count; i++) {
        if (E.cmd.length == 0 || strncasecmp(E.macros[i].sequence, E.cmd.sequence, (size_t)E.cmd.length) == 0) {
            snprintf(buffer, sizeof(buffer), "%s ('%s')", E.macros[i].sequence, E.macros[i].action);
            int len = (int)strlen(buffer);
            if (current_col + len + 2 > E.screen_cols + BORDER_WIDTH) {
                current_row++;
                current_col = BORDER_WIDTH;
                if (current_row >= start_y + SUGGESTION_ROWS) break;
            }
            mvprintw(current_row, current_col, "%s  ", buffer);
            current_col += len + 2;
        }
    }

    wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_SUGGESTIONS));
}


/**
 * @brief Draws the UI hints panel at the bottom of the screen.
 */
void editor_draw_hints(void) {
    int start_y = E.total_screen_rows - HINT_ROWS - 1;
    wattron(stdscr, COLOR_PAIR(COLOR_PAIR_HINTS));

    for (int y = 0; y < HINT_ROWS; y++) {
        mvhline(start_y + y, 0, ' ', (chtype)(E.screen_cols + 2 * BORDER_WIDTH));
    }

    const char* hints_line1;
    const char* hints_line2;

    if (E.visual_mode) {
        hints_line1 = "^C Copy | ^X Cut | ^V Paste | ESC Cancel Selection";
        hints_line2 = "Visual Mode ON. Move cursor to select.";
    } else {
        // Updated hints for normal mode
        hints_line1 = "^S Save | ^O Open | ^F Find | ^\\ Cmd | ^Q Quit | ^H Help"; // Changed ^J to ^\
        hints_line2 = "^C Copy | ^X Cut | ^P Paste | ^Z Undo | ^Y Redo | ^A Select All";
    }
    
    mvprintw(start_y, BORDER_WIDTH, "%s", hints_line1);
    mvprintw(start_y + 1, BORDER_WIDTH, "%s", hints_line2);
    
    wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_HINTS));
}


/**
 * @brief Draws the main editor content to the screen.
 *
 * Clears the screen, draws visible lines with highlighting, and then draws
 * the status bar. Optimizes by redrawing only dirty lines.
 */
void editor_refresh_screen(void) {
    getmaxyx(stdscr, E.total_screen_rows, E.screen_cols);
    E.screen_rows = E.total_screen_rows - HINT_ROWS - 1 - SUGGESTION_ROWS - 2 * BORDER_WIDTH;
    
    int line_num_width = 0;
    if (E.show_line_numbers) {
        line_num_width = snprintf(NULL, 0, "%d", E.num_lines > 0 ? E.num_lines : 1) + 1;
        if (line_num_width < 4) line_num_width = 4;
    }
    int editor_content_cols = E.screen_cols - 2 * BORDER_WIDTH - line_num_width;


    if (E.cursor_y < E.scroll_y) {
        E.scroll_y = E.cursor_y;
    }
    if (E.cursor_y >= E.scroll_y + E.screen_rows) {
        E.scroll_y = E.cursor_y - E.screen_rows + 1;
    }

    int rx = 0;
    if (E.cursor_y < E.num_lines) {
        rx = editor_row_cx_to_rx(&E.lines[E.cursor_y], E.cursor_x);
    }
    if (rx < E.scroll_x) {
        E.scroll_x = rx;
    }
    if (rx >= E.scroll_x + editor_content_cols) {
        E.scroll_x = rx - editor_content_cols + 1;
    }

    wattron(stdscr, COLOR_PAIR(COLOR_PAIR_BORDER));
    box(stdscr, 0, 0);
    wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_BORDER));


    E.in_multiline_comment_global = false;
    for (int i = 0; i < E.scroll_y; i++) {
        if (i < E.num_lines) {
             update_highlighting(&E.lines[i]);
        }
    }

    for (int y = 0; y < E.screen_rows; y++) {
        int file_line_idx = y + E.scroll_y;
        int screen_y = y + BORDER_WIDTH;

        mvhline(screen_y, BORDER_WIDTH, ' ', (chtype)(E.screen_cols - 2 * BORDER_WIDTH));

        if (file_line_idx < E.num_lines) {
            const EditorLine *current_line = &E.lines[file_line_idx];

            if (E.show_line_numbers) {
                wattron(stdscr, COLOR_PAIR(COLOR_PAIR_DEFAULT));
                mvprintw(screen_y, BORDER_WIDTH, "%*d ", line_num_width - 1, file_line_idx + 1);
                wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_DEFAULT));
            }

            editor_draw_line_highlighted(current_line, file_line_idx, screen_y, line_num_width);
        } else {
            wattron(stdscr, COLOR_PAIR(COLOR_PAIR_DEFAULT));
            mvaddch(screen_y, BORDER_WIDTH + line_num_width, '~');
            wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_DEFAULT));
        }
    }

    E.dirty_line_start = -1;
    E.dirty_line_end = -1;

    if (E.cmd.active && !E.cmd.show_help) {
        show_command_suggestions();
    } else {
        clear_suggestion_area();
    }

    editor_draw_hints();

    int status_bar_y = E.total_screen_rows - 1;
    wattron(stdscr, COLOR_PAIR(COLOR_PAIR_STATUS_BAR));
    mvhline(status_bar_y, 0, ' ', (chtype)(E.screen_cols + 2 * BORDER_WIDTH));
    
    if (E.cmd.active) {
        if (E.cmd.show_help) {
            // Help screen is handled separately
        } else if (E.creative_mode) {
            mvprintw(status_bar_y, BORDER_WIDTH, "Creative Mode: Enter action for '%s'", E.cmd.sequence);
        } else {
            // Updated status message for command mode
            mvprintw(status_bar_y, BORDER_WIDTH, "Command Mode: %s (Tab: suggestions, Esc: cancel)", E.cmd.sequence);
        }
    } else {
        // Improved status message for new files
        if (E.filename == NULL && !E.dirty) {
            mvprintw(status_bar_y, BORDER_WIDTH, "NEW FILE - Press Ctrl+S to save. Ctrl+H for help.");
        } else {
            mvprintw(status_bar_y, BORDER_WIDTH, "%.20s | %s %s", 
                    E.filename ? E.filename : "[New]",
                    E.dirty ? "***" : "",
                    E.is_code_file ? "</>" : "TXT");
        }
    }

    char msg[MAX_STATUS_MESSAGE_LENGTH];
    if (time(NULL) - E.status_message_time < 5) {
        snprintf(msg, sizeof(msg), "%s", E.status_message);
    } else {
        msg[0] = '\0';
    }
    mvprintw(status_bar_y, E.screen_cols + 2 * BORDER_WIDTH - (int)strlen(msg) - BORDER_WIDTH, "%s", msg);
    wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_STATUS_BAR));

    int cursor_screen_y = E.cursor_y - E.scroll_y + BORDER_WIDTH;
    int cursor_screen_x = editor_row_cx_to_rx(
        (E.cursor_y < E.num_lines ? &E.lines[E.cursor_y] : NULL), E.cursor_x) - E.scroll_x + BORDER_WIDTH;
    
    if (E.show_line_numbers) {
        cursor_screen_x += line_num_width;
    }

    wmove(stdscr, cursor_screen_y, cursor_screen_x);

    if (E.cmd.active && E.cmd.show_help) {
        show_command_help_screen();
    }

    doupdate();
}

/**
 * @brief Prompts the user for input in the status bar.
 *
 * @param prompt_msg The message to display as a prompt.
 * @param buffer The buffer to store user input.
 * @param buf_size The size of the buffer.
 * @return The buffer with user input, or NULL if cancelled.
 */
char *editor_prompt(const char *prompt_msg, char *buffer, size_t buf_size) {
    buffer[0] = '\0';
    int i = 0;
    bool was_cmd_active = E.cmd.active;
    bool was_creative_mode = E.creative_mode;
    reset_command_mode();

    while (true) {
        set_status_message(prompt_msg, buffer);
        editor_refresh_screen();
        int c = editor_read_key();
        if (c == KEY_ENTER || c == '\n') {
            if (i > 0) {
                if (was_cmd_active) E.cmd.active = true;
                if (was_creative_mode) E.creative_mode = true;
                return buffer;
            } else {
                set_status_message("Input cannot be empty!");
            }
        } else if (c == 27) {
            set_status_message("Cancelled.");
            if (was_cmd_active) E.cmd.active = true;
            if (was_creative_mode) E.creative_mode = true;
            return NULL;
        } else if (c == KEY_BACKSPACE || c == 127) {
            if (i > 0) {
                buffer[--i] = '\0';
            }
        } else if (isprint(c) && (size_t)i < buf_size - 1) {
            buffer[i++] = (char)c;
            buffer[i] = '\0';
        }
    }
}

/**
 * @brief Shows a confirmation dialog to the user.
 * @param prompt_msg The message to display.
 * @return true if confirmed (Y/y), false otherwise (N/n or ESC).
 */
bool show_confirmation_dialog(const char *prompt_msg) {
    char choice_buf[10];
    char full_prompt[MAX_STATUS_MESSAGE_LENGTH];
    snprintf(full_prompt, sizeof(full_prompt), "%s (Y/N): %%s", prompt_msg);
    char *result = editor_prompt(full_prompt, choice_buf, sizeof(choice_buf));
    return (result != NULL && (tolower((unsigned char)result[0]) == 'y'));
}

/**
 * @brief Displays a simple ASCII loading screen for the editor.
 */
void display_loading_screen(void) {
    clear();
    attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));

    const char *ascii_art[] = {
        " _   _ _   _ ___ _____ _____ ____  ",
        "| | | | | | |_ _| ____|_   _|  _ \\ ",
        "| | | | | | || ||  _|   | | | | | |",
        "| |_| | |_| || || |___  | | | |_| |",
        " \\___/ \\___/|___|_____| |_| |____/ "
    };
    int num_lines = sizeof(ascii_art) / sizeof(ascii_art[0]);

    int center_y = LINES / 2 - num_lines / 2;
    int center_x = COLS / 2 - (int)strlen(ascii_art[0]) / 2;

    for (int i = 0; i < num_lines; i++) {
        mvprintw(center_y + i, center_x, "%s", ascii_art[i]);
        refresh();
        usleep(100000);
    }

    mvprintw(center_y + num_lines + 2, center_x, "Loading Unied Editor by Ferki...");
    refresh();
    usleep(1000000);
    clear();
}

/**
 * @brief Sets whether the file should be treated as code or plain text for highlighting.
 *
 * @param is_code True for code, false for plain text.
 */
void editor_set_file_type(bool is_code) {
    E.is_code_file = is_code;
    mark_lines_dirty(0, E.num_lines - 1);
}

/**
 * @brief Prompts the user to choose between code or plain text file type.
 */
void prompt_file_type(void) {
    char choice_buf[MAX_STATUS_MESSAGE_LENGTH];
    char *result = editor_prompt("Is this a code file (C/Python/JS etc.) or plain text? (C/T): %s", choice_buf, sizeof(choice_buf));
    
    if (result != NULL && (tolower((unsigned char)result[0]) == 'c')) {
        editor_set_file_type(true);
        set_status_message("File type set to: Code.");
    } else {
        editor_set_file_type(false);
        set_status_message("File type set to: Text.");
    }
}

/**
 * @brief Displays a full-screen help message.
 */
void show_command_help_screen(void) {
    clear();
    wattron(stdscr, COLOR_PAIR(COLOR_PAIR_HINTS));
    
    int row = 1;
    int col = 2;

    mvprintw(row++, col, "UNIED Editor Help");
    mvprintw(row++, col, "-----------------");
    row++;

    mvprintw(row++, col, "Basic Navigation:");
    mvprintw(row++, col, "  Arrow Keys: Move cursor");
    mvprintw(row++, col, "  Home/End: Move to start/end of line");
    mvprintw(row++, col, "  Ctrl+Left/Right: Move by word");
    mvprintw(row++, col, "  Ctrl+H: Move to start of file");
    mvprintw(row++, col, "  Ctrl+E: Move to end of file");
    mvprintw(row++, col, "  PgUp/PgDn: Scroll page up/down");
    row++;

    mvprintw(row++, col, "Editing:");
    mvprintw(row++, col, "  Enter: New line");
    mvprintw(row++, col, "  Backspace/Delete: Delete character");
    row++;

    mvprintw(row++, col, "Quick Commands (Ctrl+Key):");
    mvprintw(row++, col, "  Ctrl+S: Save current file");
    mvprintw(row++, col, "  Ctrl+O: Open file");
    mvprintw(row++, col, "  Ctrl+Q: Quit (with confirmation)");
    mvprintw(row++, col, "  Ctrl+F: Find text");
    mvprintw(row++, col, "  Ctrl+G: Go to Line Number");
    mvprintw(row++, col, "  Ctrl+A: Select All");
    mvprintw(row++, col, "  Ctrl+V: Toggle Visual (Selection) Mode");
    mvprintw(row++, col, "  Ctrl+C: Copy selected text/current line");
    mvprintw(row++, col, "  Ctrl+X: Cut selected text/current line");
    mvprintw(row++, col, "  Ctrl+P: Paste text");
    mvprintw(row++, col, "  Ctrl+Z: Undo last action");
    mvprintw(row++, col, "  Ctrl+Y: Redo last undone action");
    mvprintw(row++, col, "  Ctrl+H: Show this Help screen");
    row++;

    mvprintw(row++, col, "Command Mode (Ctrl+\\ + sequence):"); // Changed activation key
    mvprintw(row++, col, "  Enter Ctrl+\\ to activate command mode, then type sequence.");
    mvprintw(row++, col, "  ESC: Exit command mode");
    mvprintw(row++, col, "  Tab: Show command suggestions / Autocomplete");
    row++;

    mvprintw(row++, col, "  S: Save current file");
    mvprintw(row++, col, "  SA: Save As (new file)");
    mvprintw(row++, col, "  F: Find (start search)");
    mvprintw(row++, col, "  FN: Find Next occurrence");
    mvprintw(row++, col, "  FP: Find Previous occurrence");
    mvprintw(row++, col, "  R: Find & Replace");
    mvprintw(row++, col, "  I: Show File Info");
    mvprintw(row++, col, "  DU: Duplicate Current Line");
    mvprintw(row++, col, "  DL: Delete Current Line");
    mvprintw(row++, col, "  UL: Uppercase Current Line");
    mvprintw(row++, col, "  LL: Lowercase Current Line");
    mvprintw(row++, col, "  LN: Toggle Line Numbers");
    mvprintw(row++, col, "  R: Show Recently Opened Files");
    mvprintw(row++, col, "  QW: Quit Without Save (force)");
    mvprintw(row++, col, "  KN: Set Keyboard Mode Normal (WASD inserts)");
    mvprintw(row++, col, "  TC: Set File Type to Code");
    mvprintw(row++, col, "  CT: Set File Type to Text");
    mvprintw(row++, col, "  h/j/k/l: Move Left/Down/Up/Right (Vim-like)");
    row++;

    mvprintw(row++, col, "Custom Macros:");
    mvprintw(row++, col, "  Ctrl+\\ :: (then type sequence): Enter Creative Mode to define a macro.");
    mvprintw(row++, col, "  Example: Type Ctrl+\\, then 'Q', then '::', then 'quit_confirm'.");
    mvprintw(row++, col, "  Now 'Ctrl+\\ Q' will prompt to quit with confirmation.");
    mvprintw(row++, col, "  Available actions for macros: 'upper', 'lower', 'duplicate', 'quit_confirm', 'save_file'.");
    row++;

    mvprintw(row++, col, "Press any key to return to editor...");
    wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_HINTS));
    wrefresh(stdscr);
    getch();
    clear();
    E.cmd.show_help = false;
}

// --- Command and Extended Function Implementations ---

/**
 * @brief Resets the command puzzle mode.
 */
void reset_command_mode(void) {
    E.cmd.active = false;
    E.cmd.length = 0;
    E.cmd.sequence[0] = '\0';
    E.cmd.last_key_time = 0;
    E.cmd.show_help = false;
    E.creative_mode = false;
    set_status_message("");
}

/**
 * @brief Executes a custom action string for a macro.
 * This function maps action strings to editor functions.
 *
 * @param action The string representing the action to perform.
 */
void execute_custom_command(const char* action) {
    if (strcmp(action, "upper") == 0) {
        editor_change_line_case(true);
    } else if (strcmp(action, "lower") == 0) {
        editor_change_line_case(false);
    } else if (strcmp(action, "duplicate") == 0) {
        editor_duplicate_line();
    } else if (strcmp(action, "quit_confirm") == 0) {
        editor_quit(false);
    } else if (strcmp(action, "save_file") == 0) {
        editor_save_file();
    }
    else {
        set_status_message("Macro action '%s' executed (placeholder).", action);
    }
}

/**
 * @brief Executes the command sequence currently stored in E.cmd.sequence.
 * This is the core logic of the "Command Puzzle System".
 */
void execute_command_sequence(void) {
    char *seq = E.cmd.sequence;
    
    if (E.cmd.length == 0) {
        set_status_message("Commands: S=Save, QW=QuitWithoutSave, SA=SaveAs, F=Find, ... (Tab:list)");
        return;
    }

    if (strcmp(seq, "::") == 0) {
        enter_creative_mode();
        return;
    } else if (strcmp(seq, "?") == 0) {
        E.cmd.show_help = !E.cmd.show_help;
        return;
    } else if (strcasecmp(seq, "KN") == 0) {
        editor_set_keyboard_mode(NORMAL_KB_MODE);
        set_status_message("Keyboard Mode: Normal.");
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "TC") == 0) {
        editor_set_file_type(true);
        set_status_message("File type changed to: Code.");
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "CT") == 0) {
        editor_set_file_type(false);
        set_status_message("File type changed to: Text.");
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "h") == 0) {
        editor_move_cursor(KEY_LEFT);
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "j") == 0) {
        editor_move_cursor(KEY_DOWN);
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "k") == 0) {
        editor_move_cursor(KEY_UP);
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "l") == 0) {
        editor_move_cursor(KEY_RIGHT);
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "I") == 0) {
        editor_show_file_info();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "FN") == 0) {
        editor_find_next();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "FP") == 0) {
        editor_find_prev();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "DU") == 0) {
        editor_duplicate_line();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "DL") == 0) { // New: Delete Line
        if (E.num_lines > 1) {
            char *deleted_content = strdup(E.lines[E.cursor_y].chars);
            UndoAction ua = {
                .type = UNDO_DELETE_BLOCK,
                .y = E.cursor_y,
                .x = 0,
                .char_val = '\0',
                .text_content = deleted_content,
                .text_len = (int)strlen(deleted_content),
                .num_lines_affected = 1
            };
            push_undo_action(ua);
            editor_delete_line(E.cursor_y);
            if (E.cursor_y >= E.num_lines && E.num_lines > 0) {
                E.cursor_y = E.num_lines - 1;
                E.cursor_x = E.lines[E.cursor_y].size;
            } else if (E.num_lines == 0) {
                editor_insert_line(0, "", 0); // Ensure at least one line
                E.cursor_y = 0;
                E.cursor_x = 0;
            }
            set_status_message("Line deleted.");
        } else {
            set_status_message("Cannot delete the last line.");
        }
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "UL") == 0) {
        editor_change_line_case(true);
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "LL") == 0) {
        editor_change_line_case(false);
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "LN") == 0) {
        editor_toggle_line_numbers();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "R") == 0) {
        editor_show_recent_files();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "Z") == 0) {
        editor_undo();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "Y") == 0) {
        editor_redo();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "S") == 0) { // Added simple 'S' command for Save
        editor_save_file();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "SA") == 0) { // Save As
        editor_save_as();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "F") == 0) { // Find (initiate search)
        editor_find();
        reset_command_mode();
        return;
    } else if (strcasecmp(seq, "R") == 0) { // Replace
        editor_find_replace();
        reset_command_mode();
        return;
    }


    for (int i = 0; i < E.macro_count; i++) {
        if (strcmp(seq, E.macros[i].sequence) == 0) {
            execute_custom_command(E.macros[i].action);
            reset_command_mode();
            return;
        }
    }

    if (strcasecmp(seq, "QW") == 0) {
        editor_quit(true);
    }
    else {
        set_status_message("Unknown command: Ctrl+\\ %s. Press ':' to save as macro.", seq);
    }
}

/**
 * @brief Handles input when in command puzzle mode.
 *
 * @param key The key pressed.
 */
void handle_command_mode_input(int key) {
    if (time(NULL) - E.cmd.last_key_time > (COMMAND_TIMEOUT_MS / 1000.0)) {
        set_status_message("Command timeout.");
        reset_command_mode();
        return;
    }

    if (key == ':' && E.cmd.length > 0) {
        enter_creative_mode();
        return;
    }

    if (key == '\t') {
        autocomplete_command(); // Call autocomplete on Tab
        E.cmd.last_key_time = time(NULL);
        return;
    }

    if (isprint(key) && E.cmd.length < MAX_COMMAND_SEQUENCE_LENGTH - 1) {
        E.cmd.sequence[E.cmd.length++] = (char)key;
        E.cmd.sequence[E.cmd.length] = '\0';
        E.cmd.last_key_time = time(NULL);
        // Do not execute_command_sequence here, only after full command or Enter
        // This allows multi-character commands to be built
    } else if (key == KEY_ENTER || key == '\n') {
        execute_command_sequence(); // Execute on Enter
        reset_command_mode(); // Reset after execution
    }
    else if (key == KEY_BACKSPACE || key == 127) { // Handle backspace in command mode
        if (E.cmd.length > 0) {
            E.cmd.length--;
            E.cmd.sequence[E.cmd.length] = '\0';
            E.cmd.last_key_time = time(NULL);
            set_status_message("Command Mode: %s (Tab: suggestions, Esc: cancel)", E.cmd.sequence);
        } else {
            reset_command_mode(); // Exit if backspace on empty command
        }
    }
    else if (key == 27) {
        set_status_message("Command mode cancelled.");
        reset_command_mode();
    } else {
        set_status_message("Command Mode: %s (Invalid key or sequence too long)", E.cmd.sequence);
        E.cmd.last_key_time = time(NULL);
    }
}

/**
 * @brief Autocompletes the current command sequence.
 */
void autocomplete_command(void) {
    const char* commands[] = {
        "S", "SA", "F", "FN", "FP", "R", "G", "LN",
        "DU", "UL", "LL", "DL", "QW", "I", "R",
        "KN", "TC", "CT", "Z", "Y",
        "h", "j", "k", "l", // Add Vim-like movements to autocomplete
        NULL
    };
    
    for (int i = 0; commands[i]; i++) {
        // Use strncasecmp for case-insensitive comparison
        if (strncasecmp(commands[i], E.cmd.sequence, (size_t)E.cmd.length) == 0) {
            strcpy(E.cmd.sequence, commands[i]);
            E.cmd.length = (int)strlen(commands[i]);
            set_status_message("Command Mode: %s (Tab: suggestions, Esc: cancel)", E.cmd.sequence);
            return;
        }
    }
    set_status_message("No autocomplete match for: %s", E.cmd.sequence);
}

/**
 * @brief Quits the editor.
 *
 * @param force_quit If true, quits without saving or confirmation.
 */
void editor_quit(bool force_quit) {
    if (E.dirty) {
        if (show_confirmation_dialog("Save before quit?")) {
            editor_save_file();
            // If the file is still dirty after attempting to save (e.g., user cancelled save-as prompt)
            if (E.dirty && !force_quit) {
                set_status_message("Quit cancelled. File not saved.");
                return;
            }
        } else if (!force_quit) { // User chose NOT to save, and it's not a forced quit
            if (!show_confirmation_dialog("Discard unsaved changes and quit?")) {
                set_status_message("Quit cancelled.");
                return;
            }
        }
    }
    deinit_editor();
    exit(0);
}

/**
 * @brief Saves the current editor content to a new file, prompting for filename.
 */
void editor_save_as(void) {
    char filename_buf[MAX_STATUS_MESSAGE_LENGTH];
    // Pre-fill filename_buf with current filename or empty string
    snprintf(filename_buf, sizeof(filename_buf), "%s", E.filename ? E.filename : "");
    
    if (editor_prompt("Save as: %s", filename_buf, sizeof(filename_buf))) {
        // Free old filename if it exists
        if (E.filename) {
            free(E.filename);
        }
        E.filename = strdup(filename_buf);
        if (E.filename == NULL) {
            set_status_message("Error: Failed to allocate memory for filename.");
            return;
        }
        editor_save_file();  // Use standard save function for actual writing
    } else {
        set_status_message("Save As cancelled.");
    }
}

/**
 * @brief Prompts for a filename and loads it into the editor.
 */
void editor_open_file(void) {
    char filename_buf[MAX_STATUS_MESSAGE_LENGTH];
    char* result = editor_prompt("Open file: %s", filename_buf, sizeof(filename_buf));
    if (result) {
        editor_load_file(filename_buf);
    } else {
        set_status_message("Open file cancelled.");
    }
}

/**
 * @brief Implements basic search functionality.
 * Prompts user for a search query and moves cursor to the first match.
 */
void editor_find(void) {
    char query[MAX_STATUS_MESSAGE_LENGTH];
    char *result = editor_prompt("Search: %s", query, sizeof(query));

    if (result == NULL) {
        E.search_active = false;
        return;
    }

    strncpy(E.last_search_query, query, sizeof(E.last_search_query) - 1);
    E.last_search_query[sizeof(E.last_search_query) - 1] = '\0';
    E.search_active = true;

    E.last_search_found_y = E.cursor_y;
    E.last_search_found_x = E.cursor_x;

    editor_find_next();
}

/**
 * @brief Finds the next occurrence of the last search query.
 */
void editor_find_next(void) {
    if (!E.search_active || E.last_search_query[0] == '\0') {
        set_status_message("No active search. Use Ctrl+F to start a new search.");
        return;
    }

    int original_cx = E.cursor_x;
    int original_cy = E.cursor_y;

    int start_y = E.last_search_found_y;
    int start_x = E.last_search_found_x + 1;

    for (int y = start_y; y < E.num_lines; y++) {
        int current_line_start_x = (y == start_y) ? start_x : 0;
        
        char *match = strstr(E.lines[y].chars + current_line_start_x, E.last_search_query);
        if (match) {
            E.cursor_y = y;
            E.cursor_x = (int)(match - E.lines[y].chars);
            E.last_search_found_y = E.cursor_y;
            E.last_search_found_x = E.cursor_x;
            set_status_message("Found '%s'", E.last_search_query);
            mark_lines_dirty(original_cy, original_cy);
            mark_lines_dirty(E.cursor_y, E.cursor_y);
            return;
        }
    }

    for (int y = 0; y <= start_y; y++) {
        int current_line_end_x = (y == start_y) ? start_x : E.lines[y].size;
        char *match = strstr(E.lines[y].chars, E.last_search_query);
        if (match && (y < start_y || (y == start_y && (match - E.lines[y].chars) < current_line_end_x) )) {
            E.cursor_y = y;
            E.cursor_x = (int)(match - E.lines[y].chars);
            E.last_search_found_y = E.cursor_y;
            E.last_search_found_x = E.cursor_x;
            set_status_message("Found '%s' (wrapped from beginning)", E.last_search_query);
            mark_lines_dirty(original_cy, original_cy);
            mark_lines_dirty(E.cursor_y, E.cursor_y);
            return;
        }
    }

    set_status_message("'%s' not found.", E.last_search_query);
    E.cursor_x = original_cx;
    E.cursor_y = original_cy;
    mark_lines_dirty(original_cy, original_cy);
    E.search_active = false;
}

/**
 * @brief Finds the previous occurrence of the last search query.
 */
void editor_find_prev(void) {
    if (!E.search_active || E.last_search_query[0] == '\0') {
        set_status_message("No active search. Use Ctrl+F to start a new search.");
        return;
    }

    int original_cx = E.cursor_x;
    int original_cy = E.cursor_y;

    int start_y = E.last_search_found_y;
    int start_x = E.last_search_found_x - 1;

    for (int y = start_y; y >= 0; y--) {
        int current_line_end_x = (y == start_y) ? start_x : E.lines[y].size - 1;
        
        for (int x = current_line_end_x; x >= 0; x--) {
            if ((size_t)x + strlen(E.last_search_query) <= (size_t)E.lines[y].size) {
                if (strncmp(E.lines[y].chars + x, E.last_search_query, strlen(E.last_search_query)) == 0) {
                    E.cursor_y = y;
                    E.cursor_x = x;
                    E.last_search_found_y = E.cursor_y;
                    E.last_search_found_x = E.cursor_x;
                    set_status_message("Found '%s'", E.last_search_query);
                    mark_lines_dirty(original_cy, original_cy);
                    mark_lines_dirty(E.cursor_y, E.cursor_y);
                    return;
                }
            }
        }
    }

    for (int y = E.num_lines - 1; y >= start_y; y--) {
        int current_line_start_x = (y == start_y) ? start_x : 0;
        for (int x = E.lines[y].size - (int)strlen(E.last_search_query); x >= current_line_start_x; x--) {
            if (x >= 0 && strncmp(E.lines[y].chars + x, E.last_search_query, strlen(E.last_search_query)) == 0) {
                E.cursor_y = y;
                E.cursor_x = x;
                E.last_search_found_y = E.cursor_y;
                E.last_search_found_x = E.cursor_x;
                set_status_message("Found '%s' (wrapped from end)", E.last_search_query);
                mark_lines_dirty(original_cy, original_cy);
                mark_lines_dirty(E.cursor_y, E.cursor_y);
                return;
            }
        }
    }

    set_status_message("'%s' not found.", E.last_search_query);
    E.cursor_x = original_cx;
    E.cursor_y = original_cy;
    mark_lines_dirty(original_cy, original_cy);
    E.search_active = false;
}

/**
 * @brief Implements Find and Replace functionality.
 */
void editor_find_replace(void) {
    char find_buf[MAX_STATUS_MESSAGE_LENGTH];
    char replace_buf[MAX_STATUS_MESSAGE_LENGTH];
    
    char* find = editor_prompt("Find: %s", find_buf, sizeof(find_buf));
    if (!find) {
        set_status_message("Find & Replace cancelled.");
        return;
    }
    
    char* replace = editor_prompt("Replace with: %s", replace_buf, sizeof(replace_buf));
    if (!replace) {
        set_status_message("Find & Replace cancelled.");
        return;
    }

    int find_len = (int)strlen(find);
    int replace_len = (int)strlen(replace);
    int occurrences = 0;

    for (int y = 0; y < E.num_lines; y++) {
        char *line_chars = E.lines[y].chars;
        int line_size = E.lines[y].size;
        char *current_pos = line_chars;

        while ((current_pos = strstr(current_pos, find)) != NULL) {
            int x_pos = (int)(current_pos - line_chars);

            // Record undo action for the modification
            // This is a simplified undo for replace. A more robust solution
            // would involve storing the exact change (delete old, insert new)
            // as a block. For now, we'll just record the line modification.
            char *original_line_content = strdup(E.lines[y].chars);
            UndoAction ua = {
                .type = UNDO_MODIFY_LINE_CASE, // Reusing this type, but it means "line content changed"
                .y = y,
                .x = 0, // Not precise for replace, but indicates line change
                .char_val = '\0',
                .text_content = original_line_content,
                .text_len = E.lines[y].size,
                .num_lines_affected = 1
            };
            push_undo_action(ua);


            // Calculate new line size
            int new_line_size = line_size - find_len + replace_len;
            if (new_line_size + 1 > E.lines[y].allocated) {
                int new_allocated = new_line_size + 1 + MAX_LINE_LENGTH_BUFFER; // Add some buffer
                E.lines[y].chars = realloc(E.lines[y].chars, (size_t)new_allocated);
                E.lines[y].hl = realloc(E.lines[y].hl, sizeof(HighlightType) * (size_t)new_allocated);
                if (E.lines[y].chars == NULL || E.lines[y].hl == NULL) {
                    set_status_message("Error: Failed to reallocate line memory for replace.");
                    free(original_line_content);
                    return;
                }
                E.lines[y].allocated = new_allocated;
                line_chars = E.lines[y].chars; // Update pointer after realloc
            }

            // Shift characters after the found string
            memmove(line_chars + x_pos + replace_len,
                    line_chars + x_pos + find_len,
                    (size_t)(line_size - (x_pos + find_len) + 1)); // +1 for null terminator

            // Copy replacement string
            memcpy(line_chars + x_pos, replace, (size_t)replace_len);

            E.lines[y].size = new_line_size;
            E.lines[y].chars[new_line_size] = '\0';
            E.lines[y].hl_revision++;
            E.dirty = true;
            mark_lines_dirty(y, y);

            occurrences++;
            current_pos = line_chars + x_pos + replace_len; // Continue search after replacement
            line_size = E.lines[y].size; // Update line size for next iteration
        }
    }
    set_status_message("Replaced %d occurrences.", occurrences);
}


/**
 * @brief Copies the current line to the clipboard.
 */
void editor_copy_line(void) {
    if (E.cursor_y >= E.num_lines) {
        set_status_message("Nothing to copy.");
        return;
    }
    EditorLine *line = &E.lines[E.cursor_y];
    if ((size_t)line->size + 1 > (size_t)E.clipboard_allocated) {
        E.clipboard_allocated = line->size + 1;
        E.clipboard_buffer = realloc(E.clipboard_buffer, (size_t)E.clipboard_allocated);
        if (E.clipboard_buffer == NULL) {
            set_status_message("Error: Failed to allocate clipboard memory.");
            return;
        }
    }
    memcpy(E.clipboard_buffer, line->chars, (size_t)line->size);
    E.clipboard_buffer[line->size] = '\0';
    E.clipboard_size = line->size;
    set_status_message("Line copied.");
}

/**
 * @brief Cuts the current line to the clipboard.
 */
void editor_cut_line(void) {
    if (E.cursor_y >= E.num_lines) {
        set_status_message("Nothing to cut.");
        return;
    }
    char *deleted_content = strdup(E.lines[E.cursor_y].chars);
    UndoAction ua = {
        .type = UNDO_DELETE_BLOCK,
        .y = E.cursor_y,
        .x = 0,
        .char_val = '\0',
        .text_content = deleted_content,
        .text_len = (int)strlen(deleted_content),
        .num_lines_affected = 1
    };
    push_undo_action(ua);

    editor_copy_line();
    editor_delete_line(E.cursor_y);
    if (E.num_lines == 0) {
        E.cursor_y = 0;
        E.cursor_x = 0;
        editor_insert_line(0, "", 0);
    } else if (E.cursor_y >= E.num_lines) {
        E.cursor_y = E.num_lines - 1;
        E.cursor_x = E.lines[E.cursor_y].size;
    }
    set_status_message("Line cut.");
}

/**
 * @brief Inserts a block of text starting at (y,x). Handles newlines within text.
 * @param y The starting line index.
 * @param x The starting character index.
 * @param text The text block to insert.
 * @param text_len The length of the text block.
 */
void editor_insert_text_block(int y, int x, const char *text, int text_len) {
    if (text_len == 0) return;

    char *temp_text = strdup(text);
    if (!temp_text) {
        set_status_message("Error: Memory allocation for text block.");
        return;
    }

    char *current_segment = temp_text;
    char *newline_pos;
    int current_y = y;
    int current_x = x;
    int original_line_remainder_len = 0;
    char *original_line_remainder = NULL;

    if (current_y < E.num_lines && current_x < E.lines[current_y].size) {
        original_line_remainder = strdup(&E.lines[current_y].chars[current_x]);
        original_line_remainder_len = (int)strlen(original_line_remainder);
        E.lines[current_y].size = current_x;
        E.lines[current_y].chars[current_x] = '\0';
        E.lines[current_y].hl_revision++;
    }

    while (*current_segment != '\0') {
        newline_pos = strchr(current_segment, '\n');
        int segment_len;
        if (newline_pos != NULL) {
            segment_len = (int)(newline_pos - current_segment);
        } else {
            segment_len = (int)strlen(current_segment);
        }

        if (current_y >= E.num_lines) {
            editor_insert_line(current_y, current_segment, segment_len);
        } else {
            EditorLine *target_line = &E.lines[current_y];
            if ((size_t)target_line->size + (size_t)segment_len + 1 > (size_t)target_line->allocated) {
                int new_allocated = target_line->allocated + segment_len + 1;
                target_line->chars = realloc(target_line->chars, (size_t)new_allocated);
                target_line->hl = realloc(target_line->hl, sizeof(HighlightType) * (size_t)new_allocated);
                if (target_line->chars == NULL || target_line->hl == NULL) { free(temp_text); free(original_line_remainder); return; }
                target_line->allocated = new_allocated;
            }
            memmove(&target_line->chars[current_x + segment_len], &target_line->chars[current_x], (size_t)target_line->size - current_x + 1);
            memcpy(&target_line->chars[current_x], current_segment, (size_t)segment_len);
            target_line->size += segment_len;
            target_line->chars[target_line->size] = '\0';
            target_line->hl_revision++;
        }

        current_segment += segment_len;
        if (newline_pos != NULL) {
            current_y++;
            current_x = 0;
            current_segment++;
        } else {
            current_x += segment_len;
        }
    }

    if (original_line_remainder != NULL && original_line_remainder_len > 0) {
        if (current_y >= E.num_lines) {
            editor_insert_line(current_y, original_line_remainder, original_line_remainder_len);
        } else {
            EditorLine *target_line = &E.lines[current_y];
            if ((size_t)target_line->size + (size_t)original_line_remainder_len + 1 > (size_t)target_line->allocated) {
                int new_allocated = target_line->allocated + original_line_remainder_len + 1;
                target_line->chars = realloc(target_line->chars, (size_t)new_allocated);
                target_line->hl = realloc(target_line->hl, sizeof(HighlightType) * (size_t)new_allocated);
                if (target_line->chars == NULL || target_line->hl == NULL) { free(temp_text); free(original_line_remainder); return; }
                target_line->allocated = new_allocated;
            }
            memcpy(&target_line->chars[target_line->size], original_line_remainder, (size_t)original_line_remainder_len + 1);
            target_line->size += original_line_remainder_len;
            target_line->hl_revision++;
        }
    }

    free(temp_text);
    free(original_line_remainder);
    mark_lines_dirty(y, current_y);
}


/**
 * @brief Deletes a block of text from (sy,sx) to (ey,ex).
 * Assumes (sy,sx) and (ey,ex) are normalized coordinates.
 * @param sy Start Y of deletion.
 * @param sx Start X of deletion.
 * @param ey End Y of deletion.
 * @param ex End X of deletion.
 */
void editor_delete_text_block(int sy, int sx, int ey, int ex) {
    if (sy < 0 || sy >= E.num_lines || ey < 0 || ey >= E.num_lines) return;

    if (sy == ey) {
        EditorLine *line = &E.lines[sy];
        memmove(&line->chars[sx], &line->chars[ex], (size_t)line->size - ex + 1);
        line->size -= (ex - sx);
        line->hl_revision++;
    } else {
        EditorLine *start_line = &E.lines[sy];
        start_line->size = sx;
        start_line->chars[sx] = '\0';
        start_line->hl_revision++;

        EditorLine *end_line_to_append_from = &E.lines[ey];
        
        if ((size_t)start_line->size + (size_t)(end_line_to_append_from->size - ex) + 1 > (size_t)start_line->allocated) {
            int new_allocated = start_line->allocated + (end_line_to_append_from->size - ex) + 1;
            start_line->chars = realloc(start_line->chars, (size_t)new_allocated);
            start_line->hl = realloc(start_line->hl, sizeof(HighlightType) * (size_t)new_allocated);
            if (start_line->chars == NULL || start_line->hl == NULL) {
                set_status_message("Error: Failed to reallocate memory for block deletion join.");
                return;
            }
            start_line->allocated = new_allocated;
        }
        memcpy(&start_line->chars[start_line->size], &end_line_to_append_from->chars[ex], (size_t)(end_line_to_append_from->size - ex + 1));
        start_line->size += (end_line_to_append_from->size - ex);
        start_line->hl_revision++;

        for (int i = 0; i < (ey - sy); i++) {
            editor_delete_line(sy + 1);
        }
    }
    mark_lines_dirty(sy, E.num_lines);
}


/**
 * @brief Pastes content from the clipboard.
 */
void editor_paste_line(void) {
    if (E.clipboard_buffer == NULL || E.clipboard_size == 0) {
        set_status_message("Clipboard is empty.");
        return;
    }

    int pasted_num_lines = 1;
    for (int i = 0; i < E.clipboard_size; i++) {
        if (E.clipboard_buffer[i] == '\n') {
            pasted_num_lines++;
        }
    }

    UndoAction ua = {
        .type = UNDO_INSERT_BLOCK,
        .y = E.cursor_y,
        .x = E.cursor_x,
        .char_val = '\0',
        .text_content = strdup(E.clipboard_buffer),
        .text_len = E.clipboard_size,
        .num_lines_affected = pasted_num_lines
    };
    push_undo_action(ua);

    editor_insert_text_block(E.cursor_y, E.cursor_x, E.clipboard_buffer, E.clipboard_size);

    int final_cursor_y = E.cursor_y;
    int final_cursor_x = E.cursor_x;
    for (int i = 0; i < E.clipboard_size; i++) {
        if (E.clipboard_buffer[i] == '\n') {
            final_cursor_y++;
            final_cursor_x = 0;
        } else {
            final_cursor_x++;
        }
    }
    E.cursor_y = final_cursor_y;
    E.cursor_x = final_cursor_x;

    E.dirty = true;
    set_status_message("Pasted.");
}

/**
 * @brief Displays information about the current file.
 */
void editor_show_file_info(void) {
    if (E.filename == NULL) {
        set_status_message("No file loaded.");
        return;
    }

    struct stat st;
    if (stat(E.filename, &st) == -1) {
        set_status_message("Error getting file info for %s: %s", E.filename, strerror(errno));
        return;
    }

    char ctime_str[64];
    char mtime_str[64];
    strftime(ctime_str, sizeof(ctime_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_ctime));
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));

    set_status_message("File: %s | Size: %ld bytes | Lines: %d | Created: %s | Modified: %s",
                       E.filename, (long)st.st_size, E.num_lines, ctime_str, mtime_str);
}

/**
 * @brief Prompts for a line number and moves the cursor to that line.
 */
void editor_goto_line(void) {
    char line_num_buf[MAX_STATUS_MESSAGE_LENGTH];
    char *result = editor_prompt("Go to line: %s", line_num_buf, sizeof(line_num_buf));

    if (result == NULL) {
        set_status_message("Go to line cancelled.");
        return;
    }

    int target_line = atoi(result);
    if (target_line <= 0) {
        set_status_message("Invalid line number. Must be positive.");
        return;
    }

    target_line--; 

    if (target_line >= 0 && target_line < E.num_lines) {
        mark_lines_dirty(E.cursor_y, E.cursor_y);
        E.cursor_y = target_line;
        E.cursor_x = 0;
        mark_lines_dirty(E.cursor_y, E.cursor_y);
        set_status_message("Moved to line %d.", target_line + 1);
    } else {
        set_status_message("Line %d is out of bounds (total lines: %d).", target_line + 1, E.num_lines);
    }
}

/**
 * @brief Toggles the display of line numbers.
 */
void editor_toggle_line_numbers(void) {
    E.show_line_numbers = !E.show_line_numbers;
    set_status_message("Line numbers: %s", E.show_line_numbers ? "ON" : "OFF");
    mark_lines_dirty(0, E.num_lines - 1);
}

/**
 * @brief Displays a list of recently opened files and allows the user to select one.
 */
void editor_show_recent_files(void) {
    if (E.num_recent_files == 0) {
        set_status_message("No recently opened files.");
        return;
    }

    char prompt_msg[MAX_STATUS_MESSAGE_LENGTH * 2];
    int offset = snprintf(prompt_msg, sizeof(prompt_msg), "Recent Files (Select #, ESC to cancel):");
    for (int i = 0; i < E.num_recent_files; i++) {
        offset += snprintf(prompt_msg + offset, sizeof(prompt_msg) - offset, "\n%d. %s", i + 1, E.recent_files[i]);
    }
    offset += snprintf(prompt_msg + offset, sizeof(prompt_msg) - offset, "\nSelect: %%s");

    char choice_buf[10];
    char *result = editor_prompt(prompt_msg, choice_buf, sizeof(choice_buf));

    if (result == NULL) {
        set_status_message("Recent files selection cancelled.");
        return;
    }

    int selection = atoi(result);
    if (selection > 0 && selection <= E.num_recent_files) {
        editor_load_file(E.recent_files[selection - 1]);
    } else {
        set_status_message("Invalid selection.");
    }
}

/**
 * @brief Enters creative mode to define a new macro.
 * Prompts the user for the action string associated with the current command sequence.
 */
void enter_creative_mode(void) {
    if (E.macro_count >= MAX_MACROS) {
        set_status_message("Max macros reached (%d). Cannot create more.", MAX_MACROS);
        reset_command_mode();
        return;
    }
    if (E.cmd.length == 0) {
        set_status_message("Cannot create macro for empty sequence.");
        reset_command_mode();
        return;
    }

    E.creative_mode = true;
    char action_buf[MAX_MACRO_ACTION_LENGTH];
    char prompt_msg[MAX_STATUS_MESSAGE_LENGTH];
    snprintf(prompt_msg, sizeof(prompt_msg), "Creative Mode: Enter action for '%s': %%s", E.cmd.sequence);

    char *result = editor_prompt(prompt_msg, action_buf, sizeof(action_buf));

    if (result != NULL) {
        strncpy(E.macros[E.macro_count].sequence, E.cmd.sequence, MAX_COMMAND_SEQUENCE_LENGTH - 1);
        E.macros[E.macro_count].sequence[MAX_COMMAND_SEQUENCE_LENGTH - 1] = '\0';
        strncpy(E.macros[E.macro_count].action, action_buf, MAX_MACRO_ACTION_LENGTH - 1);
        E.macros[E.macro_count].action[MAX_MACRO_ACTION_LENGTH - 1] = '\0';
        E.macro_count++;
        set_status_message("Macro saved: '%s' => '%s'", E.macros[E.macro_count-1].sequence, E.macros[E.macro_count-1].action);
    } else {
        set_status_message("Macro creation cancelled.");
    }
    reset_command_mode();
}

/**
 * @brief Duplicates the current line.
 */
void editor_duplicate_line(void) {
    if (E.cursor_y >= E.num_lines) {
        set_status_message("Nothing to duplicate.");
        return;
    }
    EditorLine *line = &E.lines[E.cursor_y];
    
    editor_insert_line(E.cursor_y + 1, line->chars, line->size);

    UndoAction ua = {
        .type = UNDO_INSERT_BLOCK,
        .y = E.cursor_y + 1,
        .x = 0,
        .char_val = '\0',
        .text_content = strdup(line->chars),
        .text_len = line->size,
        .num_lines_affected = 1
    };
    push_undo_action(ua);
    set_status_message("Line duplicated.");
}

/**
 * @brief Changes the case of characters in the current line.
 *
 * @param to_upper If true, converts to uppercase; otherwise, to lowercase.
 */
void editor_change_line_case(bool to_upper) {
    if (E.cursor_y >= E.num_lines) {
        set_status_message("Nothing to change case.");
        return;
    }
    EditorLine *line = &E.lines[E.cursor_y];
    
    UndoAction ua = {
        .type = UNDO_MODIFY_LINE_CASE,
        .y = E.cursor_y,
        .x = 0,
        .char_val = '\0',
        .text_content = strdup(line->chars),
        .text_len = line->size,
        .num_lines_affected = 1
    };
    push_undo_action(ua);

    for (int i = 0; i < line->size; i++) {
        if (to_upper) {
            line->chars[i] = (char)toupper((unsigned char)line->chars[i]);
        } else {
            line->chars[i] = (char)tolower((unsigned char)line->chars[i]);
        }
    }
    
    mark_lines_dirty(E.cursor_y, E.cursor_y);
    set_status_message(to_upper ? "Converted to uppercase." : "Converted to lowercase.");
}

/**
 * @brief Sets the current keyboard input mode.
 *
 * @param mode The KeyboardMode to set.
 */
void editor_set_keyboard_mode(KeyboardMode mode) {
    E.keyboard_mode = mode;
}

/**
 * @brief Moves the cursor to the beginning of the current or previous word.
 */
void move_to_word_start(void) {
    if (E.cursor_y >= E.num_lines) return;
    EditorLine *line = &E.lines[E.cursor_y];
    int cx = E.cursor_x;

    while (cx > 0 && !isalnum((unsigned char)line->chars[cx - 1]) && !isspace((unsigned char)line->chars[cx-1])) {
        cx--;
    }
    while (cx > 0 && isalnum((unsigned char)line->chars[cx - 1])) {
        cx--;
    }
    mark_lines_dirty(E.cursor_y, E.cursor_y);
    E.cursor_x = cx;
    mark_lines_dirty(E.cursor_y, E.cursor_y);
}

/**
 * @brief Moves the cursor to the end of the current or next word.
 */
void move_to_word_end(void) {
    if (E.cursor_y >= E.num_lines) return;
    EditorLine *line = &E.lines[E.cursor_y];
    int cx = E.cursor_x;

    while (cx < line->size && !isalnum((unsigned char)line->chars[cx]) && !isspace((unsigned char)line->chars[cx])) {
        cx++;
    }
    while (cx < line->size && isalnum((unsigned char)line->chars[cx])) {
        cx++;
    }
    mark_lines_dirty(E.cursor_y, E.cursor_y);
    E.cursor_x = cx;
    mark_lines_dirty(E.cursor_y, E.cursor_y);
}

/**
 * @brief Normalizes selection coordinates so that (sy, sx) is always the top-left
 * and (ey, ex) is always the bottom-right of the selection.
 * This handles cases where the user selects backwards.
 * @param sy Pointer to store normalized start Y.
 * @param sx Pointer to store normalized start X.
 * @param ey Pointer to store normalized normalized end Y.
 * @param ex Pointer to store normalized normalized end X.
 */
void get_normalized_selection_coords(int *sy, int *sx, int *ey, int *ex) {
    if (E.visual_start_y < E.cursor_y) {
        *sy = E.visual_start_y;
        *sx = E.visual_start_x;
        *ey = E.cursor_y;
        *ex = E.cursor_x;
    } else if (E.visual_start_y > E.cursor_y) {
        *sy = E.cursor_y;
        *sx = E.cursor_x;
        *ey = E.visual_start_y;
        *ex = E.visual_start_x;
    } else {
        *sy = E.visual_start_y;
        *ey = E.cursor_y;
        if (E.visual_start_x < E.cursor_x) {
            *sx = E.visual_start_x;
            *ex = E.cursor_x;
        } else {
            *sx = E.cursor_x;
            *ex = E.visual_start_x;
        }
    }
}

/**
 * @brief Gets the content of the currently selected text.
 * @param sy Start Y of selection.
 * @param sx Start X of selection.
 * @param ey End Y of selection.
 * @param ex End X of selection.
 * @param num_lines Pointer to store the number of lines in the selection.
 * @return Dynamically allocated string containing the selected text, or NULL if error/empty.
 */
char *get_selection_content(int sy, int sx, int ey, int ex, int *num_lines) {
    *num_lines = 0;
    if (sy < 0 || sy >= E.num_lines || ey < 0 || ey >= E.num_lines) return NULL;

    size_t total_len = 0;
    for (int y = sy; y <= ey; y++) {
        int start_col = (y == sy) ? sx : 0;
        int end_col = (y == ey) ? ex : E.lines[y].size;
        if (end_col < start_col) end_col = start_col;
        total_len += (size_t)(end_col - start_col);
        if (y < ey) total_len++;
        (*num_lines)++;
    }

    if (total_len == 0) return NULL;

    char *buffer = malloc(total_len + 1);
    if (buffer == NULL) {
        set_status_message("Error: Failed to allocate memory for selection content.");
        return NULL;
    }

    char *ptr = buffer;
    for (int y = sy; y <= ey; y++) {
        int start_col = (y == sy) ? sx : 0;
        int end_col = (y == ey) ? ex : E.lines[y].size;
        if (end_col < start_col) end_col = start_col;
        
        memcpy(ptr, E.lines[y].chars + start_col, (size_t)(end_col - start_col));
        ptr += (end_col - start_col);
        if (y < ey) {
            *ptr++ = '\n';
        }
    }
    *ptr = '\0';
    return buffer;
}

/**
 * @brief Checks if a given character coordinate (row, col) is within the current selection.
 * @param row The row of the character.
 * @param col The column of the character.
 * @return True if the character is selected, false otherwise.
 */
bool is_char_in_selection(int row, int col) {
    if (!E.visual_mode) return false;

    int sy, sx, ey, ex;
    get_normalized_selection_coords(&sy, &sx, &ey, &ex);

    if (row < sy || row > ey) return false;

    if (row == sy && row == ey) {
        return (col >= sx && col < ex);
    } else if (row == sy) {
        return (col >= sx);
    } else if (row == ey) {
        return (col < ex);
    } else {
        return true;
    }
}

/**
 * @brief Toggles visual selection mode.
 */
void editor_toggle_visual_mode(void) {
    E.visual_mode = !E.visual_mode;
    if (E.visual_mode) {
        E.visual_start_x = E.cursor_x;
        E.visual_start_y = E.cursor_y;
        set_status_message("Visual Mode ON. Move cursor to select. ESC to cancel.");
    } else {
        set_status_message("Visual Mode OFF.");
    }
    mark_lines_dirty(0, E.num_lines - 1);
}

/**
 * @brief Copies the currently selected text to the clipboard.
 */
void editor_copy_selection(void) {
    if (!E.visual_mode) {
        editor_copy_line();
        return;
    }

    int sy, sx, ey, ex;
    get_normalized_selection_coords(&sy, &sx, &ey, &ex);
    
    int num_lines_in_selection;
    char *selection_content = get_selection_content(sy, sx, ey, ex, &num_lines_in_selection);

    if (selection_content == NULL) {
        set_status_message("Empty selection. Nothing copied.");
        editor_toggle_visual_mode();
        return;
    }

    if (strlen(selection_content) + 1 > (size_t)E.clipboard_allocated) {
        E.clipboard_allocated = (int)strlen(selection_content) + 1;
        E.clipboard_buffer = realloc(E.clipboard_buffer, (size_t)E.clipboard_allocated);
        if (E.clipboard_buffer == NULL) {
            set_status_message("Error: Failed to allocate clipboard memory for selection.");
            free(selection_content);
            return;
        }
    }

    memcpy(E.clipboard_buffer, selection_content, strlen(selection_content) + 1);
    E.clipboard_size = (int)strlen(selection_content);
    free(selection_content);

    set_status_message("Selection copied (%zu chars).", (size_t)E.clipboard_size);
    editor_toggle_visual_mode();
}

/**
 * @brief Deletes the currently selected text.
 */
void editor_delete_selection(void) {
    if (!E.visual_mode) {
        set_status_message("No selection to delete. Enter visual mode (Ctrl+V) first.");
        return;
    }

    int sy, sx, ey, ex;
    get_normalized_selection_coords(&sy, &sx, &ey, &ex);

    int num_lines_in_selection;
    char *deleted_content = get_selection_content(sy, sx, ey, ex, &num_lines_in_selection);

    if (deleted_content == NULL) {
        set_status_message("Empty selection. Nothing deleted.");
        editor_toggle_visual_mode();
        return;
    }

    UndoAction ua = {
        .type = UNDO_DELETE_BLOCK,
        .y = sy, .x = sx, .char_val = '\0',
        .text_content = deleted_content,
        .text_len = (int)strlen(deleted_content),
        .num_lines_affected = num_lines_in_selection
    };
    push_undo_action(ua);

    editor_delete_text_block(sy, sx, ey, ex);

    E.cursor_x = sx;
    E.cursor_y = sy;
    E.dirty = true;
    mark_lines_dirty(sy, E.num_lines - 1);

    set_status_message("Selection deleted.");
    editor_toggle_visual_mode();
}

/**
 * @brief Cuts the currently selected text to the clipboard.
 */
void editor_cut_selection(void) {
    if (!E.visual_mode) {
        editor_cut_line();
        return;
    }
    editor_copy_selection();

    E.visual_mode = true; 

    int sy, sx, ey, ex;
    get_normalized_selection_coords(&sy, &sx, &ey, &ex);
    
    int num_lines_in_selection;
    char *deleted_content = get_selection_content(sy, sx, ey, ex, &num_lines_in_selection);

    if (deleted_content == NULL) {
        set_status_message("Empty selection. Nothing cut.");
        editor_toggle_visual_mode();
        return;
    }

    UndoAction ua = {
        .type = UNDO_DELETE_BLOCK,
        .y = sy, .x = sx, .char_val = '\0',
        .text_content = deleted_content,
        .text_len = (int)strlen(deleted_content),
        .num_lines_affected = num_lines_in_selection
    };
    push_undo_action(ua);

    editor_delete_text_block(sy, sx, ey, ex);

    E.cursor_x = sx;
    E.cursor_y = sy;
    E.dirty = true;
    mark_lines_dirty(sy, E.num_lines - 1);

    set_status_message("Selection cut.");
    editor_toggle_visual_mode();
}

/**
 * @brief Adds a filename to the list of recently opened files.
 * Handles duplicates and maintains maximum list size.
 * @param filename The path to the file to add.
 */
void add_to_recent_files(const char *filename) {
    if (!filename) return;

    for (int i = 0; i < E.num_recent_files; i++) {
        if (E.recent_files[i] && strcmp(E.recent_files[i], filename) == 0) {
            char *temp = E.recent_files[i];
            for (int j = i; j > 0; j--) {
                E.recent_files[j] = E.recent_files[j-1];
            }
            E.recent_files[0] = temp;
            return;
        }
    }

    if (E.num_recent_files == MAX_RECENT_FILES) {
        free(E.recent_files[MAX_RECENT_FILES - 1]);
        for (int i = MAX_RECENT_FILES - 1; i > 0; i--) {
            E.recent_files[i] = E.recent_files[i-1];
        }
        E.num_recent_files--;
    }

    E.recent_files[0] = strdup(filename);
    if (E.recent_files[0] == NULL) {
        set_status_message("Error: Failed to store recent file path.");
        return;
    }
    E.num_recent_files++;
}

// --- Undo/Redo Implementation ---

/**
 * @brief Initializes the undo/redo history stacks.
 */
void init_undo_redo(void) {
    for (int i = 0; i < E.undo_head; i++) {
        free_undo_action(&E.undo_history[i]);
    }
    for (int i = 0; i < E.redo_head; i++) {
        free_undo_action(&E.redo_history[i]);
    }
    E.undo_head = 0;
    E.redo_head = 0;
}

/**
 * @brief Frees dynamically allocated memory within an UndoAction struct.
 * @param action Pointer to the UndoAction to free.
 */
void free_undo_action(UndoAction *action) {
    if (action->text_content) {
        free(action->text_content);
        action->text_content = NULL;
    }
}

/**
 * @brief Pushes an action onto the undo history stack.
 * Clears the redo history when a new action is pushed.
 * @param action The UndoAction to push.
 */
void push_undo_action(UndoAction action) {
    if (E.undo_head == MAX_UNDO_HISTORY) {
        free_undo_action(&E.undo_history[0]);
        for (int i = 0; i < MAX_UNDO_HISTORY - 1; i++) {
            E.undo_history[i] = E.undo_history[i+1];
        }
        E.undo_head--;
    }

    if (action.text_content) {
        E.undo_history[E.undo_head].text_content = strdup(action.text_content);
        if (E.undo_history[E.undo_head].text_content == NULL) {
            set_status_message("Error: Failed to allocate undo history memory.");
            return;
        }
    } else {
        E.undo_history[E.undo_head].text_content = NULL;
    }
    E.undo_history[E.undo_head].type = action.type;
    E.undo_history[E.undo_head].y = action.y;
    E.undo_history[E.undo_head].x = action.x;
    E.undo_history[E.undo_head].char_val = action.char_val;
    E.undo_history[E.undo_head].text_len = action.text_len;
    E.undo_history[E.undo_head].num_lines_affected = action.num_lines_affected;

    E.undo_head++;
    clear_redo_history();
}

/**
 * @brief Pushes an action onto the redo history stack.
 * @param action The UndoAction to push.
 */
void push_redo_action(UndoAction action) {
    if (E.redo_head == MAX_UNDO_HISTORY) {
        free_undo_action(&E.redo_history[0]);
        for (int i = 0; i < MAX_UNDO_HISTORY - 1; i++) {
            E.redo_history[i] = E.redo_history[i+1];
        }
        E.redo_head--;
    }

    if (action.text_content) {
        E.redo_history[E.redo_head].text_content = strdup(action.text_content);
        if (E.redo_history[E.redo_head].text_content == NULL) {
            set_status_message("Error: Failed to allocate redo history memory.");
            return;
        }
    } else {
        E.redo_history[E.redo_head].text_content = NULL;
    }
    E.redo_history[E.redo_head].type = action.type;
    E.redo_history[E.redo_head].y = action.y;
    E.redo_history[E.redo_head].x = action.x;
    E.redo_history[E.redo_head].char_val = action.char_val;
    E.redo_history[E.redo_head].text_len = action.text_len;
    E.redo_history[E.redo_head].num_lines_affected = action.num_lines_affected;

    E.redo_head++;
}

/**
 * @brief Clears the redo history stack and frees associated memory.
 */
void clear_redo_history(void) {
    for (int i = 0; i < E.redo_head; i++) {
        free_undo_action(&E.redo_history[i]);
    }
    E.redo_head = 0;
}

/**
 * @brief Performs an undo operation.
 */
void editor_undo(void) {
    if (E.undo_head == 0) {
        set_status_message("Nothing to undo.");
        return;
    }

    E.undo_head--;
    UndoAction ua = E.undo_history[E.undo_head];

    int original_cursor_y = E.cursor_y;
    int original_cursor_x = E.cursor_x;
    char *current_content_for_redo = NULL;
    int current_len_for_redo = 0;


    switch (ua.type) {
        case UNDO_INSERT_CHAR: {
            editor_line_delete_char(&E.lines[ua.y], ua.x);
            E.cursor_y = ua.y;
            E.cursor_x = ua.x;
            break;
        }
        case UNDO_DELETE_CHAR: {
            editor_line_insert_char(&E.lines[ua.y], ua.x, ua.char_val);
            E.cursor_y = ua.y;
            E.cursor_x = ua.x + 1;
            break;
        }
        case UNDO_INSERT_EMPTY_LINE: {
            editor_delete_line(ua.y);
            E.cursor_y = ua.y;
            E.cursor_x = 0;
            if (E.num_lines == 0) { editor_insert_line(0, "", 0); E.cursor_y = 0; E.cursor_x = 0; }
            else if (E.cursor_y >= E.num_lines) E.cursor_y = E.num_lines - 1;
            break;
        }
        case UNDO_SPLIT_LINE: {
            EditorLine *line_to_join = &E.lines[ua.y];
            EditorLine *next_line = &E.lines[ua.y + 1];

            if ((size_t)line_to_join->size + (size_t)next_line->size + 1 > (size_t)line_to_join->allocated) {
                int new_allocated = line_to_join->allocated + next_line->size + 1;
                line_to_join->chars = realloc(line_to_join->chars, (size_t)new_allocated);
                line_to_join->hl = realloc(line_to_join->hl, sizeof(HighlightType) * (size_t)new_allocated);
                if (line_to_join->chars == NULL || line_to_join->hl == NULL) { set_status_message("Error: Failed to reallocate line memory for undo join."); break; }
                line_to_join->allocated = new_allocated;
            }
            memcpy(&line_to_join->chars[line_to_join->size], next_line->chars, (size_t)next_line->size + 1);
            line_to_join->size += next_line->size;
            line_to_join->hl_revision++;

            editor_delete_line(ua.y + 1);
            E.cursor_y = ua.y;
            E.cursor_x = ua.x;
            break;
        }
        case UNDO_JOIN_LINES: {
            editor_insert_line(ua.y + 1, ua.text_content, ua.text_len);
            EditorLine *line_before_break = &E.lines[ua.y];
            line_before_break->size = ua.x;
            line_before_break->chars[ua.x] = '\0';
            line_before_break->hl_revision++;
            E.cursor_y = ua.y + 1;
            E.cursor_x = 0;
            break;
        }
        case UNDO_INSERT_BLOCK: {
            int sy = ua.y;
            int sx = ua.x;
            int ey = sy + ua.num_lines_affected - 1;
            int ex = sx;
            if (ua.num_lines_affected == 1) {
                ex = sx + ua.text_len;
            } else {
                const char *last_line_start = ua.text_content;
                const char *temp_ptr = ua.text_content;
                while (*temp_ptr != '\0') {
                    if (*temp_ptr == '\n') {
                        last_line_start = temp_ptr + 1;
                    }
                    temp_ptr++;
                }
                ex = (int)strlen(last_line_start);
            }
            editor_delete_text_block(sy, sx, ey, ex);
            E.cursor_y = sy;
            E.cursor_x = sx;
            break;
        }
        case UNDO_DELETE_BLOCK: {
            editor_insert_text_block(ua.y, ua.x, ua.text_content, ua.text_len);
            E.cursor_y = ua.y;
            E.cursor_x = ua.x;
            break;
        }
        case UNDO_MODIFY_LINE_CASE: {
            current_content_for_redo = strdup(E.lines[ua.y].chars);
            current_len_for_redo = E.lines[ua.y].size;

            free(E.lines[ua.y].chars);
            E.lines[ua.y].chars = strdup(ua.text_content);
            E.lines[ua.y].size = ua.text_len;
            E.lines[ua.y].allocated = ua.text_len + 1;
            E.lines[ua.y].hl_revision++;

            E.cursor_y = ua.y;
            E.cursor_x = ua.x;
            break;
        }
    }
    E.dirty = true;
    mark_lines_dirty(0, E.num_lines - 1);
    set_status_message("Undo successful.");

    UndoAction redo_ua;
    redo_ua.y = original_cursor_y;
    redo_ua.x = original_cursor_x;
    redo_ua.char_val = ua.char_val;
    redo_ua.text_len = ua.text_len;
    redo_ua.num_lines_affected = ua.num_lines_affected;

    switch (ua.type) {
        case UNDO_INSERT_CHAR: redo_ua.type = UNDO_DELETE_CHAR; redo_ua.char_val = ua.char_val; redo_ua.text_content = NULL; break;
        case UNDO_DELETE_CHAR: redo_ua.type = UNDO_INSERT_CHAR; redo_ua.char_val = ua.char_val; redo_ua.text_content = NULL; break;
        case UNDO_INSERT_EMPTY_LINE: redo_ua.type = UNDO_INSERT_EMPTY_LINE; redo_ua.text_content = NULL; redo_ua.text_len = 0; redo_ua.num_lines_affected = 0; break;
        case UNDO_SPLIT_LINE: redo_ua.type = UNDO_SPLIT_LINE; redo_ua.text_content = strdup(ua.text_content); redo_ua.text_len = ua.text_len; redo_ua.num_lines_affected = 0; break;
        case UNDO_JOIN_LINES: redo_ua.type = UNDO_JOIN_LINES; redo_ua.text_content = strdup(ua.text_content); redo_ua.text_len = ua.text_len; redo_ua.num_lines_affected = 0; break;
        case UNDO_INSERT_BLOCK: redo_ua.type = UNDO_INSERT_BLOCK; redo_ua.text_content = strdup(ua.text_content); redo_ua.text_len = ua.text_len; redo_ua.num_lines_affected = ua.num_lines_affected; break;
        case UNDO_DELETE_BLOCK: redo_ua.type = UNDO_DELETE_BLOCK; redo_ua.text_content = strdup(ua.text_content); redo_ua.text_len = ua.text_len; redo_ua.num_lines_affected = ua.num_lines_affected; break;
        case UNDO_MODIFY_LINE_CASE: redo_ua.type = UNDO_MODIFY_LINE_CASE; redo_ua.text_content = current_content_for_redo; redo_ua.text_len = current_len_for_redo; redo_ua.num_lines_affected = 1; break;
    }
    push_redo_action(redo_ua);

    free_undo_action(&ua);
}

/**
 * @brief Performs a redo operation.
 */
void editor_redo(void) {
    if (E.redo_head == 0) {
        set_status_message("Nothing to redo.");
        return;
    }

    E.redo_head--;
    UndoAction ra = E.redo_history[E.redo_head];

    int original_cursor_y = E.cursor_y;
    int original_cursor_x = E.cursor_x;
    char *current_content_for_undo = NULL;
    int current_len_for_undo = 0;


    switch (ra.type) {
        case UNDO_INSERT_CHAR: {
            editor_line_insert_char(&E.lines[ra.y], ra.x, ra.char_val);
            E.cursor_y = ra.y;
            E.cursor_x = ra.x + 1;
            break;
        }
        case UNDO_DELETE_CHAR: {
            editor_line_delete_char(&E.lines[ra.y], ra.x);
            E.cursor_y = ra.y;
            E.cursor_x = ra.x;
            break;
        }
        case UNDO_INSERT_EMPTY_LINE: {
            editor_insert_line(ra.y, "", 0);
            E.cursor_y = ra.y + 1;
            E.cursor_x = 0;
            break;
        }
        case UNDO_SPLIT_LINE: {
            EditorLine *line = &E.lines[ra.y];
            editor_insert_line(ra.y + 1, &line->chars[ra.x], line->size - ra.x);
            line = &E.lines[ra.y];
            line->size = ra.x;
            line->chars[line->size] = '\0';
            line->hl_revision++;
            E.cursor_y = ra.y + 1;
            E.cursor_x = 0;
            break;
        }
        case UNDO_JOIN_LINES: {
            E.cursor_x = E.lines[ra.y].size;
            EditorLine *line_to_delete = &E.lines[ra.y + 1];
            
            EditorLine *prev_line = &E.lines[ra.y];
            if ((size_t)prev_line->size + (size_t)line_to_delete->size + 1 > (size_t)prev_line->allocated) {
                int new_allocated = prev_line->allocated + line_to_delete->size + 1;
                prev_line->chars = realloc(prev_line->chars, (size_t)new_allocated);
                prev_line->hl = realloc(prev_line->hl, sizeof(HighlightType) * (size_t)new_allocated);
                if (prev_line->chars == NULL || prev_line->hl == NULL) { set_status_message("Error: Failed to reallocate line memory for redo join."); break; }
                prev_line->allocated = new_allocated;
            }
            memcpy(&prev_line->chars[prev_line->size], line_to_delete->chars, (size_t)line_to_delete->size + 1);
            prev_line->size += line_to_delete->size;
            prev_line->hl_revision++;
            
            editor_delete_line(ra.y + 1);
            E.cursor_y = ra.y;
            E.cursor_x = ra.x;
            break;
        }
        case UNDO_INSERT_BLOCK: {
            editor_insert_text_block(ra.y, ra.x, ra.text_content, ra.text_len);
            E.cursor_y = ra.y;
            E.cursor_x = ra.x;
            break;
        }
        case UNDO_DELETE_BLOCK: {
            int sy = ra.y;
            int sx = ra.x;
            int ey = sy + ra.num_lines_affected - 1;
            int ex = sx;
            if (ra.num_lines_affected == 1) {
                ex = sx + ra.text_len;
            } else {
                const char *last_line_start = ra.text_content;
                const char *temp_ptr = ra.text_content;
                while (*temp_ptr != '\0') {
                    if (*temp_ptr == '\n') {
                        last_line_start = temp_ptr + 1;
                    }
                    temp_ptr++;
                }
                ex = (int)strlen(last_line_start);
            }
            editor_delete_text_block(sy, sx, ey, ex);
            E.cursor_y = sy;
            E.cursor_x = sx;
            break;
        }
        case UNDO_MODIFY_LINE_CASE: {
            current_content_for_undo = strdup(E.lines[ra.y].chars);
            current_len_for_undo = E.lines[ra.y].size;

            free(E.lines[ra.y].chars);
            E.lines[ra.y].chars = strdup(ra.text_content);
            E.lines[ra.y].size = ra.text_len;
            E.lines[ra.y].allocated = ra.text_len + 1;
            E.lines[ra.y].hl_revision++;

            E.cursor_y = ra.y;
            E.cursor_x = ra.x;
            break;
        }
    }
    E.dirty = true;
    mark_lines_dirty(0, E.num_lines - 1);
    set_status_message("Redo successful.");

    UndoAction undo_ra;
    undo_ra.y = original_cursor_y;
    undo_ra.x = original_cursor_x;
    undo_ra.char_val = ra.char_val;
    undo_ra.text_len = ra.text_len;
    undo_ra.num_lines_affected = ra.num_lines_affected;

    switch (ra.type) {
        case UNDO_INSERT_CHAR: undo_ra.type = UNDO_DELETE_CHAR; undo_ra.char_val = ra.char_val; undo_ra.text_content = NULL; break;
        case UNDO_DELETE_CHAR: undo_ra.type = UNDO_INSERT_CHAR; undo_ra.char_val = ra.char_val; undo_ra.text_content = NULL; break;
        case UNDO_INSERT_EMPTY_LINE: undo_ra.type = UNDO_INSERT_EMPTY_LINE; undo_ra.text_content = NULL; undo_ra.text_len = 0; undo_ra.num_lines_affected = 0; break;
        case UNDO_SPLIT_LINE: undo_ra.type = UNDO_SPLIT_LINE; undo_ra.text_content = strdup(ra.text_content); undo_ra.text_len = ra.text_len; undo_ra.num_lines_affected = 0; break;
        case UNDO_JOIN_LINES: undo_ra.type = UNDO_JOIN_LINES; undo_ra.text_content = strdup(ra.text_content); undo_ra.text_len = ra.text_len; undo_ra.num_lines_affected = 0; break;
        case UNDO_INSERT_BLOCK: undo_ra.type = UNDO_INSERT_BLOCK; undo_ra.text_content = strdup(ra.text_content); undo_ra.text_len = ra.text_len; undo_ra.num_lines_affected = ra.num_lines_affected; break;
        case UNDO_DELETE_BLOCK: undo_ra.type = UNDO_DELETE_BLOCK; undo_ra.text_content = strdup(ra.text_content); undo_ra.text_len = ra.text_len; undo_ra.num_lines_affected = ra.num_lines_affected; break;
        case UNDO_MODIFY_LINE_CASE: undo_ra.type = UNDO_MODIFY_LINE_CASE; undo_ra.text_content = current_content_for_undo; undo_ra.text_len = current_len_for_undo; undo_ra.num_lines_affected = 1; break;
    }
    push_undo_action(undo_ra);

    free_undo_action(&ra);
}

/**
 * @brief Selects all text in the editor.
 */
void editor_select_all(void) {
    if (E.num_lines == 0) {
        set_status_message("No text to select.");
        return;
    }
    E.visual_mode = true;
    E.visual_start_x = 0;
    E.visual_start_y = 0;
    E.cursor_y = E.num_lines - 1;
    E.cursor_x = E.lines[E.num_lines - 1].size;
    set_status_message("All text selected. Use Ctrl+C/Ctrl+X to copy/cut.");
    mark_lines_dirty(0, E.num_lines - 1);
}

/**
 * @brief Processes a single key press from the user.
 *
 * This is the main input handling loop.
 */
void editor_process_keypress(void) {
    int c = editor_read_key();

    if (E.cmd.active) {
        handle_command_mode_input(c);
        return;
    }

    switch (c) {
        case CTRL('q'):
            editor_quit(false);
            break;
        case CTRL('s'):
            editor_save_file();
            break;
        case CTRL('o'): // Ctrl+O for Open file
            editor_open_file();
            break;
        case CTRL('\\'): // Ctrl+\ for Command mode (changed from Ctrl+J)
            E.cmd.active = true;
            E.cmd.length = 0;
            E.cmd.sequence[0] = '\0';
            E.cmd.last_key_time = time(NULL);
            set_status_message("Command Mode: (type command sequence)");
            break;
        case CTRL('f'): // Ctrl+F for Find
            editor_find();
            break;
        case CTRL('a'): // Ctrl+A for Select All
            editor_select_all();
            break;
        case CTRL('v'): // Visual mode
            editor_toggle_visual_mode();
            break;
        case CTRL('c'): // Copy
            editor_copy_selection();
            break;
        case CTRL('x'): // Cut
            editor_cut_selection();
            break;
        case CTRL('p'): // Paste
            editor_paste_line();
            break;
        case CTRL('z'): // Undo
            editor_undo();
            break;
        case CTRL('y'): // Redo
            editor_redo();
            break;
        case CTRL('h'): // Ctrl+H for Help
            show_command_help_screen();
            break;
        case CTRL('g'): // Ctrl+G for Go to Line
            editor_goto_line();
            break;
        case KEY_HOME:
            E.cursor_x = 0;
            break;
        case KEY_END:
            if (E.cursor_y < E.num_lines) {
                E.cursor_x = E.lines[E.cursor_y].size;
            }
            break;
        case KEY_PPAGE: // Page Up
            E.cursor_y = E.scroll_y;
            for (int i = 0; i < E.screen_rows; i++) {
                editor_move_cursor(KEY_UP);
            }
            break;
        case KEY_NPAGE: // Page Down
            E.cursor_y = E.scroll_y + E.screen_rows - 1;
            if (E.cursor_y > E.num_lines) E.cursor_y = E.num_lines;
            for (int i = 0; i < E.screen_rows; i++) {
                editor_move_cursor(KEY_DOWN);
            }
            break;
        case CTRL('r'): // Move to end of next word
            move_to_word_end();
            break;
        case CTRL('w'): // Move to start of previous word
            move_to_word_start();
            break;
        case CTRL('e'): // Move to end of file
            E.cursor_y = E.num_lines > 0 ? E.num_lines - 1 : 0;
            E.cursor_x = E.lines[E.cursor_y].size;
            break;
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_UP:
        case KEY_DOWN:
            editor_move_cursor(c);
            break;
        case KEY_BACKSPACE:
        case 127: // ASCII DEL
            editor_delete_char();
            break;
        case KEY_DC: // Delete key
            if (E.cursor_x < E.lines[E.cursor_y].size) {
                editor_line_delete_char(&E.lines[E.cursor_y], E.cursor_x);
                E.dirty = true;
                mark_lines_dirty(E.cursor_y, E.cursor_y);
            } else if (E.cursor_y < E.num_lines - 1) { // Delete at end of line joins with next
                char *deleted_content = strdup(E.lines[E.cursor_y+1].chars);
                int current_line_size = E.lines[E.cursor_y].size;

                UndoAction ua = {
                    .type = UNDO_JOIN_LINES,
                    .y = E.cursor_y,
                    .x = current_line_size,
                    .char_val = '\0',
                    .text_content = deleted_content,
                    .text_len = (int)strlen(deleted_content),
                    .num_lines_affected = 0
                };
                push_undo_action(ua);

                EditorLine *current_line = &E.lines[E.cursor_y];
                EditorLine *next_line = &E.lines[E.cursor_y + 1];
                if ((size_t)current_line->size + (size_t)next_line->size + 1 > (size_t)current_line->allocated) {
                    int new_allocated = current_line->allocated + next_line->size + 1;
                    current_line->chars = realloc(current_line->chars, (size_t)new_allocated);
                    current_line->hl = realloc(current_line->hl, sizeof(HighlightType) * (size_t)new_allocated);
                    if (current_line->chars == NULL || current_line->hl == NULL) {
                        set_status_message("Error: Failed to reallocate line memory for join (DEL).");
                        return;
                    }
                    current_line->allocated = new_allocated;
                }
                memcpy(&current_line->chars[current_line->size], next_line->chars, (size_t)next_line->size + 1);
                current_line->size += next_line->size;
                current_line->hl_revision++;
                editor_delete_line(E.cursor_y + 1);
                E.dirty = true;
                mark_lines_dirty(E.cursor_y, E.cursor_y);
            }
            break;
        case KEY_ENTER:
        case '\n':
            editor_insert_newline();
            break;
        default:
            if (isprint(c)) {
                editor_insert_char(c);
            }
            break;
    }
}


/**
 * @brief Main function of the Unied editor.
 *
 * Initializes the editor, loads the specified file (or an empty buffer),
 * enters the main loop for processing input and refreshing the screen,
 * and finally deinitializes the editor before exiting.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings representing the command-line arguments.
 * Expected: argv[1] is the path to the file to open (optional).
 * @return 0 on successful execution, non-zero on error.
 */
int main(int argc, char *argv[]) {
    // Initialize ncurses early for loading screen
    initscr();
    if (has_colors()) {
        start_color();
        init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
    }
    noecho();
    curs_set(0); // Hide cursor during loading
    refresh();

    display_loading_screen(); // Show loading animation

    endwin(); // Deinitialize ncurses to re-init with full settings
    
    init_editor(); // Initialize editor with full ncurses settings

    if (argc >= 2) {
        editor_load_file(argv[1]);
    } else {
        // If no filename provided, start with one empty line
        editor_insert_line(0, "", 0);
        E.dirty = false;
        // Updated status message for new files
        set_status_message("NEW FILE - Press Ctrl+S to save. Ctrl+H for help.");
        mark_lines_dirty(0,0); // Mark the initial empty line dirty for redraw
        prompt_file_type(); // Prompt for file type on new file
    }

    // Main editor loop
    while (true) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    // deinit_editor() is called inside editor_process_keypress when quitting
    return 0;
}
