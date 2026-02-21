/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EventLoop.h"

#include "XBMCApp.h"
#include "windowing/android/AndroidUtils.h"

#define IS_FROM_SOURCE(v, s) ((v & s) == s)

CEventLoop::CEventLoop(android_app* application)
  : m_application(application), m_activityHandler(NULL), m_inputHandler(NULL)
{
  if (m_application == NULL)
    return;

  m_application->userData = this;
  m_application->onAppCmd = activityCallback;
  m_application->onInputEvent = inputCallback;
}

void CEventLoop::run(IActivityHandler &activityHandler, IInputHandler &inputHandler)
{
  int ident;
  int events;
  struct android_poll_source* source;

  m_activityHandler = &activityHandler;
  m_inputHandler = &inputHandler;

  CXBMCApp::android_printf("CEventLoop: starting event loop");
  while (true)
  {
    // We will block forever waiting for events.
    while ((ident = ALooper_pollOnce(-1, NULL, &events, (void**)&source)) >= 0)
    {
      // Process this event.
      if (source != NULL)
        source->process(m_application, source);

      // Check if we are exiting.
      if (m_application->destroyRequested)
      {
        CXBMCApp::android_printf("CEventLoop: we are being destroyed");
        return;
      }
    }
  }
}

void CEventLoop::processActivity(int32_t command)
{
  switch (command)
  {
    case APP_CMD_CONFIG_CHANGED:
      m_activityHandler->onConfigurationChanged();
      break;

    case APP_CMD_INIT_WINDOW:
      // The window is being shown, get it ready.
      m_activityHandler->onCreateWindow(m_application->window);

      // set the proper DPI value
      m_inputHandler->setDPI(CXBMCApp::Get().GetDPI());
      break;

    case APP_CMD_WINDOW_RESIZED:
      // The window has been resized
      m_activityHandler->onResizeWindow();
      break;

    case APP_CMD_TERM_WINDOW:
      // The window is being hidden or closed, clean it up.
      m_activityHandler->onDestroyWindow();
      break;

    case APP_CMD_GAINED_FOCUS:
      m_activityHandler->onGainFocus();
      break;

    case APP_CMD_LOST_FOCUS:
      m_activityHandler->onLostFocus();
      break;

    case APP_CMD_LOW_MEMORY:
      m_activityHandler->onLowMemory();
      break;

    case APP_CMD_START:
      m_activityHandler->onStart();
      break;

    case APP_CMD_RESUME:
      m_activityHandler->onResume();
      break;

    case APP_CMD_SAVE_STATE:
      // The system has asked us to save our current state. Do so.
      m_activityHandler->onSaveState(&m_application->savedState, &m_application->savedStateSize);
      break;

    case APP_CMD_PAUSE:
      m_activityHandler->onPause();
      break;

    case APP_CMD_STOP:
      m_activityHandler->onStop();
      break;

    case APP_CMD_DESTROY:
      m_activityHandler->onDestroy();
      break;

    default:
      break;
  }
}

// =============================================================================

#include "QuestScrollMapperAdvanced.h"
// -----------------------------------------------------------------------------

DirectionalKeys_ scroll_keys_;

#include "ServiceBroker.h"
#include "application/AppInboundProtocol.h"
#include "input/keyboard/XBMC_keysym.h"
#include "input/mouse/MouseStat.h"

static std::map<int32_t, uint16_t> xKeyMap = {
//{ AKEYCODE_HOME            , XBMCK_HOME },
  { AKEYCODE_BACK            , XBMCK_BACKSPACE },
  { AKEYCODE_DPAD_UP         , XBMCK_UP },
  { AKEYCODE_DPAD_DOWN       , XBMCK_DOWN },
  { AKEYCODE_DPAD_LEFT       , XBMCK_LEFT },
  { AKEYCODE_DPAD_RIGHT      , XBMCK_RIGHT },
  { AKEYCODE_DPAD_CENTER     , XBMCK_RETURN },
  { AKEYCODE_BUTTON_A        , XBMCK_RETURN },
  { AKEYCODE_BUTTON_B        , XBMCK_BACKSPACE }
};

