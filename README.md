![Preview](https://github.com/pvtoari/mpvpaper-stop/blob/master/media/preview.gif)
# mpvpaper-stop

`mpvpaper-stop` is a lightweight background utility designed to work with [mpvpaper](https://github.com/GhostNaN/mpvpaper). It automatically pauses `mpvpaper` when any window becomes visible on your active Hyprland workspace and resumes playback when no windows are displayed.

**Important Note:** Currently, `mpvpaper-stop` exclusively supports **Hyprland** due to its reliance on `hyprctl` for window detection. Future enhancements may include support for other Wayland compositors.

## How it works

`mpvpaper-stop` periodically performs the following actions:
1.  It queries Hyprland via its IPC socket to determine the number of visible windows on the active workspace.
2.  It checks the current playback status of `mpvpaper` (paused or playing) via its IPC socket.
3.  If there are no windows visible and `mpvpaper` is paused, it sends a resume command.
4.  If there are one or more windows visible and `mpvpaper` is playing, it sends a pause command and triggers specified behaviours by the user, such as color generation.

This ensures that your video wallpaper only plays when it's actually visible.

## Features
*   Automatic pause/resume of `mpvpaper` based on window visibility.
*   Supports pywal on pause events to set your colour scheme to the paused frame's one
*   Configurable polling period.
*   Option to fork into the background.
*   Verbose mode for debugging.
*   Customizable path for `mpvpaper`'s IPC socket.
*   Waits for the `mpvpaper` socket to be available before starting.

## Dependencies
*   **[mpvpaper](https://github.com/GhostNaN/mpvpaper)**: Must be installed and running with IPC socket enabled.
*   **Hyprland**
*   Optional: pywal
## Prerequisites: `mpvpaper` IPC Setup

Before running `mpvpaper-stop`, ensure `mpvpaper` is started with the `input-ipc-server` option. This creates a socket that `mpvpaper-stop` uses to send commands.

Example `mpvpaper` execution (add to your Hyprland config or run manually):

```ini
# For a specific monitor (e.g., DP-1)
exec-once = mpvpaper -o "--input-ipc-server=/tmp/mpvsocket" DP-1 /path/to/your/video.mp4
```

**Note:** `mpvpaper-stop` defaults to `/tmp/mpvsocket` for the socket path. If you use a different path for `mpvpaper`, you must specify it when running `mpvpaper-stop`.

## Build using meson

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/pvtoari/mpvpaper-stop
    cd mpvpaper-stop
    ```

2.  **Setup project with meson**
    ```bash
    meson setup build
    ```

3.  **Compile and install**
    ```bash
    meson compile -C build
    cd build
    sudo meson install
    ```

## Usage

Run `mpvpaper-stop` after `mpvpaper` has started.

e.g. in your Hyprland config:
```ini
exec-once = mpvpaper -o "--input-ipc-server=/tmp/mpvsocket" ALL bg.mp4
exec-once = mpvpaper-stop
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
  -c, --pywal	         Runs pywal on pause
  -h, --help             Shows this help message
```

**Example with custom options:**
```bash
mpvpaper-stop --verbose --socket-path /tmp/my-mpv-socket --period 500 --pywal --fork 
```

This will:
*   Print verbose logs (if not forked, or to syslog if configured).
*   Connect to `mpvpaper` at `/tmp/my-mpv-socket`.
*   Check for windows every 500 milliseconds.
*   Trigger pywal on a pause event
*   Run in the background.

## License
This project is licensed under the MIT license. See the `LICENSE` file for details.
