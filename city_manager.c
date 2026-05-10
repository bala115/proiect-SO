#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
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

    printf("\n[OK] Raportul cu ID %d a fost salvat cu succes.\n", r.id);

    // NOTIFICARE MONITOR (FAZA 2)
    int monitor_notified = 0;
    int pid_fd = open(".monitor_pid", O_RDONLY);
    if (pid_fd >= 0) {
        char pid_buf[32] = {0};
        int bytes = read(pid_fd, pid_buf, sizeof(pid_buf) - 1);
        close(pid_fd);
        if (bytes > 0) {
            pid_t monitor_pid = atoi(pid_buf);
            // Trimitem semnalul SIGUSR1 si verificam succesul
            if (monitor_pid > 0 && kill(monitor_pid, SIGUSR1) == 0) {
                monitor_notified = 1;
            }
        }
    }

    // LOGGING
    char log_path[256];
    sprintf(log_path, "%s/logged_district", district);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd >= 0) {
        char log_entry[512];
        // 1. Logul normal cerut in faza 1
        sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: ADD_REPORT ID:%d\n", 
                time(NULL), role, user, r.id);
        write(log_fd, log_entry, strlen(log_entry));

        // 2. Logul cerut in faza 2 (rezultatul notificarii monitorului)
        if (monitor_notified) {
            sprintf(log_entry, "[%ld] SUCCESS: Monitor informed via SIGUSR1 about new report ID:%d\n", time(NULL), r.id);
        } else {
            sprintf(log_entry, "[%ld] ERROR: Monitor could not be informed of the event!\n", time(NULL));
        }
        write(log_fd, log_entry, strlen(log_entry));
        close(log_fd);
    }
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
    if (fd < 0) return;
    
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

    // Cautam raportul
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == target_id) {
            found = 1;
            off_t current_pos = lseek(fd, 0, SEEK_CUR); // Pozitia dupa raportul gasit
            off_t write_pos = current_pos - sizeof(Report); // Pozitia raportului de sters
            
            Report temp;
            // Shiftam restul rapoartelor peste cel sters
            while (read(fd, &temp, sizeof(Report)) == sizeof(Report)) {
                lseek(fd, write_pos, SEEK_SET);
                write(fd, &temp, sizeof(Report));
                write_pos += sizeof(Report);
                lseek(fd, write_pos + sizeof(Report), SEEK_SET);
            }
            
            // Taiem capatul fisierului
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
        if (log_fd >= 0) {
            char log_entry[256];
            sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: REMOVE_REPORT ID:%d\n", time(NULL), role, user, target_id);
            write(log_fd, log_entry, strlen(log_entry));
            close(log_fd);
        }
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
    struct stat st;
    if (stat(path, &st) == 0) {
        if ((st.st_mode & 0777) != 0640) {
            printf("Eroare de securitate: Fisierul district.cfg nu are permisiunile 640. Avortam operatiunea.\n");
            return;
        }
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd < 0) { perror("Eroare la crearea cfg"); return; }
    
    chmod(path, 0640);
    
    char buffer[32];
    int len = sprintf(buffer, "%d\n", new_value);
    write(fd, buffer, len);
    close(fd);

    printf("Prag actualizat cu succes la valoarea %d.\n", new_value);

    // LOGGING
    char log_path[256];
    sprintf(log_path, "%s/logged_district", district);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd >= 0) {
        char log_entry[256];
        sprintf(log_entry, "[%ld] Role: %s, User: %s, Action: UPDATE_THRESHOLD Val:%d\n", time(NULL), role, user, new_value);
        write(log_fd, log_entry, strlen(log_entry));
        close(log_fd);
    }
}

int parse_condition(const char *input, char *field, char *op, char *value) {
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
    return 0;
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

void remove_district(const char *district, const char *role) {
    if (strcmp(role, "manager") != 0) {
        printf("Acces interzis: Doar rolul de 'manager' poate sterge districte!\n");
        return;
    }

    char symlink_name[256];
    snprintf(symlink_name, sizeof(symlink_name), "active_reports-%s", district);

    pid_t pid = fork();
    if (pid < 0) {
        perror("Eroare la fork");
        return;
    }

    if (pid == 0) { 
        // Proces fiu - folosim execlp in loc de system(), conform Faza 2
        execlp("rm", "rm", "-rf", district, NULL);
        perror("Eroare la exec (rm -rf)");
        exit(1); 
    } else {
        // Părintele așteaptă
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // Daca s-a sters cu succes directorul, eliminam si symlink-ul cu unlink()
            unlink(symlink_name);
            printf("Succes: Districtul '%s' si symlink-ul sau au fost sterse.\n", district);
        } else {
            printf("Eroare: Comanda rm a esuat pentru %s\n", district);
        }
    }
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

    if (!role || !user || !cmd || !dist) {
        printf("Eroare: Argumente insuficiente.\n");
        return 1;
    }

    // Creare Symlink (link simbolic)
    char symlink_name[256];
    sprintf(symlink_name, "active_reports-%s", dist);
    char target_path[256];
    sprintf(target_path, "%s/reports.dat", dist);
    
    unlink(symlink_name); 
    symlink(target_path, symlink_name);

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
    } else if (strcmp(cmd, "remove_district") == 0) {
        remove_district(dist, role);
    } else {
        printf("Comanda necunoscuta: %s\n", cmd);
    }

    return 0;
}