void xEvent(XBMC_Event& newEvent)
{
  std::shared_ptr<CAppInboundProtocol> appPort = CServiceBroker::GetAppPort();
  if (appPort)
    appPort->OnEvent(newEvent);
}

void xKey(uint32_t code, uint16_t key, bool up)
{
  XBMC_Event newEvent = {};

  unsigned char type = up ? XBMC_KEYUP : XBMC_KEYDOWN;
  newEvent.type = type;
  newEvent.key.keysym.scancode = code;
  newEvent.key.keysym.sym = (XBMCKey)key;
  newEvent.key.keysym.unicode = 0u;
  newEvent.key.keysym.mod = (XBMCMod)0u;

  xEvent(newEvent);
}

void xMouseMove(float x, float y)
{
  XBMC_Event newEvent = {};

  newEvent.type = XBMC_MOUSEMOTION;
  newEvent.motion.x = x;
  newEvent.motion.y = y;

  xEvent(newEvent);
}

void xMouseButton(float x, float y, uint16_t button, bool up)
{
  XBMC_Event newEvent = {};

  newEvent.type = (!up) ? XBMC_MOUSEBUTTONDOWN : XBMC_MOUSEBUTTONUP;
  newEvent.button.x = x;
  newEvent.button.y = y;
  newEvent.button.button = button;

  xEvent(newEvent);
}

int64_t last_scroll_time_ = 0;
int64_t last_lbdown_time_ = 0;
float last_lclick_x_ = 0.0f;
float last_lclick_y_ = 0.0f;
float last_hoverm_x_ = 0.0f;
float last_hoverm_y_ = 0.0f;

// Quest double-click: suppress HOVER_MOVE during the double-click window so pointer drift
// doesn't reset STATE_IN_DOUBLE_CLICK in MouseStat, and snap the second DOWN to the first
// UP position so InClickRange always passes. Must match double_click_time in MouseStat.h.
static constexpr int64_t QUEST_DOUBLE_CLICK_WINDOW_NS = 500LL * 1000000LL;
// Quest scroll: suppress HOVER_MOVE for this duration after a joystick scroll event so that
// incidental pointer drift from thumbstick movement doesn't move the cursor and interrupt
// joystick-driven navigation.
static constexpr int64_t QUEST_SCROLL_EVENT_WINDOW_NS = 1000LL * 1000000LL;
// Quest hover dead zone: suppress HOVER_MOVE events where the cursor has moved less than
// this many pixels from the last sent position. Ray jitter from the Quest controller can
// cause the cursor to drift across a control's hit boundary even when the pointer appears
// stationary, triggering repeated hover-out/hover-in transitions on the focused control.
static constexpr float QUEST_HOVER_DEAD_ZONE_PX = 3.0f;

// =============================================================================

