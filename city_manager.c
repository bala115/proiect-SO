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

    // Cream directorul daca nu exista
    mkdir(district, 0750);

    // Pregatim structura
    Report r;
    memset(&r, 0, sizeof(Report));

    // CITIRE DE LA TASTATURA
    printf("--- Introducere date raport nou ---\n");
    printf("Categorie (ex: road, lighting): ");
    scanf("%29s", r.category); // Citim un singur cuvant

    printf("Severitate (1-minor, 2-moderate, 3-critical): ");
    scanf("%d", &r.severity);

    printf("Latitudine: ");
    scanf("%f", &r.lat);

    printf("Longitudine: ");
    scanf("%f", &r.lon);

    // Curatam buffer-ul de la tastatura pentru a folosi fgets
    while (getchar() != '\n'); 

    printf("Descriere problema: ");
    fgets(r.description, 199, stdin);
    r.description[strcspn(r.description, "\n")] = 0; // Eliminam newline-ul de la final

    // Completam restul campurilor automate
    r.id = (int)time(NULL);
    strncpy(r.inspector, user, 49);
    r.timestamp = time(NULL);

    // SCRIERE IN FISIER BINAR
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd < 0) { perror("Eroare deschidere reports.dat"); return; }
    
    write(fd, &r, sizeof(Report));
    close(fd);

    // LOGGING
    char log_path[256];
    sprintf(log_path, "%s/logged_district", district);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd >= 0) {
        char log_entry[512];
        sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: ADD_REPORT ID:%d\n", 
                time(NULL), role, user, r.id);
        write(log_fd, log_entry, strlen(log_entry));
        close(log_fd);
    }
    
    printf("\n[OK] Raportul cu ID %d a fost salvat cu succes.\n", r.id);
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

void view_report(const char *district, int target_id) {
    char path[256];
    sprintf(path, "%s/reports.dat", district);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Nu s-au gasit rapoarte in %s.\n", district);
        return;
    }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == target_id) {
            printf("\n--- Detalii Raport %d ---\n", r.id);
            printf("Inspector: %s\n", r.inspector);
            printf("Coordonate: Lat %f, Lon %f\n", r.lat, r.lon);
            printf("Categorie: %s | Severitate: %d\n", r.category, r.severity);
            
            // Formatarea timestamp-ului
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&r.timestamp));
            printf("Data: %s\n", time_str);
            printf("Descriere: %s\n-------------------------\n", r.description);
            
            found = 1;
            break;
        }
    }
    
    if (!found) printf("Raportul cu ID %d nu a fost gasit.\n", target_id);
    close(fd);
}

void remove_report(const char *district, int target_id, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) {
        printf("Acces interzis: Doar rolul de 'manager' poate sterge rapoarte!\n");
        return;
    }

    char path[256];
    sprintf(path, "%s/reports.dat", district);
    int fd = open(path, O_RDWR);
    if (fd < 0) { perror("Eroare la deschidere"); return; }

    Report r;
    int found = 0;
    off_t pos = 0;

    // Cautam raportul
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == target_id) {
            found = 1;
            off_t current_pos = lseek(fd, 0, SEEK_CUR); // Pozitia dupa raportul gasit
            off_t write_pos = current_pos - sizeof(Report); // Pozitia raportului care trebuie sters
            
            Report temp;
            // Shiftam restul rapoartelor peste cel sters
            while (read(fd, &temp, sizeof(Report)) == sizeof(Report)) {
                lseek(fd, write_pos, SEEK_SET);
                write(fd, &temp, sizeof(Report));
                write_pos += sizeof(Report);
                lseek(fd, write_pos + sizeof(Report), SEEK_SET); // Ne mutam inapoi pentru urmatoarea citire
            }
            
            // Taiem capatul fisierului cu o marime de raport
            struct stat st;
            fstat(fd, &st);
            ftruncate(fd, st.st_size - sizeof(Report));
            break;
        }
    }

    close(fd);

    if (found) {
        printf("Raportul %d a fost sters.\n", target_id);
        // LOGGING
        char log_path[256];
        sprintf(log_path, "%s/logged_district", district);
        int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        char log_entry[256];
        sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: REMOVE_REPORT ID:%d\n", time(NULL), role, user, target_id);
        write(log_fd, log_entry, strlen(log_entry));
        close(log_fd);
    } else {
        printf("Raportul nu a fost gasit.\n");
    }
}

