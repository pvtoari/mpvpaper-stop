![Preview](https://github.com/pvtoari/mpvpaper-stop/blob/master/media/preview.gif)
# mpvpaper-stop

`mpvpaper-stop` is a lightweight background utility designed to work with [mpvpaper](https://github.com/GhostNaN/mpvpaper). It automatically pauses `mpvpaper` when any window becomes visible on your active Hyprland workspace and resumes playback when no windows are displayed.

**Important Note:** Currently, `mpvpaper-stop` exclusively supports **Hyprland** due to its reliance on `hyprctl` for window detection. Future enhancements may include support for other Wayland compositors.

## How it works

`mpvpaper-stop` periodically performs the following actions:
1.  It queries Hyprland (using `hyprctl activeworkspace -j`) to determine the number of visible windows on the active workspace.
2.  It checks the current playback status of `mpvpaper` (paused or playing) via its IPC socket.
3.  If there are no windows visible and `mpvpaper` is paused, it sends a resume command.
4.  If there are one or more windows visible and `mpvpaper` is playing, it sends a pause command.

This ensures that your video wallpaper only plays when it's actually visible.

## Features
*   Automatic pause/resume of `mpvpaper` based on window visibility.
*   Configurable polling period.
*   Option to fork into the background.
*   Verbose mode for debugging.
*   Customizable path for `mpvpaper`'s IPC socket.
*   Waits for the `mpvpaper` socket to be available before starting.

## Dependencies

### Runtime:
*   **[mpvpaper](https://github.com/GhostNaN/mpvpaper)**: Must be installed and running with IPC socket enabled.
*   **Hyprland**: Required for window detection via `hyprctl`.
*   **`hyprctl`**: Command-line utility for Hyprland.

### Build-time:
*   **CMake** (version 3.5 or higher)
*   A **C Compiler** supporting C11 (e.g., GCC, Clang)
*   **Git** (for fetching cJSON library)
*   Standard C libraries

## Prerequisites: `mpvpaper` IPC Setup

Before running `mpvpaper-stop`, ensure `mpvpaper` is started with the `input-ipc-server` option. This creates a socket that `mpvpaper-stop` uses to send commands.

Example `mpvpaper` execution (add to your Hyprland config or run manually):
```bash
# For a specific monitor (e.g., DP-1)
exec mpvpaper -o "--input-ipc-server=/tmp/mpvsocket" DP-1 /path/to/your/video.mp4
```
**Note:** `mpvpaper-stop` defaults to `/tmp/mpvsocket` for the socket path. If you use a different path for `mpvpaper`, you must specify it when running `mpvpaper-stop`.

## Building `mpvpaper-stop`

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/pvtoari/mpvpaper-stop
    cd mpvpaper-stop
    ```

2.  **Configure and build using CMake:**
    A helper script `build.sh` is provided:
    ```bash
    ./build.sh
    ```
    This will create a release build in the `cmake-build-release` directory. The executable will be `cmake-build-release/mpvpaper-stop`.

    Alternatively, to build manually:
    ```bash
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .
    ```
    The executable will be `build/mpvpaper-stop`.

3.  **Manual install using CMake:**
    If you want to install it system-wide (e.g., to `/usr/local/bin`):
    ```bash
    sudo cmake --install cmake-build-release # (from project root)
    ```

## Usage

Run `mpvpaper-stop` after `mpvpaper` has started.

```bash
# Assuming mpvpaper is already running and its socket is at /tmp/mpvsocket
./mpvpaper-stop
```

If you built using `build.sh`:
```bash
./cmake-build-release/mpvpaper-stop
```

### Command-line options:

```
Usage: mpvpaper-stop [options]
Options:
  -v, --verbose          Enables verbose output
  -f, --fork             Forks the process
  -p, --socket-path PATH Path to the mpvpaper socket (default: /tmp/mpvsocket)
  -w, --socket-wait-time TIME Wait time for the socket in milliseconds (default: 5000)
  -t, --period TIME      Polling period in milliseconds (default: 1000)
  -h, --help             Shows this help message
```

**Example with custom options:**
```bash
./mpvpaper-stop --socket-path /tmp/my-mpv-socket --period 500 --fork --verbose
```
This will:
*   Connect to `mpvpaper` at `/tmp/my-mpv-socket`.
*   Check for windows every 500 milliseconds.
*   Run in the background.
*   Print verbose logs (if not forked, or to syslog if configured).

To run `mpvpaper-stop` automatically on Hyprland startup, add it to your `hyprland.conf` after the `exec` line for `mpvpaper`:
```ini
# In your hyprland.conf
exec-once = mpvpaper -o "input-ipc-server=/tmp/mpvsocket" '*' /path/to/your/video.mp4
exec-once = /path/to/your/mpvpaper-stop # Use the actual path to the executable
```

## How to control `mpvpaper` manually
While `mpvpaper-stop` handles pause/resume, you can still send other commands to `mpvpaper` using `socat` or any tool that can coomunicate with a Unix socket.

Refer to the [mpv manual's command interface section](https://mpv.io/manual/master/#command-interface) for available commands.

## License
This project is licensed under the MIT license. See the `LICENSE` file for details.
