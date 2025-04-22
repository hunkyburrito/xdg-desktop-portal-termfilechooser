project(
    'xdg-desktop-portal-termfilechooser',
    'c',
    version: '1.0.5',
    license: 'MIT',
    meson_version: '>=0.50.0',
    default_options: ['c_std=c11', 'warning_level=2', 'werror=true'],
)

add_project_arguments('-DTERMFILECHOOSER_VERSION="'+meson.project_version()+'"', language: 'c')

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
    assert(
        get_option('auto_features').auto(),
        'sd-bus-provider must not be set to auto since auto_features != auto',
    )
    sdbus = dependency(
        'libsystemd',
        required: false,
        not_found_message: 'libsystemd not found, trying libelogind',
    )
    if not sdbus.found()
        sdbus = dependency(
            'libelogind',
            required: false,
            not_found_message: 'libelogind not found, trying basu',
        )
    endif
    if not sdbus.found()
        sdbus = dependency(
            'basu',
            required: false,
        )
    endif
    if not sdbus.found()
        error('Neither libsystemd, nor libelogind, nor basu was found')
    endif
else
    sdbus = dependency(sd_bus_provider)
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
    if meson.version().version_compare('>=51')
        user_unit_dir = systemd.get_variable(
            pkgconfig: 'systemduserunitdir',
            pkgconfig_define: ['prefix', prefix],
        )
    else
        user_unit_dir = systemd.get_pkgconfig_variable(
            'systemduserunitdir',
            define_variable: ['prefix', prefix],
        )
    endif
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

scdoc = dependency('scdoc', required: get_option('man-pages'), version: '>= 1.9.7', native: true)
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
                '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.get_pkgconfig_variable('scdoc'), output),
            ],
            install: true,
            install_dir: join_paths(get_option('mandir'), 'man' + section),
        )
    endforeach
endif
