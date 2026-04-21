#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "reports.h"


void get_permissions_string(mode_t mode, char *str) {
    strcpy(str, "---------");
    if (mode & S_IRUSR) str[0] = 'r';
    if (mode & S_IWUSR) str[1] = 'w';
    if (mode & S_IXUSR) str[2] = 'x';
    if (mode & S_IRGRP) str[3] = 'r';
    if (mode & S_IWGRP) str[4] = 'w';
    if (mode & S_IXGRP) str[5] = 'x';
    if (mode & S_IROTH) str[6] = 'r';
    if (mode & S_IWOTH) str[7] = 'w';
    if (mode & S_IXOTH) str[8] = 'x';
}

void add_report(const char *district, const char *user, const char *role) {
    char path[256];
    sprintf(path, "%s/reports.dat", district);

    mkdir(district, 0750);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd < 0) { perror("Eroare open"); return; }
    chmod(path, 0664); 

    Report r;
    memset(&r, 0, sizeof(Report));
    r.id = (int)time(NULL);
    strncpy(r.inspector, user, 49);
    r.severity = 2; 
    r.timestamp = time(NULL);
    strncpy(r.category, "road", 29);
    strncpy(r.description, "Problema detectata pe teren", 199);

    write(fd, &r, sizeof(Report));
    close(fd);

    char log_path[256];
    sprintf(log_path, "%s/logged_district", district);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    char log_entry[256];
    sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: ADD\n", time(NULL), role, user);
    write(log_fd, log_entry, strlen(log_entry));
    close(log_fd);
    
    printf("Raport adaugat cu succes in %s\n", district);
}

void list_reports(const char *district) {
    char path[256];
    sprintf(path, "%s/reports.dat", district);

    struct stat st;
    if (stat(path, &st) < 0) {
        printf("Districtul %s nu are rapoarte.\n", district);
        return;
    }

    char perms[11];
    get_permissions_string(st.st_mode, perms);
    printf("File: %s | Perms: %s | Size: %ld bytes\n", path, perms, st.st_size);

    int fd = open(path, O_RDONLY);
    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        printf("ID: %d | Cat: %s | Sev: %d | Insp: %s\n", 
                r.id, r.category, r.severity, r.inspector);
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    char *role = "", *user = "", *cmd = "", *dist = "";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0) user = argv[++i];
        else if (strcmp(dist, "")==0) dist = argv[i];         else if (strmp(cmd, "")==0) cmd = argv[i]; 
    }

    if (argc < 6) {
        printf("Utilizare: ./city_manager --role <r> --user <u> <comanda> <district>\n");
        return 1;
    }
    
    cmd = argv[5];
    dist = argv[6];

    if (strcmp(cmd, "add") == 0) {
        add_report(dist, user, role);
    } else if (strcmp(cmd, "list") == 0) {
        list_reports(dist);
    }

    return 0;
}
