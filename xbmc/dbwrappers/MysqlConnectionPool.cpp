/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MysqlConnectionPool.h"

#include "utils/log.h"

CMysqlConnectionPool& CMysqlConnectionPool::GetInstance()
{
  static CMysqlConnectionPool instance;
  return instance;
}

MYSQL* CMysqlConnectionPool::Checkout(const std::string& host,
                                      const std::string& port,
                                      const std::string& dbName)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  PoolKey key{host, port, dbName};
  auto it = m_pool.find(key);
  if (it == m_pool.end() || it->second.empty())
    return nullptr;

  auto& entries = it->second;
  while (!entries.empty())
  {
    PoolEntry entry = entries.back();
    entries.pop_back();

    if (mysql_ping(entry.conn) == 0)
    {
      CLog::Log(LOGDEBUG, "CMysqlConnectionPool: reusing pooled connection for {}", dbName);
      return entry.conn;
    }

    CLog::Log(LOGDEBUG, "CMysqlConnectionPool: discarding stale connection for {}", dbName);
    mysql_close(entry.conn);
  }

  return nullptr;
}

void CMysqlConnectionPool::Return(const std::string& host,
                                  const std::string& port,
                                  const std::string& dbName,
                                  MYSQL* conn)
{
  if (!conn)
    return;

  std::lock_guard<std::mutex> lock(m_mutex);

  EvictStale();

  PoolKey key{host, port, dbName};
  m_pool[key].push_back({conn, std::chrono::steady_clock::now()});
  CLog::Log(LOGDEBUG, "CMysqlConnectionPool: returned connection for {}", dbName);
}

void CMysqlConnectionPool::EvictStale()
{
  auto now = std::chrono::steady_clock::now();

  for (auto it = m_pool.begin(); it != m_pool.end();)
  {
    auto& entries = it->second;
    for (auto eit = entries.begin(); eit != entries.end();)
    {
      if (now - eit->lastUsed > IDLE_TIMEOUT)
      {
        CLog::Log(LOGDEBUG, "CMysqlConnectionPool: evicting idle connection for {}",
                  it->first.dbName);
        mysql_close(eit->conn);
        eit = entries.erase(eit);
      }
      else
      {
        ++eit;
      }
    }

    if (entries.empty())
      it = m_pool.erase(it);
    else
      ++it;
  }
}

void CMysqlConnectionPool::Shutdown()
{
  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto& [key, entries] : m_pool)
  {
    for (auto& entry : entries)
      mysql_close(entry.conn);
  }
  m_pool.clear();
  CLog::Log(LOGINFO, "CMysqlConnectionPool: all connections closed");
}

CMysqlConnectionPool::~CMysqlConnectionPool()
{
  Shutdown();
}
