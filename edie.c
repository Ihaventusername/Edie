/**
 * EDIE - The Minimalist Scriptable Binary/Text Editor
 * * Features: 
 * - Pointer-based navigation and macro loops
 * - Inline Visual Mode with real-time command execution
 * - Sparse file support (automatic padding)
 * - Standalone: Zero dependencies beyond standard I/O
 * * Usage:
 * - Compile: gcc -O2 edie.c -o edie
 * - Instructions: +{file} (Open), ^^...^^ (Insert), %%...%% (Overwrite), 
 * |n...| (Loop), ^V (Visual Mode), EXIT (Quit)
 */

#include <stdio.h>

/* Forward declaration for system calls to avoid stdlib.h dependency */
extern int system(const char *command);

/* Configuration */
#define MAX_BUF 65535
#define MAX_PATH 256
#define LOOP_BUF 256

/* Core Context Structure */
typedef struct {
    char data[MAX_BUF];
    int size;           /* Total data size */
    int ptr;            /* Current cursor position */
    char path[MAX_PATH];/* Current file path */
    int is_hex;         /* Hexadecimal input mode toggle */
} Edie;

/* --- Minimal Utility Library --- */

/* Memory move with overlap protection (memmove equivalent) */
static void b_mov(char *dst, char *src, int n) {
    if (n <= 0) return;
    if (dst > src) {
        while (n--) dst[n] = src[n];
    } else {
        int i = 0;
        while (i < n) { dst[i] = src[i]; i++; }
    }
}

static int s_len(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (int)(p - s);
}

