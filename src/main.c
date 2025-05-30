#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/inotify.h>
#include <errno.h>
#include <string.h>

#include "../include/constants.h"
#include <cJSON.h>

bool verbose = false, fork_process = false;

typedef enum {
    STATE_PAUSED,
    STATE_PLAYING
} player_state;

void log_verbose(const char* message) {
    if (verbose) printf("%ld: %s\n", time(NULL), message);
}

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

void wait_for_socket(const char *socket_path, int wait_time) {
    int elapsed = 0;
    while (elapsed < wait_time * 1000) {
        const int interval = 100000;
        if (access(socket_path, F_OK) == 0) {
            log_verbose("Socket available");
            return;
        }

        log_verbose("Socket not available, sleeping...");
        usleep(interval);
        elapsed += interval;
    }

    fprintf(stderr, "Socket %s not available after waiting %d ms\n", socket_path, wait_time);
    exit(EXIT_FAILURE);
}


void validate_period(int period) {
    if (period <= 0) {
        fprintf(stderr, "Period must be greater than 0\n");
        exit(EXIT_FAILURE);
    }
}

char* run_command_string(const char* command) {
    FILE* fp = popen(command, "r");
    if (!fp) return NULL;

    char buffer[512];
    char* output = NULL;
    size_t output_len = 0;

    output = malloc(1);
    if (!output) {
        pclose(fp);
        return NULL;
    }

    output[0] = '\0';
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t buffer_len = strlen(buffer);
        char* new_output = realloc(output, output_len + buffer_len + 1);
        if (!new_output) {
            free(output);
            pclose(fp);
            return NULL;
        }

        output = new_output;
        strcpy(output + output_len, buffer);
        output_len += buffer_len;
    }

    pclose(fp);

    return output;
}

char* send_mpv_command(const char* socket_path, const char* command) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket error");
        return NULL;
    }

    char buffer [4096] = {0};
    char* response = NULL;
    struct sockaddr_un addr = {0};

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("Connection to socket error");
        close(sockfd);
        return NULL;
    }

    if (write(sockfd, command, strlen(command)) < 0) {
        perror("Write to socket error");
        close(sockfd);
        return NULL;
    }

    ssize_t n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        response = strdup(buffer);
    }

    close(sockfd);

    return response;
}

int queryWindows() {
    char* json_str = run_command_string(QUERY_ACTIVE_WORKSPACE_COMMAND);
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

bool queryPauseStatus(const char* socket_path) {
    char* json_str = send_mpv_command(socket_path, QUERY_SOCKET_PAUSE_PROPERTY);

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

void resume_mpv(const char* socket_path) {
    log_verbose("Resuming");
    char* response = send_mpv_command(socket_path, SET_SOCKET_RESUME);

    if (response) free(response);
}

void pause_mpv(const char* socket_path) {
    log_verbose("Pausing");
    char* response = send_mpv_command(socket_path, SET_SOCKET_PAUSE);

    if (response) free(response);
}

void update_state(const char* socket_path, player_state* current_state) {
    int windows = queryWindows();

    if (windows < 0) return;

    bool is_paused = queryPauseStatus(socket_path);
    char message[64];
    snprintf(message, sizeof(message), "{windows: %d, paused: %d}", windows, is_paused);
    log_verbose(message);

    if (windows == 0 && is_paused) {
        resume_mpv(socket_path);
        *current_state = STATE_PLAYING;
    } else if (windows > 0 && !is_paused) {
        pause_mpv(socket_path);
        *current_state = STATE_PAUSED;
    }
}

void fork_if(bool flag) {
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

int main(int argc, char **argv) {
    int opt;
    char* socket_path = DEFAULT_SOCKET_PATH;
    int period = DEFAULT_PERIOD;
    int socket_wait_time = DEFAULT_SOCKET_WAIT_TIME;

    struct option long_options[] = {
        {"verbose", no_argument, NULL, 'v'},
        {"fork", no_argument, NULL, 'f'},
        {"socket-path", required_argument, NULL, 'p'},
        {"socket-wait-time", required_argument, NULL, 'w'},
        {"period", required_argument, NULL, 't'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "vfp:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                return 0;
            case 'v':
                verbose = true;
                break;
            case 'f':
                fork_process = true;
                break;
            case 'p':
                socket_path = optarg;
                break;
            case 't':
                period = atoi(optarg);
                break;
            case 'w':
                socket_wait_time = atoi(optarg);
                period = atoi(optarg);
                break;
            default:
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }

    fork_if(fork_process);
    wait_for_socket(socket_path, socket_wait_time);
    validate_period(period);

    player_state current_state = STATE_PAUSED;
    log_verbose("Starting monitoring loop");

    while (1) {
        update_state(socket_path, &current_state);
        usleep(period*1000);
    }
}