void update_threshold(const char *district, int new_value, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) {
        printf("Acces interzis: Doar managerii pot actualiza pragul!\n");
        return;
    }

    char path[256];
    sprintf(path, "%s/district.cfg", district);

    // Daca fisierul exista, verificam permisiunile (trebuie sa fie 640)
    struct stat st;
    if (stat(path, &st) == 0) {
        if ((st.st_mode & 0777) != 0640) {
            printf("Eroare de securitate: Fisierul district.cfg nu are permisiunile 640. Avortam operatiunea.\n");
            return;
        }
    }

    // O_TRUNC sterge continutul vechi inainte sa il scrie pe cel nou
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd < 0) { perror("Eroare la crearea cfg"); return; }
    
    chmod(path, 0640); // Fortam permisiunile la 640
    
    char buffer[32];
    int len = sprintf(buffer, "%d\n", new_value);
    write(fd, buffer, len);
    close(fd);

    printf("Prag actualizat cu succes la valoarea %d.\n", new_value);

    // LOGGING
    char log_path[256];
    sprintf(log_path, "%s/logged_district", district);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    char log_entry[256];
    sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: UPDATE_THRESHOLD Val:%d\n", time(NULL), role, user, new_value);
    write(log_fd, log_entry, strlen(log_entry));
    close(log_fd);
}

// --- FUNCTII GENERATE DE AI (pentru filter) ---
int parse_condition(const char *input, char *field, char *op, char *value) {
    // Parseeaza string-uri de genul "severity:>=:2"
    if (sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3) return 1;
    return 0;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int v = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == v;
        if (strcmp(op, ">=") == 0) return r->severity >= v;
        if (strcmp(op, "<=") == 0) return r->severity <= v;
    }
    else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
    }
    return 0; // Nu se potriveste
}

void filter_reports(const char *district, int argc, char *argv[]) {
    char path[256];
    sprintf(path, "%s/reports.dat", district);
    int fd = open(path, O_RDONLY);
    if (fd < 0) { printf("Nu s-au gasit rapoarte.\n"); return; }

    Report r;
    int gasit = 0;
    printf("\n--- Rezultate Filtrare ---\n");
    
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int all_match = 1;
        // Incepem verificarea conditiilor (argv[7] incolo)
        for (int i = 7; i < argc; i++) {
            char field[50], op[5], value[50];
            if (parse_condition(argv[i], field, op, value)) {
                if (!match_condition(&r, field, op, value)) {
                    all_match = 0;
                    break;
                }
            }
        }
        if (all_match) {
            printf("ID: %d | Cat: %s | Sev: %d | Insp: %s\n", r.id, r.category, r.severity, r.inspector);
            gasit++;
        }
    }
    
    if (gasit == 0) printf("Niciun raport nu corespunde filtrelor.\n");
    close(fd);
}

int main(int argc, char *argv[]) {
    char *role = NULL, *user = NULL, *cmd = NULL, *dist = NULL;

    // Parsare argumente
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            role = argv[++i];
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user = argv[++i];
        } else if (cmd == NULL) {
            cmd = argv[i];
        } else if (dist == NULL) {
            dist = argv[i];
        }
    }

    // Verificam daca avem datele minime
   // Creare Symlink (link simbolic) conform cerintei
    char symlink_name[256];
    sprintf(symlink_name, "active_reports-%s", dist);
    char target_path[256];
    sprintf(target_path, "%s/reports.dat", dist);
    
    unlink(symlink_name); // Il stergem daca exista deja ca sa-l actualizam
    symlink(target_path, symlink_name); // Cream link-ul catre fisierul .dat

    // Executia comenzilor
    if (strcmp(cmd, "add") == 0) {
        add_report(dist, user, role);
    } else if (strcmp(cmd, "list") == 0) {
        list_reports(dist);
    } else if (strcmp(cmd, "view") == 0) {
        if (argc < 8) { printf("Eroare: Lipseste ID-ul raportului.\n"); return 1; }
        int id = atoi(argv[7]);
        view_report(dist, id);
    } else if (strcmp(cmd, "remove_report") == 0) {
        if (argc < 8) { printf("Eroare: Lipseste ID-ul raportului.\n"); return 1; }
        int id = atoi(argv[7]);
        remove_report(dist, id, role, user);
    } else if (strcmp(cmd, "update_threshold") == 0) {
        if (argc < 8) { printf("Eroare: Lipseste valoarea pragului.\n"); return 1; }
        int val = atoi(argv[7]);
        update_threshold(dist, val, role, user);
    } else if (strcmp(cmd, "filter") == 0) {
        if (argc < 8) { printf("Eroare: Lipseste cel putin o conditie de filtrare.\n"); return 1; }
        filter_reports(dist, argc, argv);
    } else {
        printf("Comanda necunoscuta: %s\n", cmd);
    }

    return 0;
}
