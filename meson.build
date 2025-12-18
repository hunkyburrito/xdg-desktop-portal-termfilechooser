project(
    'xdg-desktop-portal-termfilechooser',
    'c',
    version: '1.3.0',
    license: 'MIT',
    meson_version: '>=0.60.0',
    default_options: ['c_std=c11', 'warning_level=2', 'werror=true'],
)

add_project_arguments(
    '-DTERMFILECHOOSER_VERSION="' + meson.project_version() + '"',
    language: 'c',
)

cc = meson.get_compiler('c')

add_project_arguments(
    cc.get_supported_arguments(
        [
            '-Wno-missing-braces',
            '-Wno-missing-field-initializers',
            '-Wno-unused-parameter',
            '-D_POSIX_C_SOURCE=200809L',
        ],
    ),
    language: 'c',
)

prefix = get_option('prefix')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
libexecdir = get_option('libexecdir')
add_project_arguments('-DSYSCONFDIR="@0@"'.format(join_paths(prefix, sysconfdir)), language: 'c')
add_project_arguments('-DDATADIR="@0@"'.format(join_paths(prefix, datadir)), language: 'c')

inc = include_directories('include')

rt = cc.find_library('rt')
iniparser = dependency('inih')
sd_bus_provider = get_option('sd-bus-provider')

if sd_bus_provider == 'auto'
    sdbus = dependency(['libsystemd', 'libelogind', 'basu'], required: true)
else
    sdbus = dependency(sd_bus_provider, required: true)
endif
add_project_arguments('-DHAVE_' + sdbus.name().to_upper() + '=1', language: 'c')

xdptf_files = files(
    'src/core/config.c',
    'src/core/logger.c',
    'src/core/main.c',
    'src/core/request.c',
    'src/filechooser/filechooser.c',
    'src/filechooser/uri.c',
)

executable(
    'xdg-desktop-portal-termfilechooser',
    [xdptf_files],
    dependencies: [
        sdbus,
        rt,
        iniparser,
    ],
    include_directories: [inc],
    install: true,
    install_dir: libexecdir,
)

conf_data = configuration_data()
conf_data.set('libexecdir', join_paths(prefix, libexecdir))
conf_data.set('systemd_service', '')

systemd = dependency('systemd', required: get_option('systemd'))

if systemd.found()
    systemd_service_file = 'xdg-desktop-portal-termfilechooser.service'
    user_unit_dir = systemd.get_variable(
        pkgconfig: 'systemduserunitdir',
        pkgconfig_define: ['prefix', prefix],
    )
    conf_data.set('systemd_service', 'SystemdService=' + systemd_service_file)

    configure_file(
        configuration: conf_data,
        input: 'contrib/systemd/' + systemd_service_file + '.in',
        output: '@BASENAME@',
        install_dir: user_unit_dir,
    )
endif

configure_file(
    configuration: conf_data,
    input: 'org.freedesktop.impl.portal.desktop.termfilechooser.service.in',
    output: '@BASENAME@',
    install_dir: join_paths(datadir, 'dbus-1', 'services'),
)

install_data(
    'termfilechooser.portal',
    install_dir: join_paths(datadir, 'xdg-desktop-portal', 'portals'),
)

install_subdir(
    'contrib',
    install_dir: join_paths(datadir, 'xdg-desktop-portal-termfilechooser'),
    strip_directory: true,
)

scdoc = dependency(
    'scdoc',
    required: get_option('man-pages'),
    version: '>= 1.9.7',
    native: true,
)
if scdoc.found()
    man_pages = ['xdg-desktop-portal-termfilechooser.5.scd']
    foreach src : man_pages
        topic = src.split('.')[0]
        section = src.split('.')[1]
        output = topic + '.' + section

        custom_target(
            output,
            input: files(src),
            output: output,
            command: [
                'sh',
                '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.get_variable('scdoc'), output),
            ],
            install: true,
            install_dir: join_paths(get_option('mandir'), 'man' + section),
        )
    endforeach
endif
