set(SOURCES NetworkAndroid.cpp
            ZeroconfAndroid.cpp
            ZeroconfBrowserAndroid.cpp
      )

set(HEADERS NetworkAndroid.h
            ZeroconfAndroid.h
            ZeroconfBrowserAndroid.h
      )

if(MICROHTTPD_FOUND)
  list(APPEND SOURCES VfsProxyServer.cpp)
  list(APPEND HEADERS VfsProxyServer.h)
endif()

core_add_library(platform_android_network)
