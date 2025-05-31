#ifndef CONSTANTS_H
#define CONSTANTS_H

#define DEFAULT_PERIOD 1000
#define DEFAULT_MPVPAPER_SOCKET_PATH "/tmp/mpvsocket"
#define DEFAULT_MPVPAPER_SOCKET_WAIT_TIME 5000
#define QUERY_HYPRLAND_SOCKET_ACTIVE_WORKSPACE "j/activeworkspace"
#define QUERY_MPVPAPER_SOCKET_PAUSE_PROPERTY "{ \"command\": [\"get_property\", \"pause\"]}\n"
#define SET_MPVPAPER_SOCKET_PAUSE "{ \"command\": [\"set_property\", \"pause\", true] }\n"
#define SET_MPVPAPER_SOCKET_RESUME "{ \"command\": [\"set_property\", \"pause\", false] }\n"

#endif
