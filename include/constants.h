#ifndef CONSTANTS_H
#define CONSTANTS_H

#define DEFAULT_PERIOD 1000
#define DEFAULT_MPVPAPER_SOCKET_PATH "/tmp/mpvsocket"
#define DEFAULT_MPVPAPER_SOCKET_WAIT_TIME 5000
#define TEMP_DIR "/tmp/mpvpaper-stop"

#define QUERY_HYPRLAND_SOCKET_ACTIVE_WORKSPACE "j/activeworkspace"

#define QUERY_MPVPAPER_SOCKET_PAUSE_PROPERTY "{ \"command\": [\"get_property\", \"pause\"]}\n"
#define QUERY_MPVPAPER_SOCKET_DO_SCREENSHOT "{ \"command\": [\"screenshot\"] }\n"
#define SET_MPVPAPER_SOCKET_PAUSE "{ \"command\": [\"set_property\", \"pause\", true] }\n"
#define SET_MPVPAPER_SOCKET_RESUME "{ \"command\": [\"set_property\", \"pause\", false] }\n"
#define SET_MPVPAPER_SCREENSHOT_DIR "{ \"command\": [\"set_property\", \"screenshot-dir\", \""TEMP_DIR"\"] }\n"

#endif
