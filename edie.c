/**
 * EDIE - Minimalist Scriptable Binary/Text Editor (Rewritten)
 * - 单缓冲区、指针导航
 * - 命令驱动编辑（parse）
 * - 可选 MTS 语法高亮（Marked Text Sheet）
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ===== 配置区域 ===== */

#define EDIE_MAX_BUF   65535   /* 编辑缓冲区最大长度 */
#define EDIE_MAX_PATH  256     /* 路径最大长度 */
#define EDIE_MAX_RULES 128     /* MTS 高亮规则最大条数 */

/* ===== 核心上下文结构 ===== */

typedef struct {
    char data[EDIE_MAX_BUF];   /* 文件内容缓冲区 */
    int  size;                 /* 当前有效数据长度 */
    int  ptr;                  /* 当前指针位置（光标） */
    char path[EDIE_MAX_PATH];  /* 当前文件路径 */
    int  is_hex;               /* 是否处于 HEX 模式（以后可用） */
} Edie;

/* ===== MTS 高亮规则表 =====
 * 每条规则：
 *   pattern: 要匹配的字符串
 *   color:   0xRRGGBB 形式的颜色
 *   type:    0 = A (完全匹配)，1 = H (包含匹配)
 */

static char         g_mts_pattern[EDIE_MAX_RULES][64];
static unsigned int g_mts_color[EDIE_MAX_RULES];
static unsigned char g_mts_type[EDIE_MAX_RULES];
static int          g_mts_count = 0;

/* ===== 加载 MTS 高亮规则 =====
 * MTS 文件格式：
 *   <pattern> <HEX> <A|H>
 * 例如：
 *   if FF5555 A
 *   ;; FF8800 H
 *   " FFCC00 H
 */
static void edie_load_mts(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("MTS load failed: %s\n", path);
        return;
    }

    g_mts_count = 0;

    while (!feof(f) && g_mts_count < EDIE_MAX_RULES) {
        char pat[64], hex[16], type[4];

        /* 读取一行规则 */
        if (fscanf(f, "%63s %15s %3s", pat, hex, type) == 3) {

            /* 复制 pattern */
            strcpy(g_mts_pattern[g_mts_count], pat);

            /* 解析颜色（HEX → int） */
            g_mts_color[g_mts_count] = strtoul(hex, NULL, 16);

            /* 匹配类型：A=完全匹配，H=包含匹配 */
            g_mts_type[g_mts_count] = (type[0] == 'A') ? 0 : 1;

            g_mts_count++;
        }
    }

    fclose(f);
    printf("Loaded MTS: %s (%d rules)\n", path, g_mts_count);
}
/* ===== 根据 token 返回颜色 =====
 * 输入：一个 token（例如 "if" 或 ";;"）
 * 输出：0xRRGGBB 颜色值
 *
 * 匹配规则：
 *   type = 0 → 完全匹配（A）
 *   type = 1 → 包含匹配（H）
 */
static unsigned int edie_color_for(const char *word) {
    for (int i = 0; i < g_mts_count; i++) {

        /* 完全匹配：word 必须与 pattern 完全一致 */
        if (g_mts_type[i] == 0) {
            if (strcmp(word, g_mts_pattern[i]) == 0)
                return g_mts_color[i];
        }

        /* 包含匹配：word 中包含 pattern 即可 */
        else {
            if (strstr(word, g_mts_pattern[i]) != NULL)
                return g_mts_color[i];
        }
    }

    /* 默认颜色：白色 */
    return 0xFFFFFF;
}
/* ===== 显示窗口（带语法高亮） =====
 * 只显示指针附近的 40 字符窗口
 * 自动分词 → 调用 edie_color_for() → 输出彩色 token
 * 光标位置使用反色显示
 */
