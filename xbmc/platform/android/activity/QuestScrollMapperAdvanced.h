#include <androidjni/KeyCharacterMap.h>

struct KeyState_ {
  int keyCode;
  bool isPressed;
  int64_t lastEventTime;  // in milliseconds
  bool isRepeating;
  
  KeyState_(int code) : keyCode(code), isPressed(false), 
  lastEventTime(0), isRepeating(false) {}
};

class DirectionalKeys_ {
private:
  static constexpr float DEAD_ZONE = 0.1f;
  static constexpr int64_t INITIAL_REPEAT_DELAY_MS = 500;
  static constexpr int64_t REPEAT_INTERVAL_MS = 50;
  
  bool prevUp, prevDown, prevLeft, prevRight;
  
  void processKeyState(KeyState_& state, bool prevState, int64_t now,
                       std::function<void(int, bool, bool)> keyCallback) {
    if (state.isPressed) {
      if (!prevState) {
        // Initial press
        keyCallback(state.keyCode, true, false);
        state.lastEventTime = now;
      } else {
        // Key is being held
        int64_t timeSinceLastEvent = now - state.lastEventTime;
        if (!state.isRepeating) {
          if (timeSinceLastEvent >= INITIAL_REPEAT_DELAY_MS) {
            keyCallback(state.keyCode, true, true);
            state.lastEventTime = now;
            state.isRepeating = true;
          }
        } else {
          if (timeSinceLastEvent >= REPEAT_INTERVAL_MS) {
            keyCallback(state.keyCode, true, true);
            state.lastEventTime = now;
          }
        }
      }
    } else if (prevState) {
      // Key was just released
      keyCallback(state.keyCode, false, false);
      state.isRepeating = false;
    }
  }
  
public:
  KeyState_ up{AKEYCODE_DPAD_UP};
  KeyState_ down{AKEYCODE_DPAD_DOWN};
  KeyState_ left{AKEYCODE_DPAD_LEFT};
  KeyState_ right{AKEYCODE_DPAD_RIGHT};
  
  void update(float x, float y) {
    // Store previous states
    prevUp = up.isPressed;
    prevDown = down.isPressed;
    prevLeft = left.isPressed;
    prevRight = right.isPressed;
    
    // Update current states
    x = std::clamp(x, -1.0f, 1.0f);
    y = std::clamp(y, -1.0f, 1.0f);
    
    if (std::abs(x) < DEAD_ZONE) x = 0;
    if (std::abs(y) < DEAD_ZONE) y = 0;
    
    up.isPressed = y > DEAD_ZONE;
    down.isPressed = y < -DEAD_ZONE;
    left.isPressed = x < -DEAD_ZONE;
    right.isPressed = x > DEAD_ZONE;
    
    // Reset repeat state when key is released
    if (!up.isPressed) up.isRepeating = false;
    if (!down.isPressed) down.isRepeating = false;
    if (!left.isPressed) left.isRepeating = false;
    if (!right.isPressed) right.isRepeating = false;
  }
  
  void processKeyStateChanges(std::function<void(int, bool, bool)> keyCallback) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    processKeyState(up, prevUp, now, keyCallback);
    processKeyState(down, prevDown, now, keyCallback);
    processKeyState(left, prevLeft, now, keyCallback);
    processKeyState(right, prevRight, now, keyCallback);
  }
};
