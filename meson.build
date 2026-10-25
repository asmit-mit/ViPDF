project('ViPDF', 'cpp',
    default_options: [
        'cpp_std=c++23',
        'buildtype=debugoptimized',
    ]
)
project_name = meson.project_name()

sfml_dep = dependency('sfml-graphics', required: true)
mupdf_dep = dependency('mupdf', required: true)
gtk_dep = dependency('gtk4', required: true)
harfbuzz_dep = dependency('harfbuzz', version: '>= 14.3.0', required: true)
harfbuzz_raster_dep = dependency('harfbuzz-raster', version: '>= 14.3.0', required: true)
freetype_dep = dependency('freetype2', required: true)

utf8cpp_includes = include_directories('thirdparty/utfcpp')
json_includes = include_directories('thirdparty/json')

includes = include_directories('include')
sources = files(
  'src/app.cpp',
  'src/core/cmd_processor.cpp',
  'src/core/cmd_loader.cpp',
  'src/core/cmd_autocomplete.cpp',
  'src/core/history_manager.cpp',
  'src/core/render_scheduler.cpp',
  'src/graphics/font_library.cpp',
  'src/graphics/glyph_atlas.cpp',
  'src/graphics/rich_text.cpp',
  'src/main.cpp',
  'src/pdf/pdf_document.cpp',
  'src/pdf/pdf_page.cpp',
  'src/pdf/pdf_renderer.cpp',
  'src/pdf/pdf_layout_manager.cpp',
  'src/pdf/pdf_view_controller.cpp',
  'src/pdf/pdf_search_controller.cpp',
  'src/pdf/pdf_selection_controller.cpp',
  'src/ui/cmdline.cpp',
  'src/ui/completions.cpp',
  'src/ui/cursor.cpp',
  'src/ui/notifications.cpp',
  'src/ui/page_view.cpp',
  'src/ui/document_view.cpp',
  'src/ui/scrollwheel.cpp',
  'src/ui/statusbar.cpp',
  'src/ui/textbox.cpp',
  'src/utils/trie.cpp',
)

executable(
  project_name,
  sources,
  include_directories: [includes, utf8cpp_includes, json_includes],
  dependencies: [sfml_dep, mupdf_dep, gtk_dep, harfbuzz_dep, harfbuzz_raster_dep, freetype_dep],
)
