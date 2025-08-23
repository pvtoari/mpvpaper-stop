#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#include "../include/constants.h"
#include <cJSON.h>

typedef struct {
    bool verbose;
    bool fork_process;
    char* mpvpaper_socket_path;
    int mpvpaper_socket_fd;
    char* hyprland_socket_path;
    int hyprland_socket_fd;
    int socket_wait_time;
    int polling_period;
} config_t;

void print_help(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose          Enables verbose output\n");
    printf("  -f, --fork             Forks the process\n");
    printf("  -p, --socket-path PATH Path to the mpvpaper socket (default: /tmp/mpvsocket)\n");
    printf("  -w, --socket-wait-time TIME Wait time for the socket in milliseconds (default: 5000)\n");
    printf("  -t, --period TIME      Polling period in milliseconds (default: 1000)\n");
    printf("  -h, --help             Shows this help message\n");
}

void log_verbose(const char* message, const config_t* config) {
    if (!config->verbose) return;

    printf("%ld: %s\n", time(NULL), message);
}

char* get_hyprctl_socket_path() {
    char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
        fprintf(stderr, "XDG_RUNTIME_DIR is not set\n");
        exit(EXIT_FAILURE);
    }

    char* hyprland_instance_signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!hyprland_instance_signature) {
        fprintf(stderr, "HYPRLAND_INSTANCE_SIGNATURE is not set\n");
        exit(EXIT_FAILURE);
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", xdg_runtime_dir, hyprland_instance_signature);
    
	bool xdg_access_failed = access(path, F_OK) != 0; 
	if(!xdg_access_failed) return strdup(path); 

	fprintf(stderr, "error: hyprland socket at %s not found, fallbacking to /tmp/hypr/\n", path);
	
	snprintf(path, sizeof(path), "/tmp/hypr/%s/.socket.sock", hyprland_instance_signature);
	bool tmp_access_failed = access(path, F_OK) != 0;
    if (tmp_access_failed) {
        fprintf(stderr, "error: hyprland socket path %s does not exist\n", path);
        exit(EXIT_FAILURE);
    }

    return strdup(path);
}

void init_config(config_t* config) {
    config->verbose = false;
    config->fork_process = false;
    config->mpvpaper_socket_path = DEFAULT_MPVPAPER_SOCKET_PATH;
    config->mpvpaper_socket_fd = -1;
    config->hyprland_socket_path = get_hyprctl_socket_path();
    config->hyprland_socket_fd = -1;
    config->socket_wait_time = DEFAULT_MPVPAPER_SOCKET_WAIT_TIME;
    config->polling_period = DEFAULT_PERIOD;
}

void wait_for_socket(const char *socket_path, const config_t* config) {
    int elapsed = 0;
    while (elapsed < config->socket_wait_time * 1000) {
        const int interval = 100000;
        if (access(socket_path, F_OK) == 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Socket %s is available", socket_path);
            log_verbose(msg, config);

            return;
        }

        char msg[256];
        snprintf(msg, sizeof(msg), "Socket %s not available, sleeping...", socket_path);
        log_verbose(msg, config);

        usleep(interval);
        elapsed += interval;
    }

    fprintf(stderr, "Socket %s not available after waiting %d ms\n", socket_path, config->socket_wait_time);
    exit(EXIT_FAILURE);
}


int initialize_socket(const char* socket_path) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket error");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("Connection to socket error");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

char* send_to_socket(const char* command, int* socket_fd, const char* socket_path, const bool reconnect) {
    if (reconnect) {
        close(*socket_fd);
        *socket_fd = initialize_socket(socket_path);
    }

    char buffer [4096] = {0};
    char* response = NULL;

    if (write(*socket_fd, command, strlen(command)) < 0) {
        perror("Write to socket error");
        close(*socket_fd);

        return NULL;
    }

    const ssize_t n = read(*socket_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        response = strdup(buffer);
    }

    return response;
}

char* send_to_mpv_socket(const char* command, config_t* config) {
    return send_to_socket(command, &config->mpvpaper_socket_fd, config->mpvpaper_socket_path, false);
}

