# extension/shared is the only source for the extension implementation.
# Browser directories are generated packages with browser-specific manifests.
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
foreach(browser chromium-development firefox-development)
  file(GLOB_RECURSE files RELATIVE "${root}/extension/shared" LIST_DIRECTORIES false "${root}/extension/shared/*")
  foreach(file IN LISTS files)
    get_filename_component(parent "${root}/extension/${browser}/${file}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent}")
    configure_file("${root}/extension/shared/${file}" "${root}/extension/${browser}/${file}" COPYONLY)
  endforeach()
endforeach()
