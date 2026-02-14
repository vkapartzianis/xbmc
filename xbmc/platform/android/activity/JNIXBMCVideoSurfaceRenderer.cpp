/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JNIXBMCVideoSurfaceRenderer.h"

#include "CompileInfo.h"
#include "utils/log.h"

#include <androidjni/Context.h>
#include <androidjni/jutils-details.hpp>

using namespace jni;

static std::string s_className =
    std::string(CCompileInfo::GetClass()) + "/XBMCVideoSurfaceRenderer";

CJNIXBMCVideoSurfaceRenderer::CJNIXBMCVideoSurfaceRenderer(const CJNISurface& outputSurface)
{
  JNIEnv* env = xbmc_jnienv();

  // Load the class first with exception checking
  jhclass cls = CJNIContext::getClassLoader().loadClass(GetDotClassName(s_className));
  if (env->ExceptionCheck())
  {
    CLog::Log(LOGERROR,
              "CJNIXBMCVideoSurfaceRenderer: Class not found: {}", s_className);
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }

  std::string signature =
      "(Landroid/view/Surface;)L" + s_className + ";";

  m_object = call_static_method<jhobject>(
      env, cls,
      "create", signature.c_str(),
      outputSurface.get_raw());

  if (env->ExceptionCheck())
  {
    CLog::Log(LOGERROR,
              "CJNIXBMCVideoSurfaceRenderer: create() threw an exception");
    env->ExceptionDescribe();
    env->ExceptionClear();
    m_object = jhobject();
    return;
  }

  if (!m_object)
    CLog::Log(LOGERROR, "CJNIXBMCVideoSurfaceRenderer: Failed to create renderer");
}

CJNIXBMCVideoSurfaceRenderer::~CJNIXBMCVideoSurfaceRenderer()
{
}

CJNISurface CJNIXBMCVideoSurfaceRenderer::getInputSurface()
{
  return call_method<jhobject>(m_object, "getInputSurface", "()Landroid/view/Surface;");
}

void CJNIXBMCVideoSurfaceRenderer::setRangeCorrection(bool enabled)
{
  call_method<void>(m_object, "setRangeCorrection", "(Z)V", static_cast<jboolean>(enabled));
}

void CJNIXBMCVideoSurfaceRenderer::release()
{
  call_method<void>(m_object, "release", "()V");
}