static void show_line_view(Edie *e) {
    int win = 40;
    int start = e->ptr - win / 2;
    if (start < 0)
        start = 0;

    printf("\r\033[K[VIEW] ");

    char token[256];
    int token_len = 0;

    for (int k = 0; k < win; k++) {
        int idx = start + k;

        /* 超出缓冲区 → 空白 */
        if (idx >= e->size) {
            putchar(' ');
            continue;
        }

        char c = e->data[idx];

        /* 构建 token：字母数字下划线分号 */
        if (isalnum(c) || c == '_' || c == ';') {
            if (token_len < 255)
                token[token_len++] = c;
            continue;
        }

        /* token 结束 → 输出彩色 token */
        if (token_len > 0) {
            token[token_len] = 0;
            unsigned int col = edie_color_for(token);

            int r = (col >> 16) & 0xFF;
            int g = (col >> 8) & 0xFF;
            int b = col & 0xFF;

            printf("\x1b[38;2;%d;%d;%d m%s\x1b[0m", r, g, b, token);
            token_len = 0;
        }

        /* 输出当前字符（非 token） */
        if (idx == e->ptr)
            printf("\033[7m%c\033[0m", c);  /* 光标反色 */
        else
            putchar(c);
    }

    printf(" %d/%d", e->ptr, e->size);
    fflush(stdout);
}
/* 前置声明：这些函数在别处实现 */
static void enter_visual_mode(Edie *e);
static int  s_cmp(const char *s1, const char *s2, int n);
static int  s_atoi(char **p);
static int  s_len(const char *s);
static void b_mov(char *dst, char *src, int n);
static int  h2b(char c);

