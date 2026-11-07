project(
  'vipdf',
  'cpp',
  version: '0.1.0',
  meson_version: '>= 0.58.0',
  default_options: [
    'cpp_std=c++23',
    'buildtype=release',
    'sqlite3:default_library=static',
  ],
)
project_name = meson.project_name()

cmake = import('cmake')

# harfbuzz
hb_proj = subproject(
  'harfbuzz',
  default_options: [
    'buildtype=release',
    'default_library=static',
    'raster=enabled',
    'freetype=enabled',
    'tests=disabled',
    'docs=disabled',
    'benchmark=disabled',
    'utilities=disabled',
    'glib=disabled',
    'gobject=disabled',
    'icu=disabled',
    'cairo=disabled',
  ],
)

harfbuzz_dep = hb_proj.get_variable('libharfbuzz_dep')
harfbuzz_raster_dep = hb_proj.get_variable('libharfbuzz_raster_dep')

systemd_dep = dependency('libsystemd', required: true)

# dbus library
sdbus_opts = cmake.subproject_options()
sdbus_opts.add_cmake_defines(
  {
    'SDBUSCPP_BUILD_DOCS': false,
    'SDBUSCPP_BUILD_TESTS': false,
    'SDBUSCPP_BUILD_CODEGEN': false,
    'SDBUSCPP_BUILD_EXAMPLES': false,
    'SDBUSCPP_INSTALL': false,
    'BUILD_SHARED_LIBS': false,
  },
)
sdbus_proj = cmake.subproject('sdbus-cpp', options: sdbus_opts)
sdbus_dep = sdbus_proj.dependency('sdbus-c++')

# clipboard library
clip_opts = cmake.subproject_options()
clip_opts.add_cmake_defines(
  {
    'CLIP_EXAMPLES': false,
    'CLIP_TESTS': false,
    'CLIP_INSTALL': false,
    'CLIP_ENABLE_IMAGE': true,
    'BUILD_SHARED_LIBS': false,
  },
)
clip_proj = cmake.subproject('clip', options: clip_opts)
clip_dep = clip_proj.dependency('clip')

png_dep = dependency('libpng', required: true)
xcb_dep = dependency('xcb', required: true)

# sqlitecpp library
sqlitecpp_dep = dependency(
  'sqlitecpp',
  default_options: ['default_library=static'],
  required: true,
)

# sfml library
sfml_opts = cmake.subproject_options()
sfml_opts.add_cmake_defines(
  {
    'BUILD_SHARED_LIBS': false,
    'SFML_BUILD_AUDIO': false,
    'SFML_BUILD_NETWORK': false,
    'SFML_BUILD_EXAMPLES': false,
    'SFML_BUILD_DOC': false,
    'SFML_BUILD_TEST_SUITE': false,
  },
)
sfml_proj = cmake.subproject('sfml', options: sfml_opts)
sfml_dep = sfml_proj.dependency('sfml-graphics')

# system libs
mupdf_dep = dependency('mupdf', required: true)
freetype_dep = dependency('freetype2', required: true)

# header only libs
utf8cpp_includes = include_directories('thirdparty/utfcpp')
inicpp_includes = include_directories('thirdparty/inicpp')
json_includes = include_directories('thirdparty/json')

datadir = get_option('prefix') / get_option('datadir')
asset_install_dir = datadir / project_name

conf = configuration_data()
conf.set_quoted('VIPDF_DATADIR', asset_install_dir)
configure_file(output: 'vipdf_config.h', configuration: conf)

includes = include_directories('include', '.')

sources = files(
  'src/app.cpp',
  'src/core/bookmark_manager.cpp',
  'src/core/cmd_autocomplete.cpp',
  'src/core/cmd_loader.cpp',
  'src/core/cmd_processor.cpp',
  'src/core/config_reader.cpp',
  'src/core/history_manager.cpp',
  'src/core/render_scheduler.cpp',
  'src/dbus/file_printer.cpp',
  'src/dbus/link_opener.cpp',
  'src/dbus/portal.cpp',
  'src/graphics/font_library.cpp',
  'src/graphics/glyph_atlas.cpp',
  'src/graphics/rich_text.cpp',
  'src/main.cpp',
  'src/pdf/pdf_document.cpp',
  'src/pdf/pdf_layout_manager.cpp',
  'src/pdf/pdf_page.cpp',
  'src/pdf/pdf_renderer.cpp',
  'src/pdf/pdf_search_controller.cpp',
  'src/pdf/pdf_selection_controller.cpp',
  'src/pdf/pdf_view_controller.cpp',
  'src/ui/cmdline.cpp',
  'src/ui/completions.cpp',
  'src/ui/cursor.cpp',
  'src/ui/document_view.cpp',
  'src/ui/notifications.cpp',
  'src/ui/page_view.cpp',
  'src/ui/password_dialog.cpp',
  'src/ui/statusbar.cpp',
  'src/ui/textbox.cpp',
  'src/utils/profiler.cpp',
  'src/utils/trie.cpp',
)

executable(
  project_name,
  sources,
  include_directories: [includes, utf8cpp_includes, json_includes, inicpp_includes],
  dependencies: [
    sfml_dep,
    mupdf_dep,
    harfbuzz_dep,
    harfbuzz_raster_dep,
    freetype_dep,
    sqlitecpp_dep,
    sdbus_dep,
    clip_dep,
    # thirdparty deps
    systemd_dep,
    png_dep,
    xcb_dep,
  ],
  install: true,
)

install_subdir('assets/fonts', install_dir: asset_install_dir)
install_data('assets/commands.json', install_dir: asset_install_dir)

install_data('assets/vipdf.desktop', install_dir: datadir / 'applications')
install_data('assets/vipdf.svg', install_dir: datadir / 'icons/hicolor/scalable/apps')

meson.add_devenv(environment({'VIPDF_ASSETS': meson.project_source_root() / 'assets'}))