int32_t CEventLoop::processInput(AInputEvent* event)
{
  int32_t rtn    = 0;
  int32_t type   = AInputEvent_getType(event);
  int32_t source = AInputEvent_getSource(event);

  // handle Quest controller joystick input
  if (CAndroidUtils::IsQuestDevice() && source == AINPUT_SOURCE_CLASS_POINTER)
  {
    if (type == AINPUT_EVENT_TYPE_MOTION)
    {
      int32_t eventAction = AMotionEvent_getAction(event);
      int8_t pointerAction = eventAction & AMOTION_EVENT_ACTION_MASK;
      size_t pointerPointerIdx = eventAction >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

      if (pointerAction == AMOTION_EVENT_ACTION_SCROLL)
      {
        last_scroll_time_ = AMotionEvent_getEventTime(event);

        float scroll_x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, pointerPointerIdx);
        float scroll_y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, pointerPointerIdx);

        auto keyCallback = [](int keycode, bool isPressed, bool isRepeat)
        {
          xKey(keycode, xKeyMap.at(keycode), !isPressed);
        };

        scroll_keys_.update(scroll_x, scroll_y);
        scroll_keys_.processKeyStateChanges(keyCallback);

        return true;
      }
    }
  }
  // handle Quest controller pointer
  if (CAndroidUtils::IsQuestDevice() && source == AINPUT_SOURCE_TOUCHSCREEN)
  {
    int32_t eventAction = AMotionEvent_getAction(event);
    int8_t pointerAction = eventAction & AMOTION_EVENT_ACTION_MASK;
    size_t pointerPointerIdx = eventAction >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    float x = AMotionEvent_getX(event, pointerPointerIdx);
    float y = AMotionEvent_getY(event, pointerPointerIdx);

    switch(pointerAction)
    {
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
    {
      int64_t now = AMotionEvent_getEventTime(event);
      // Suppress cursor movement during the double-click window. MouseStat processes
      // XBMC_MOUSEMOTION events through the button state machine: if the position
      // drifts more than 5px from the first click's UP position, STATE_IN_DOUBLE_CLICK
      // is reset to STATE_RELEASED before the second DOWN arrives, making double-click
      // impossible regardless of where the second click lands.
      if (now - last_lbdown_time_ < QUEST_DOUBLE_CLICK_WINDOW_NS)
        return true;
      // Suppress cursor movement after a joystick scroll event. The thumbstick
      // produces both scroll and HOVER_MOVE events; without this guard the
      // incidental pointer drift would move the cursor and interrupt navigation.
      if (now - last_scroll_time_ > QUEST_SCROLL_EVENT_WINDOW_NS)
      {
        float dx = x - last_hoverm_x_;
        float dy = y - last_hoverm_y_;
        if (dx * dx + dy * dy > QUEST_HOVER_DEAD_ZONE_PX * QUEST_HOVER_DEAD_ZONE_PX)
        {
          xMouseMove(x, y);
          last_hoverm_x_ = x;
          last_hoverm_y_ = y;
        }
      }
      return true;
    }
    case AMOTION_EVENT_ACTION_DOWN:
    {
      int64_t now = AMotionEvent_getEventTime(event);
      if (now - last_lbdown_time_ < QUEST_DOUBLE_CLICK_WINDOW_NS)
      {
        x = last_lclick_x_;
        y = last_lclick_y_;
      }
      xMouseButton(x, y, XBMC_BUTTON_LEFT, false);
      return true;
    }
    case AMOTION_EVENT_ACTION_UP:
      last_lbdown_time_ = AMotionEvent_getEventTime(event);
      last_lclick_x_ = x;
      last_lclick_y_ = y;
      xMouseButton(x, y, XBMC_BUTTON_LEFT, true);
      return true;
    }
  }

  // handle joystick input
  if (IS_FROM_SOURCE(source, AINPUT_SOURCE_GAMEPAD) || IS_FROM_SOURCE(source, AINPUT_SOURCE_JOYSTICK))
  {
    if (m_inputHandler->onJoyStickEvent(event))
      return true;
  }

  switch(type)
  {
    case AINPUT_EVENT_TYPE_KEY:
      rtn = m_inputHandler->onKeyboardEvent(event);
      break;
    case AINPUT_EVENT_TYPE_MOTION:
      if (IS_FROM_SOURCE(source, AINPUT_SOURCE_TOUCHSCREEN))
        rtn = m_inputHandler->onTouchEvent(event);
      else if (IS_FROM_SOURCE(source, AINPUT_SOURCE_MOUSE))
        rtn = m_inputHandler->onMouseEvent(event);
      break;
  }

  return rtn;
}

void CEventLoop::activityCallback(android_app* application, int32_t command)
{
  if (application == NULL || application->userData == NULL)
    return;

  CEventLoop& eventLoop = *((CEventLoop*)application->userData);
  eventLoop.processActivity(command);
}

int32_t CEventLoop::inputCallback(android_app* application, AInputEvent* event)
{
  if (application == NULL || application->userData == NULL || event == NULL)
    return 0;

  CEventLoop& eventLoop = *((CEventLoop*)application->userData);

  return eventLoop.processInput(event);
}