/* ===== 核心解析引擎：完整版 ===== */
static void parse(Edie *e, char *cmd) {
    int i = 0;

    while (cmd[i]) {
        char c = cmd[i];

        /* 无操作标记 */
        if (c == ':' || c == '_') {
            i++;
            continue;
        }

        /* 进入可视模式：^V (ASCII 22) */
        if ((unsigned char)c == 22) {
            enter_visual_mode(e);
            i++;
            continue;
        }

        /* ^^ (插入) / %% (覆盖) 块 */
        if ((c == '^' && cmd[i+1] == '^') || (c == '%' && cmd[i+1] == '%')) {
            int is_ins = (c == '^');
            i += 2;

            while (cmd[i] && !(cmd[i] == c && cmd[i+1] == c)) {
                if (e->ptr >= EDIE_MAX_BUF - 1)
                    break;

                unsigned char byte;

                if (e->is_hex) {
                    /* HEX 模式：两个字符组成一个字节 */
                    if (!cmd[i] || !cmd[i+1])
                        break;
                    byte = (h2b(cmd[i]) << 4) | h2b(cmd[i+1]);
                    i += 2;
                } else {
                    byte = (unsigned char)cmd[i++];
                }

                if (is_ins) {
                    int move_len = e->size - e->ptr;
                    if (move_len < 0) move_len = 0;
                    b_mov(e->data + e->ptr + 1, e->data + e->ptr, move_len);
                    e->size++;
                } else {
                    if (e->ptr == e->size)
                        e->size++;
                }

                e->data[e->ptr++] = byte;
            }

            if (cmd[i])
                i += 2; /* 跳过结束 ^^ / %% */
            continue;
        }

        /* 指针移动 & 跳转 */
        if (c == '>') {
            if (e->ptr < EDIE_MAX_BUF - 1) {
                if (e->ptr == e->size) {
                    e->data[e->size] = e->is_hex ? 0 : ' ';
                    e->size++;
                }
                e->ptr++;
            }
            i++;
            continue;
        }

        if (c == '<') {
            if (e->ptr > 0)
                e->ptr--;
            i++;
            continue;
        }

        if (c == ')') { /* 跳到开头 */
            e->ptr = 0;
            i++;
            continue;
        }

        if (c == '(') { /* 跳到末尾 */
            e->ptr = e->size;
            i++;
            continue;
        }

        /* |>DT!!:BODY| —— 数据驱动循环 */
        if (c == '|' && cmd[i+1] == '>') {
            i += 2;

            int dt_start = i, dt_len = 0;
            int body_start = -1, body_len = 0;
            int end_pos = -1;

            /* 找 DT 和 BODY 分界：!!: */
            while (cmd[i]) {
                if (cmd[i] == '!' && cmd[i+1] == '!' && cmd[i+2] == ':') {
                    dt_len = i - dt_start;
                    body_start = i + 3;
                    i += 3;
                    break;
                }
                i++;
            }

            if (body_start == -1)
                continue;

            /* 找结束 | */
            while (cmd[i]) {
                if (cmd[i] == '|') {
                    end_pos = i;
                    break;
                }
                i++;
            }

            if (end_pos == -1)
                continue;

            body_len = end_pos - body_start;
            if (body_len >= LOOP_BUF)
                body_len = LOOP_BUF - 1;

            char dt[LOOP_BUF], body[LOOP_BUF];

            for (int k = 0; k < dt_len; k++)
                dt[k] = cmd[dt_start + k];
            dt[dt_len] = 0;

            for (int k = 0; k < body_len; k++)
                body[k] = cmd[body_start + k];
            body[body_len] = 0;

            /* while-loop：当前指针处数据 == DT 时反复执行 BODY */
            while (1) {
                if (dt_len > 0 &&
                    (e->ptr + dt_len <= e->size) &&
                    s_cmp(&e->data[e->ptr], dt, dt_len) == 0) {
                    parse(e, body);
                } else {
                    break;
                }
            }

            i = end_pos + 1;
            continue;
        }

        /* |n...| —— 循环容器（支持嵌套） */
        if (c == '|') {
            i++;
            char *p_num = &cmd[i];
            int n = s_atoi(&p_num);
            i = (int)(p_num - cmd);

            int body_start = i;
            int depth = 1;

            while (cmd[i] && depth > 0) {
                if (cmd[i] == '|' && cmd[i+1] >= '0' && cmd[i+1] <= '9') {
                    depth++;
                    i++;
                } else if (cmd[i] == '|') {
                    depth--;
                    if (depth == 0)
                        break;
                    i++;
                } else {
                    i++;
                }
            }

            if (depth == 0) {
                int body_end = i;
                int len = body_end - body_start;
                if (len > 0 && n > 0) {
                    if (len >= LOOP_BUF)
                        len = LOOP_BUF - 1;
                    char sub_cmd[LOOP_BUF];
                    for (int k = 0; k < len; k++)
                        sub_cmd[k] = cmd[body_start + k];
                    sub_cmd[len] = 0;

                    while (n--)
                        parse(e, sub_cmd);
                }
                if (cmd[i] == '|')
                    i++;
                continue;
            }
            /* 没找到匹配的 |，当普通字符跳过 */
        }

        /* HEX 模式切换：` */
        if (c == '`') {
            e->is_hex = !e->is_hex;
            printf(e->is_hex ? "\n[MODE: HEX]\n" : "\n[MODE: TEXT]\n");
            i++;
            continue;
        }

        /* !!!DT*EQ*NE!!! —— 条件分支 */
        if (c == '!' && cmd[i+1] == '!' && cmd[i+2] == '!') {
            i += 3;

            int dt_start = i, dt_len = 0;
            int eq_start = -1, eq_len = 0;
            int ne_start = -1, ne_len = 0;
            int end_pos = -1;

            int p = i;
            while (cmd[p]) {
                if (s_cmp(&cmd[p], "!!!", 3) == 0) {
                    end_pos = p;
                    break;
                }
                if (cmd[p] == '*') {
                    if (eq_start == -1) {
                        dt_len = p - dt_start;
                        eq_start = p + 1;
                    } else if (ne_start == -1) {
                        eq_len = p - eq_start;
                        ne_start = p + 1;
                    }
                }
                p++;
            }

            if (end_pos != -1) {
                if (eq_start != -1 && eq_len == 0 && ne_start == -1)
                    eq_len = end_pos - eq_start;
                if (ne_start != -1 && ne_len == 0)
                    ne_len = end_pos - ne_start;

                int match = 0;
                if (dt_len > 0 && (e->ptr + dt_len <= e->size)) {
                    if (s_cmp(&e->data[e->ptr], &cmd[dt_start], dt_len) == 0)
                        match = 1;
                } else if (dt_len == 0) {
                    match = 1;
                }

                char sub[LOOP_BUF];

                if (match && eq_start != -1 && eq_len > 0) {
                    if (eq_len >= LOOP_BUF)
                        eq_len = LOOP_BUF - 1;
                    for (int k = 0; k < eq_len; k++)
                        sub[k] = cmd[eq_start + k];
                    sub[eq_len] = 0;
                    parse(e, sub);
                } else if (!match && ne_start != -1 && ne_len > 0) {
                    if (ne_len >= LOOP_BUF)
                        ne_len = LOOP_BUF - 1;
                    for (int k = 0; k < ne_len; k++)
                        sub[k] = cmd[ne_start + k];
                    sub[ne_len] = 0;
                    parse(e, sub);
                }

                i = end_pos + 3;
                continue;
            }
            /* 没找到结束 !!!，当普通字符处理 */
        }

        /* 保存文件：={...}，支持 $ 展开当前路径 */
        if (c == '=' && cmd[i+1] == '{') {
            i += 2;
            char target[EDIE_MAX_PATH];
            int ti = 0;

            while (cmd[i] && cmd[i] != '}' && ti < EDIE_MAX_PATH-1) {
                if (cmd[i] == '$') {
                    int k = 0;
                    while (e->path[k] && ti < EDIE_MAX_PATH-1)
                        target[ti++] = e->path[k++];
                    i++;
                } else {
                    target[ti++] = cmd[i++];
                }
            }
            target[ti] = 0;

            if (cmd[i] == '}')
                i++;

            FILE *f = fopen(target, "wb");
            if (f) {
                fwrite(e->data, 1, e->size, f);
                fclose(f);
                printf("S %dB -> %s\n", e->size, target);
            } else {
                printf("Save failed: %s\n", target);
            }
            continue;
        }

        /* 调试：P 打印附近字节 */
        if (c == 'P') {
            printf("[%d] ", e->ptr);
            for (int k = -2; k <= 2; k++) {
                int pos = e->ptr + k;
                if (pos >= 0 && pos < e->size)
                    printf("%02X ", (unsigned char)e->data[pos]);
            }
            printf("\n");
            i++;
            continue;
        }

        /* ;;n;; —— 从 stdin 读 n 个字节插入 */
        if (c == ';' && cmd[i+1] == ';') {
            i += 2;
            int n = 0;

            while (cmd[i] >= '0' && cmd[i] <= '9') {
                n = n * 10 + (cmd[i] - '0');
                i++;
            }

            if (cmd[i] == ';' && cmd[i+1] == ';') {
                i += 2;

                if (n > 0) {
                    if (n > 1024) n = 1024;
                    char buf[1024];
                    int r = (int)fread(buf, 1, n, stdin);
                    if (r > 0) {
                        if (e->size + r > EDIE_MAX_BUF)
                            r = EDIE_MAX_BUF - e->size;
                        if (r > 0) {
                            int move_len = e->size - e->ptr;
                            if (move_len < 0) move_len = 0;
                            b_mov(e->data + e->ptr + r,
                                  e->data + e->ptr,
                                  move_len);
                            memcpy(e->data + e->ptr, buf, r);
                            e->ptr += r;
                            e->size += r;
                        }
                    }
                }
                continue;
            }
        }

        /* &&NAME —— 跳转到 "NAME */
        if (c == '&' && cmd[i+1] == '&') {
            i += 2;

            int name_start = i;
            while (cmd[i] &&
                   ((cmd[i] >= 'A' && cmd[i] <= 'Z') ||
                    (cmd[i] >= 'a' && cmd[i] <= 'z') ||
                    (cmd[i] >= '0' && cmd[i] <= '9') ||
                    cmd[i] == '_')) {
                i++;
            }
            int name_len = i - name_start;

            int scan = 0;
            while (cmd[scan]) {
                if (cmd[scan] == '"') {
                    int p2 = scan + 1;
                    int k = 0;
                    while (k < name_len &&
                           cmd[p2] == cmd[name_start + k]) {
                        p2++; k++;
                    }
                    if (k == name_len &&
                        !((cmd[p2] >= 'A' && cmd[p2] <= 'Z') ||
                          (cmd[p2] >= 'a' && cmd[p2] <= 'z') ||
                          (cmd[p2] >= '0' && cmd[p2] <= '9') ||
                          cmd[p2] == '_')) {
                        i = p2;
                        break;
                    }
                }
                scan++;
            }
            continue;
        }

        /* "NAME —— 标签定义（不执行，只作为 &&NAME 的目标） */
        if (c == '"') {
            i++;
            while (cmd[i] &&
                   ((cmd[i] >= 'A' && cmd[i] <= 'Z') ||
                    (cmd[i] >= 'a' && cmd[i] <= 'z') ||
                    (cmd[i] >= '0' && cmd[i] <= '9') ||
                    cmd[i] == '_')) {
                i++;
            }
            continue;
        }

        /* 默认：跳过未知字符 */
        i++;
    }
}
/* ===== 工具函数：安全内存移动（等价 memmove） ===== */
static void b_mov(char *dst, const char *src, int n) {
    if (n <= 0) return;

    if (dst > src) {
        /* 从后往前拷贝 */
        for (int i = n - 1; i >= 0; i--)
            dst[i] = src[i];
    } else {
        /* 从前往后拷贝 */
        for (int i = 0; i < n; i++)
            dst[i] = src[i];
    }
}