static int s_cmp(const char *s1, const char *s2, int n) {
    while (n-- && *s1 && *s1 == *s2) { s1++; s2++; }
    if (n == -1) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* String to Integer (advances pointer) */
static int s_atoi(char **p) {
    int n = 0;
    while (**p >= '0' && **p <= '9') {
        n = n * 10 + (**p - '0');
        (*p)++;
    }
    return n;
}

/* Hex character to byte value */
static int h2b(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

/* --- Visual Interface Module --- */

/* Forward declaration for the recursive engine */
void parse(Edie *e, char *cmd);

/* Renders a single-line "radar" view of the buffer */
static void show_line_view(Edie *e, const char *v_cmd, int vi) {
    int win = 40; /* Viewport width */
    int start = e->ptr - (win / 2);
    if (start < 0) start = 0;
    
    printf("\r\033[K"); /* CR + Clear Line */

    /* Render Tag/Command Area */
    if (vi == 0) {
        printf("[VIEW] ");
    } else {
        /* Scroll long commands to keep the tail visible */
        int disp_start = (vi > 10) ? vi - 10 : 0;
        printf("[%s] ", &v_cmd[disp_start]);
    }

    /* Render Buffer Content Area */
    for (int k = 0; k < win; k++) {
        int idx = start + k;
        if (idx < MAX_BUF) {
            char c = (idx < e->size) ? e->data[idx] : ' ';
            if (c < 32 || c > 126) c = '.'; /* Non-printable placeholder */
            
            if (idx == e->ptr) printf("\033[7m%c\033[0m", c); /* Highlight cursor */
            else printf("%c", c);
        }
    }
    printf(" %d/%d ", e->ptr, e->size);
    fflush(stdout);
}

/* Interactive Visual Mode: Inline command execution */
static void enter_visual_mode(Edie *e) {
    char v_cmd[128] = {0};
    int vi = 0;
    
    /* Enter raw terminal mode (disable echo/canonical processing) */
    system("stty -icanon -echo");
    
    while (1) {
        show_line_view(e, v_cmd, vi);
        char c = getchar();

        if (c == 22) break; /* Ctrl+V to Exit */
        
        if (c == 127 || c == 8) { /* Backspace */
            if (vi > 0) v_cmd[--vi] = 0;
            continue;
        }

        if (c == '\r' || c == '\n') { /* Execute command in-place */
            if (vi > 0) {
                v_cmd[vi] = 0;
                parse(e, v_cmd);
                for(int k = 0; k < 128; k++) v_cmd[k] = 0;
                vi = 0;
            }
            continue;
        }

        /* Quick navigation when command buffer is empty */
        if (vi == 0 && c == '>') {
            if (e->ptr < MAX_BUF - 1) {
                if (e->ptr == e->size) { e->data[e->size] = ' '; e->size++; }
                e->ptr++;
            }
        } else if (vi == 0 && c == '<') {
            if (e->ptr > 0) e->ptr--;
        } 
        /* Buffer command input */
        else if (vi < 127 && c >= 32 && c <= 126) {
            v_cmd[vi++] = c;
            v_cmd[vi] = 0;
        }
    }
    
    system("stty icanon echo"); /* Restore terminal */
    printf("\n");
}

/* --- Core Parsing Engine (Recursive) --- */

void parse(Edie *e, char *cmd) {
    int i = 0;
    while (cmd[i]) {
        char c = cmd[i];

        if (c == ':' || c == '_') { i++; continue; }

        if (c == 22) {
            enter_visual_mode(e);
            i += 2;
            continue;
        }

        /* ^^ (Insert) or %% (Overwrite) block */
        if ((c == '^' && cmd[i+1] == '^') || (c == '%' && cmd[i+1] == '%')) {
            int is_ins = (c == '^'); 
            i += 2;
            
            while (cmd[i] && !(cmd[i] == c && cmd[i+1] == c)) {
                if (e->ptr >= MAX_BUF - 1) break;

                unsigned char byte;
                if (e->is_hex) {
                    byte = (h2b(cmd[i]) << 4) | h2b(cmd[i+1]);
                    i += 2;
                } else {
                    byte = cmd[i++];
                }

                if (is_ins) {
                    int move_len = e->size - e->ptr;
                    if (move_len < 0) move_len = 0;
                    b_mov(e->data + e->ptr + 1, e->data + e->ptr, move_len);
                    e->size++;
                } else {
                    if (e->ptr == e->size) e->size++;
                }
                e->data[e->ptr++] = byte;
            }
            if (cmd[i]) i += 2;
        }
        
        /* Navigation & Padding */
        else if (c == '>') {
            if (e->ptr < MAX_BUF - 1) {
                if (e->ptr == e->size) {
                    e->data[e->size] = e->is_hex ? 0 : ' ';
                    e->size++;
                }
                e->ptr++;
            }
            i++;
        }
        else if (c == '<') {
            if (e->ptr > 0) e->ptr--;
            i++;
        }
        else if (c == ')') { e->ptr = 0; i++; }
        else if (c == '(') { e->ptr = e->size; i++; }

        /* |n...| Loop container */
        /* |n...| Loop container fixed */
else if (c == '|') {
    i++;
    char *p_num = &cmd[i];
    int n = s_atoi(&p_num); 
    i = (int)(p_num - cmd); 
    
    int start_idx = i;
    int depth = 1;
    
    while (cmd[i] && depth > 0) {
        if (cmd[i] == '|') {
            depth--;
            if (depth == 0) break; 
        } else if (cmd[i] == ' ' && depth == 1) {
            //Nothing :(
        }
        i++;
    }
    
    if (depth == 0) {
        int len = i - start_idx; 
        if (len > 0 && n > 0) {
            char sub_cmd[LOOP_BUF];
            if (len >= LOOP_BUF) len = LOOP_BUF - 1; 
            
            for(int k=0; k<len; k++) sub_cmd[k] = cmd[start_idx + k];
            sub_cmd[len] = 0;

            while (n--) {
                parse(e, sub_cmd); 
            }
        }
        i++; 
        continue; 
    }
}

        /* HEX */

else if (c == '`') {
    e->is_hex = !e->is_hex; /* 0 -> 1 or 1 -> 0 */
    printf(e->is_hex ? "\n[MODE: HEX]\n" : "\n[MODE: TEXT]\n");
    i++;
}


        /* Save File */
        else if (c == '=') {
            i++;
            if (cmd[i] == '{') {
                i++;
                char target[MAX_PATH];
                int ti = 0;
                while (cmd[i] && cmd[i] != '}') {
                    if (cmd[i] == '$') {
                        int k = 0;
                        while (e->path[k] && ti < MAX_PATH - 1) 
                            target[ti++] = e->path[k++];
                    } else if (ti < MAX_PATH - 1) {
                        target[ti++] = cmd[i];
                    }
                    i++;
                }
                target[ti] = 0;
                
                FILE *f = fopen(target, "wb");
                if (f) {
                    fwrite(e->data, 1, e->size, f);
                    fclose(f);
                    printf("S %dB\n", e->size);
                }
                if (cmd[i] == '}') i++;
            }
        }
        else if (c == 'P') {
            printf("[%d] ", e->ptr);
            for(int k=-2; k<=2; k++) {
                int pos = e->ptr + k;
                if(pos >= 0 && pos < e->size) printf("%02X ", (unsigned char)e->data[pos]);
            }
            printf("\n");
            i++;
        }
        else i++;
    }
}

/* --- Entry Point --- */

int main(int argc, char **argv) {
    Edie e = {0};
    
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'H') e.is_hex = 1;

    char cmd[1024];
    while (1) {
        printf(e.is_hex ? "_ " : ":");
        
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        
        int len = s_len(cmd);
        if (len > 0 && cmd[len-1] == '\n') cmd[len-1] = 0;

        if (s_cmp(cmd, "EXIT", 4) == 0) break;

        /* +{file} Context Switch */
        if (cmd[0] == '+') {
            char *p_open = NULL, *p_close = NULL;
            int k = 0;
            while (cmd[k]) {
                if (cmd[k] == '{' && !p_open) p_open = &cmd[k];
                if (cmd[k] == '}') p_close = &cmd[k];
                k++;
            }

            if (p_open && p_close && p_close > p_open) {
                e.size = 0; e.ptr = 0;
                int path_len = (int)(p_close - p_open - 1);
                if (path_len >= MAX_PATH) path_len = MAX_PATH - 1;
                int m = 0;
                while (m < path_len) {
                    e.path[m] = p_open[m+1];
                    m++;
                }
                e.path[m] = 0;

                FILE *f = fopen(e.path, "rb");
                if (f) {
                    e.size = (int)fread(e.data, 1, MAX_BUF, f);
                    fclose(f);
                    printf("L %dB\n", e.size);
                } else {
                    printf("N %s\n", e.path);
                }
            }
        } else {
            parse(&e, cmd);
        }
    }
    return 0;
}
