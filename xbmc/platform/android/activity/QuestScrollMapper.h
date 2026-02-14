#include <androidjni/KeyCharacterMap.h>

struct KeyState_ {
  int keyCode;
  bool isPressed;
  
  KeyState_(int code) : keyCode(code), isPressed(false) {}
};

class DirectionalKeys_ {
private:
  static constexpr float DEAD_ZONE = 0.1f;
  bool prevUp, prevDown, prevLeft, prevRight;
  
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
  }
  
  void processKeyStateChanges(std::function<void(int, bool)> keyCallback) {
    // Only send events when state changes
    if (up.isPressed != prevUp) {
      keyCallback(up.keyCode, up.isPressed);
    }
    if (down.isPressed != prevDown) {
      keyCallback(down.keyCode, down.isPressed);
    }
    if (left.isPressed != prevLeft) {
      keyCallback(left.keyCode, left.isPressed);
    }
    if (right.isPressed != prevRight) {
      keyCallback(right.keyCode, right.isPressed);
    }
  }
};
