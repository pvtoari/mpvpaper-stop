#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#include "../include/constants.h"
#include <cJSON.h>

typedef struct {
    bool verbose;
    bool fork_process;
    bool do_pywal;
    bool do_matugen;
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
    printf("  -c <color_backend>     Chooses color backend (pywal or matugen)\n");
    printf("  --pywal	         Runs pywal on pause\n");
    printf("  --matugen              Runs matugen on pause\n");
    printf("  -h, --help             Shows this help message\n");
}

void log_verbose(const char* message, const config_t* config) {
    if (!config->verbose) return;

    printf("%ld: %s\n", time(NULL), message);
}

char* get_hyprctl_socket_path() {
    char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
        fprintf(stderr, "error: XDG_RUNTIME_DIR is not set\n");
        exit(EXIT_FAILURE);
    }

    char* hyprland_instance_signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!hyprland_instance_signature) {
        fprintf(stderr, "error: HYPRLAND_INSTANCE_SIGNATURE is not set\n");
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

    fprintf(stderr, "error: socket %s not available after waiting %d ms\n", socket_path, config->socket_wait_time);
    exit(EXIT_FAILURE);
}


int initialize_socket(const char* socket_path) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("error: socket error");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("error: connection to socket error");
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
        perror("error: write to socket error");
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
        fprintf(stderr, "error: failed to query active workspace\n");
        return -1;
    }

    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        fprintf(stderr, "error: failed to parse JSON\n");
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
        fprintf(stderr, "error: failed to query pause status\n");
        return false;
    }

    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        fprintf(stderr, "error: failed to parse JSON\n");
        return false;
    }

    cJSON* json_data = cJSON_GetObjectItemCaseSensitive(json, "data");
    bool paused = cJSON_IsBool(json_data) ? cJSON_IsTrue(json_data) : false;

    cJSON_Delete(json);

    return paused;
}

void create_temp_dir() {
	if (mkdir(TEMP_DIR, 0755) == -1) {
        if (errno != EEXIST) {
            perror("error: failed to create TEMP_DIR");
            exit(EXIT_FAILURE);
        }
    }
}

void validate_colors(config_t *config, char *mode) {
  FILE *fp;
  if (strcmp(mode, "pywal") == 0) {
	  fp = popen("wal -v", "r");
  } else if (strcmp(mode, "matugen") == 0) {
    fp = popen("matugen --version", "r");
  }

	if (fp == NULL) {
		perror("error: unable to open process");
		exit(EXIT_FAILURE);
	}

	int status = WEXITSTATUS(pclose(fp));
	if (status != EXIT_SUCCESS) {
		perror(("error: cannot run %s", mode));
		exit(EXIT_FAILURE);
	}

	log_verbose(("%s is available", mode), config);
	create_temp_dir();

	char *json_str = send_to_mpv_socket(SET_MPVPAPER_SCREENSHOT_DIR, config);
	if(!json_str) {
		perror("error: failed to set temp screenshot dir");
		exit(EXIT_FAILURE);
	}
	
	cJSON *json = cJSON_Parse(json_str);
	free(json_str);
	
	cJSON *json_error = cJSON_GetObjectItemCaseSensitive(json, "error");
	if(strcmp(json_error->valuestring, "success") != 0) {
		perror("error: failed to set temp screenshot dir");
		exit(EXIT_FAILURE);
	}

	cJSON_Delete(json);
	log_verbose("screenshot directory successfully set", config);
}

