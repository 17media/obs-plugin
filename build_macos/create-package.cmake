make_directory("$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/package/Library/Application Support/obs-studio/plugins")

if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.plugin" AND NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.plugin")
  file(INSTALL DESTINATION "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/package/Library/Application Support/obs-studio/plugins"
    TYPE DIRECTORY FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.plugin" USE_SOURCE_PERMISSIONS)

  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$" OR CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.plugin.dSYM" AND NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.plugin.dSYM")
      file(INSTALL DESTINATION "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/package/Library/Application Support/obs-studio/plugins" TYPE DIRECTORY FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.plugin.dSYM" USE_SOURCE_PERMISSIONS)
    endif()
  endif()
endif()

make_directory("$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/temp")

execute_process(
  COMMAND /usr/bin/pkgbuild
    --identifier 'com.example.obs-plugintemplate'
    --version '1.0.0'
    --root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/package"
    "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/temp/obs-plugintemplate.pkg"
    COMMAND_ERROR_IS_FATAL ANY
  )

execute_process(
  COMMAND /usr/bin/productbuild
    --distribution "/Users/zhuyu/workspace/eduself/obs/obs-plugintemplate/build_macos/distribution"
    --package-path "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/temp"
    "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.pkg"
    COMMAND_ERROR_IS_FATAL ANY)

if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/obs-plugintemplate.pkg")
  file(REMOVE_RECURSE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/temp")
  file(REMOVE_RECURSE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/package")
endif()
