set(SOURCES Database.cpp
            DatabaseQuery.cpp
            dataset.cpp
            qry_dat.cpp
            sqlitedataset.cpp)

set(HEADERS Database.h
            DatabaseQuery.h
            dataset.h
            qry_dat.h
            sqlitedataset.h)

if(TARGET MySqlClient::MySqlClient OR TARGET MariaDBClient::MariaDBClient)
  list(APPEND SOURCES MysqlConnectionPool.cpp
                      mysqldataset.cpp)
  list(APPEND HEADERS MysqlConnectionPool.h
                      mysqldataset.h)
endif()

core_add_library(dbwrappers)