/* ===== 工具函数：字符串长度 ===== */
static int s_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* ===== 工具函数：比较前 n 字节 ===== */
static int s_cmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char a = s1[i];
        unsigned char b = s2[i];
        if (a != b)
            return a - b;
        if (a == 0)
            return 0;
    }
    return 0;
}

/* ===== 工具函数：从字符串解析整数 ===== */
static int s_atoi(char **p) {
    int n = 0;
    while (**p >= '0' && **p <= '9') {
        n = n * 10 + (**p - '0');
        (*p)++;
    }
    return n;
}

/* ===== 工具函数：HEX 字符 → 数字 ===== */
static int h2b(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}
/* ===== 可视模式：实时显示 + 输入命令 ===== */
static void enter_visual_mode(Edie *e) {
    char v_cmd[128] = {0};
    int vi = 0;

    /* 进入原始终端模式 */
    system("stty -icanon -echo");

    while (1) {
        show_line_view(e);

        char c = getchar();

        /* 退出可视模式：Ctrl+V */
        if ((unsigned char)c == 22)
            break;

        /* 退格 */
        if (c == 127 || c == 8) {
            if (vi > 0)
                v_cmd[--vi] = 0;
            continue;
        }

        /* 回车：执行命令 */
        if (c == '\r' || c == '\n') {
            if (vi > 0) {
                v_cmd[vi] = 0;
                parse(e, v_cmd);
                memset(v_cmd, 0, sizeof(v_cmd));
                vi = 0;
            }
            continue;
        }

        /* 快速导航（命令为空时） */
        if (vi == 0 && c == '>') {
            if (e->ptr < e->size)
                e->ptr++;
            continue;
        }
        if (vi == 0 && c == '<') {
            if (e->ptr > 0)
                e->ptr--;
            continue;
        }

        /* 普通字符加入命令缓冲区 */
        if (c >= 32 && c <= 126 && vi < 127) {
            v_cmd[vi++] = c;
            v_cmd[vi] = 0;
        }
    }

    /* 恢复终端 */
    system("stty icanon echo");
    printf("\n");
}
/* ===== EDIE 主循环 =====
 * 支持：
 *   +{file}        加载文件
 *   [[syntax.mts]] 加载高亮规则
 *   EXIT           退出
 *   其他命令       交给 parse()
 * 每次命令执行后自动刷新窗口（show_line_view）
 */
