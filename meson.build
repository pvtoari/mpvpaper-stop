project('mpvpaper-stop', 'c',
    version: run_command('cat', join_paths(meson.project_source_root(), 'VERSION'), check: true).stdout().strip(),
    default_options : ['c_std=gnu11', 'buildtype=release']
)

inc = include_directories('include')
cjson_dep = dependency('libcjson', required: true)

executable('mpvpaper-stop', 'src/main.c',
    dependencies: [cjson_dep],
    include_directories: inc,
    install: true
)
