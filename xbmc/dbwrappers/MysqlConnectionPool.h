/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifdef HAS_MYSQL
#include <mysql/mysql.h>
#elif defined(HAS_MARIADB)
#include <mariadb/mysql.h>
#endif

class CMysqlConnectionPool
{
public:
  static CMysqlConnectionPool& GetInstance();

  // Check out a live connection for the given {host, port, dbName}.
  // Returns nullptr if no pooled connection is available.
  MYSQL* Checkout(const std::string& host, const std::string& port, const std::string& dbName);

  // Return a connection to the pool.
  void Return(const std::string& host, const std::string& port, const std::string& dbName,
              MYSQL* conn);

  // Close all pooled connections. Called at shutdown.
  void Shutdown();

  ~CMysqlConnectionPool();

private:
  CMysqlConnectionPool() = default;
  CMysqlConnectionPool(const CMysqlConnectionPool&) = delete;
  CMysqlConnectionPool& operator=(const CMysqlConnectionPool&) = delete;

  struct PoolKey
  {
    std::string host;
    std::string port;
    std::string dbName;

    bool operator<(const PoolKey& other) const
    {
      if (host != other.host) return host < other.host;
      if (port != other.port) return port < other.port;
      return dbName < other.dbName;
    }
  };

  struct PoolEntry
  {
    MYSQL* conn;
    std::chrono::steady_clock::time_point lastUsed;
  };

  void EvictStale();

  std::mutex m_mutex;
  std::map<PoolKey, std::vector<PoolEntry>> m_pool;

  static constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);
};
