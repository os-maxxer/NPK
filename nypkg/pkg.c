#include <nyx/graphics.h>
#include <nyx/npx.h>
#include <nyx/nofs.h>
#include <nyx/vfs.h>
#include <stdbool.h>

#define LINE_H 16
#define COLS 60
#define LINE_BUF 256

static char lines[32][COLS];
static int line_count;
static bool dirty;

static char line_buf[LINE_BUF];
static int line_pos;

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void clear_screen(void) {
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < COLS; j++)
            lines[i][j] = ' ';
    line_count = 0;
}

static void add_line(const char *s) {
    if (line_count >= 32) return;
    int i = 0;
    while (*s && i < COLS - 1)
        lines[line_count][i++] = *s++;
    while (i < COLS - 1)
        lines[line_count][i++] = ' ';
    lines[line_count][COLS - 1] = '\0';
    line_count++;
}

static void uint32_to_str(uint32_t n, char *buf) {
    char tmp[12];
    int ti = 0;
    if (n == 0) { tmp[ti++] = '0'; }
    while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
    int bi = 0;
    while (ti > 0) buf[bi++] = tmp[--ti];
    buf[bi] = '\0';
}

static void prompt(void) {
    add_line("npxpk> ");
}

static void print_help(void) {
    add_line("NYPKG v1.0 - Nyx Package Manager");
    add_line("");
    add_line("Commands:");
    add_line("  help                  - show this help");
    add_line("  search [term]         - list installed packages");
    add_line("  install <file>        - install a .npx package from disk");
    add_line("  purge <name>          - uninstall a package by name");
    add_line("  clear                 - clear screen");
    add_line("");
}

static void cmd_search(const char *arg) {
    (void)arg;
    char names[NPX_MAX_APPS][NPX_NAME_LEN];
    int count = npx_list_installed(names, NPX_MAX_APPS);

    if (count == 0) {
        add_line("No packages installed.");
        return;
    }

    char buf[64];
    int bi;

    add_line("Installed packages:");
    for (int i = 0; i < count; i++) {
        int slot = npx_find_slot(names[i]);
        bi = 0;
        buf[bi++] = ' ';
        buf[bi++] = ' ';
        const char *s = names[i];
        while (*s && bi < 40) buf[bi++] = *s++;
        const char *p = " (slot ";
        while (*p && bi < 60) buf[bi++] = *p++;
        char slot_str[4];
        uint32_to_str((uint32_t)slot, slot_str);
        const char *q = slot_str;
        while (*q && bi < 60) buf[bi++] = *q++;
        buf[bi++] = ')';
        buf[bi] = '\0';
        add_line(buf);
    }

    if (count == 1)
        add_line("1 package total.");
    else {
        char total[32];
        uint32_to_str((uint32_t)count, total);
        bi = 0;
        const char *p = total;
        while (*p) buf[bi++] = *p++;
        const char *q = " packages total.";
        while (*q && bi < 60) buf[bi++] = *q++;
        buf[bi] = '\0';
        add_line(buf);
    }
}

static void cmd_install(const char *path) {
    if (!path || !*path) {
        add_line("Usage: install <filename>");
        return;
    }

    add_line("Reading package from disk...");

    int fd = vfs_open(path);
    if (fd < 0) {
        add_line("Error: file not found.");
        return;
    }

    int size = vfs_get_size(fd);
    if (size <= 0 || size > 65536) {
        add_line("Error: invalid or too large.");
        return;
    }

    uint8_t buf[65536];
    int n = vfs_read(fd, buf, (uint32_t)size);
    if (n <= 0) {
        add_line("Error: could not read file.");
        return;
    }

    add_line("Installing...");
    int slot = npx_install(buf, (uint32_t)n);
    if (slot < 0) {
        add_line("Error: install failed (no free slots or bad package).");
        return;
    }

    add_line("Package installed successfully!");

    char names[NPX_MAX_APPS][NPX_NAME_LEN];
    int count = npx_list_installed(names, NPX_MAX_APPS);
    char msg[64];
    int bi = 0;
    const char *p = "Installed in slot ";
    while (*p) msg[bi++] = *p++;
    char slot_str[4];
    uint32_to_str((uint32_t)slot, slot_str);
    p = slot_str;
    while (*p) msg[bi++] = *p++;
    const char *q = " (";
    while (*q) msg[bi++] = *q++;
    uint32_to_str((uint32_t)count, slot_str);
    q = slot_str;
    while (*q) msg[bi++] = *q++;
    q = " total)";
    while (*q) msg[bi++] = *q++;
    msg[bi] = '\0';
    add_line(msg);
}

static void cmd_purge(const char *name) {
    if (!name || !*name) {
        add_line("Usage: purge <appname>");
        return;
    }

    int result = npx_uninstall(name);
    if (result < 0) {
        add_line("Error: package not found.");
        return;
    }

    char msg[64];
    int bi = 0;
    const char *p = "Uninstalled: ";
    while (*p) msg[bi++] = *p++;
    p = name;
    while (*p && bi < 60) msg[bi++] = *p++;
    msg[bi] = '\0';
    add_line(msg);
}

static void cmd_execute(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    char command[32];
    int ci = 0;
    while (*cmd && *cmd != ' ' && ci < 31) {
        command[ci++] = *cmd;
        cmd++;
    }
    command[ci] = '\0';
    while (*cmd == ' ') cmd++;

    if (str_eq(command, "help")) {
        print_help();
    } else if (str_eq(command, "clear")) {
        clear_screen();
    } else if (str_eq(command, "search")) {
        cmd_search(cmd);
    } else if (str_eq(command, "install")) {
        cmd_install(cmd);
    } else if (str_eq(command, "purge")) {
        cmd_purge(cmd);
    } else {
        char msg[48];
        int bi = 0;
        const char *p = "Unknown command: ";
        while (*p) msg[bi++] = *p++;
        p = command;
        while (*p && bi < 46) msg[bi++] = *p++;
        msg[bi] = '\0';
        add_line(msg);
        add_line("Type 'help' for available commands.");
    }
}

void pkg_init(void) {
    clear_screen();
    print_help();
    prompt();
    line_pos = 0;
    dirty = true;
}

void pkg_draw(int x, int y, int w, int h) {
    if (!dirty) return;
    dirty = false;

    int rows = (h - 8) / LINE_H;
    if (rows > line_count) rows = line_count;

    graphics_fill_rect(x, y, w, h, 0xFF1E1E2E);

    for (int r = 0; r < rows; r++) {
        graphics_draw_string(x + 4, y + 4 + r * LINE_H, lines[r], 0xFF00FF00);
    }

    char prompt_line[COLS];
    int bi = 0;
    const char *p = "npxpk> ";
    while (*p) prompt_line[bi++] = *p++;
    for (int i = 0; i < line_pos; i++)
        prompt_line[bi++] = line_buf[i];
    prompt_line[bi] = '\0';
    graphics_draw_string(x + 4, y + 4 + (line_count < rows ? line_count : rows) * LINE_H, prompt_line, 0xFF00FF00);
}

void pkg_handle_key(char key) {
    if (key == '\n') {
        line_buf[line_pos] = '\0';
        cmd_execute(line_buf);
        prompt();
        line_pos = 0;
    } else if (key == '\b') {
        if (line_pos > 0) line_pos--;
    } else if (key == 0x1b) {
        line_pos = 0;
    } else if (key >= 32) {
        if (line_pos < LINE_BUF - 1)
            line_buf[line_pos++] = key;
    }
    dirty = true;
}