char* send_to_hyprland_socket(const char* command, config_t* config) {
    return send_to_socket(command, &config->hyprland_socket_fd, config->hyprland_socket_path, true);
}

int query_windows(config_t* config) {
    char* json_str = send_to_hyprland_socket(QUERY_HYPRLAND_SOCKET_ACTIVE_WORKSPACE, config);
    if (!json_str) {
        fprintf(stderr, "Failed to query active workspace\n");
        return -1;
    }

    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        fprintf(stderr, "Failed to parse JSON\n");
        return -1;
    }

    cJSON* json_windows = cJSON_GetObjectItemCaseSensitive(json, "windows");
    int windows = cJSON_IsNumber(json_windows) ? json_windows->valueint : 0;

    cJSON_Delete(json);

    return windows;
}

bool query_pause_status(config_t* config) {
    char* json_str = send_to_mpv_socket(QUERY_MPVPAPER_SOCKET_PAUSE_PROPERTY, config);

    if (!json_str) {
        fprintf(stderr, "Failed to query pause status\n");
        return false;
    }

    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        fprintf(stderr, "Failed to parse JSON\n");
        return false;
    }

    cJSON* json_data = cJSON_GetObjectItemCaseSensitive(json, "data");
    bool paused = cJSON_IsBool(json_data) ? cJSON_IsTrue(json_data) : false;

    cJSON_Delete(json);

    return paused;
}

void resume_mpv(config_t* config) {
    log_verbose("Resuming", config);
    char* response = send_to_mpv_socket(SET_MPVPAPER_SOCKET_RESUME, config);

    if (response) free(response);
}

void pause_mpv(config_t* config) {
    log_verbose("Pausing", config);
    char* response = send_to_mpv_socket(SET_MPVPAPER_SOCKET_PAUSE, config);

    if (response) free(response);
}

void update_mpv_state(config_t* config) {
	static int last_windows = -1;
	static bool last_paused = false;

    int windows = query_windows(config);
    if (windows < 0) return;

    bool is_paused = query_pause_status(config);

	if(windows == last_windows && is_paused == last_paused) return;

	last_windows = windows;
	last_paused = is_paused;

    char message[64];
    snprintf(message, sizeof(message), "{windows: %d, paused: %d}", windows, is_paused);
    log_verbose(message, config);

    if (windows == 0 && is_paused) {
        resume_mpv(config);
    } else if (windows > 0 && !is_paused) {
        pause_mpv(config);
    }
}

void fork_if(const bool flag) {
    if (!flag) return;

    int pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        perror("Setsid failed");
        exit(EXIT_FAILURE);
    }
}

void validate_period(int period) {
    if (period <= 0) {
        fprintf(stderr, "Period must be greater than 0\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    int opt;
    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"period", required_argument, NULL, 't'},
        {"socket-path", required_argument, NULL, 'p'},
        {"fork", no_argument, NULL, 'f'},
        {"verbose", no_argument, NULL, 'v'},
        {"socket-wait-time", required_argument, NULL, 'w'},
        {0, 0, 0, 0}
    };

    config_t config;
    init_config(&config);

    while ((opt = getopt_long(argc, argv, "hvfp:t:w:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                return 0;
            case 'f':
                config.fork_process = true;
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'p':
                config.mpvpaper_socket_path = optarg;
                break;
            case 't':
                config.polling_period = atoi(optarg);
                break;
            case 'w':
                config.socket_wait_time = atoi(optarg);
                break;
            default:
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }

    validate_period(config.polling_period);
    wait_for_socket(config.mpvpaper_socket_path, &config);
    fork_if(config.fork_process);
    config.mpvpaper_socket_fd = initialize_socket(config.mpvpaper_socket_path);
    config.hyprland_socket_fd = initialize_socket(config.hyprland_socket_path);

    log_verbose("Starting monitoring loop", &config);

    while (1) {
        update_mpv_state(&config);
        usleep(config.polling_period*1000);
    }
}