void run_colors(config_t *config, char *mode) {
	log_verbose("attempting to perform screenshot...", config);
	char *json_str = send_to_mpv_socket(QUERY_MPVPAPER_SOCKET_DO_SCREENSHOT, config);
	if(!json_str) {
		perror("error: failed to perform a screenshot");
		exit(EXIT_FAILURE);
	}

	cJSON *json = cJSON_Parse(json_str);
	free(json_str);

	if(!json) {
		perror("error: failed to parse JSON");
		exit(EXIT_FAILURE);
	}

	cJSON *json_data = cJSON_GetObjectItemCaseSensitive(json, "data");
	cJSON *json_error = cJSON_GetObjectItemCaseSensitive(json, "error");
	if(strcmp(json_error->valuestring, "success") != 0) {
		perror("error: failed to perform a screenshot");
		exit(EXIT_FAILURE);
	}
	
	cJSON *json_filename = cJSON_GetObjectItemCaseSensitive(json_data, "filename");
	if (!json_filename || !cJSON_IsString(json_filename)) {
	    log_verbose("screenshot already exists, skipping", config);
	    cJSON_Delete(json);
	    return;
	}
	
	char cmd_buf[256];
  if (strcmp(mode, "pywal") == 0) {
	  snprintf(cmd_buf, sizeof(cmd_buf), "wal -i %s >> %s/last_wal.log 2>&1", json_filename->valuestring, TEMP_DIR);
  } else if (strcmp(mode, "matugen") == 0) {
    snprintf(cmd_buf, sizeof(cmd_buf), "matugen image %s -m dark >> %s/last_matugen.loc 2>&1", json_filename->valuestring, TEMP_DIR);
  }
	log_verbose(("running %s command:", mode), config);
	log_verbose(cmd_buf, config);

	FILE *fp = popen(cmd_buf, "r");
	if(!fp) {
		perror("error: popen");
		exit(EXIT_FAILURE);
	}
	
	int status = WEXITSTATUS(pclose(fp));
	if (status != EXIT_SUCCESS) {
		perror(("error: failed to run %s", mode));
		exit(EXIT_FAILURE);
	}

	log_verbose(("%s ran succesfully", mode), config);
	log_verbose("removing screenshot:", config);
	log_verbose(json_filename->valuestring, config);

	if(remove(json_filename->valuestring) != 0) {
		perror("error: cannot remove last screenshot");
		exit(EXIT_FAILURE);
	}
	
	cJSON_Delete(json);
}

void resume_mpv(config_t* config) {
    log_verbose("Resuming", config);
    char* response = send_to_mpv_socket(SET_MPVPAPER_SOCKET_RESUME, config);

    if (response) free(response);
}

void pause_mpv(config_t* config) {
    log_verbose("Pausing", config);
    char* response = send_to_mpv_socket(SET_MPVPAPER_SOCKET_PAUSE, config);

    if (config->do_pywal == true) run_colors(config, "pywal");
    if (config->do_matugen == true) run_colors(config, "matugen");
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
        perror("error: fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        perror("error: setsid failed");
        exit(EXIT_FAILURE);
    }
}

void validate_period(int period) {
    if (period <= 0) {
        fprintf(stderr, "error: period must be greater than 0\n");
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
        {"pywal", no_argument, NULL, '_pywal'},
        {"matugen", no_argument, NULL, '_matugen'},
        {"socket-wait-time", required_argument, NULL, 'w'},
        {0, 0, 0, 0}
    };

    config_t config;
    init_config(&config);
    config.do_pywal = false;
    config.do_matugen = false;

    while ((opt = getopt_long(argc, argv, "hvfp:c:t:w:", long_options, NULL)) != -1) {
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
            // This exists purely for allowing options in the -c short option
            case 'c':
                if(strcmp(optarg, "pywal") == 0) config.do_pywal = true;
                if(strcmp(optarg, "matugen") == 0) config.do_matugen = true;
                break;
            case '_pywal': // For --pywal
                config.do_pywal = true;
                break;
            case '_matugen': // For --matugen
                config.do_matugen = true;
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

    if (config.do_pywal == true) validate_colors(&config, "pywal");
    if (config.do_matugen == true) validate_colors(&config, "matugen");

    log_verbose("Starting monitoring loop", &config);

    while (1) {
        update_mpv_state(&config);
        usleep(config.polling_period*1000);
    }
}