int main() {
    Edie e = {0};
    char cmd[1024];

    while (1) {
        printf(":");
        fflush(stdout);

        /* 读取一行命令 */
        if (!fgets(cmd, sizeof(cmd), stdin))
            break;

        int len = s_len(cmd);
        if (len > 0 && cmd[len - 1] == '\n')
            cmd[len - 1] = 0;

        /* ===== EXIT ===== */
        if (strcmp(cmd, "EXIT") == 0)
            break;

        /* ===== 加载 MTS：[[path]] ===== */
        if (strncmp(cmd, "[[", 2) == 0) {
            char *end = strstr(cmd, "]]");
            if (end) {
                char path[EDIE_MAX_PATH];
                int L = end - (cmd + 2);
                if (L >= EDIE_MAX_PATH) L = EDIE_MAX_PATH - 1;
                strncpy(path, cmd + 2, L);
                path[L] = 0;
                edie_load_mts(path);
            }
            show_line_view(&e);
            continue;
        }

        /* ===== 加载文件：+{path} ===== */
        if (cmd[0] == '+') {
            char *p = strchr(cmd, '{');
            char *q = strchr(cmd, '}');
            if (p && q && q > p) {
                int L = q - p - 1;
                if (L >= EDIE_MAX_PATH) L = EDIE_MAX_PATH - 1;
                strncpy(e.path, p + 1, L);
                e.path[L] = 0;

                FILE *f = fopen(e.path, "rb");
                if (f) {
                    e.size = fread(e.data, 1, EDIE_MAX_BUF, f);
                    fclose(f);
                    printf("Loaded %dB from %s\n", e.size, e.path);
                } else {
                    printf("New file: %s\n", e.path);
                    e.size = 0;
                    e.ptr = 0;
                }
            }
            show_line_view(&e);
            continue;
        }

        /* ===== 普通命令：交给 parse() ===== */
        parse(&e, cmd);

        /* ===== 执行完命令后刷新窗口 ===== */
        show_line_view(&e);
    }

    return 0;
}
