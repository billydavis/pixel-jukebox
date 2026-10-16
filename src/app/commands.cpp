#include "app/commands.h"

#include "pins.h"
#include "app/telemetry.h"

static const int QUEUE_LENGTH = 8;
static const uint32_t BUTTON_POLL_MS = 10;
static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t LONG_PRESS_MS = 600;
static const uint32_t SETUP_PRESS_MS = 5000;
static const uint32_t BUTTON_STACK_BYTES = 2048;

static QueueHandle_t queue;

bool commandSend(CommandType type, uint32_t value) {
  Command command = {type, value};
  return xQueueSend(queue, &command, 0) == pdTRUE;
}

bool commandReceive(Command &command) { return xQueueReceive(queue, &command, 0) == pdTRUE; }

bool commandPeek(Command &command) { return xQueuePeek(queue, &command, 0) == pdTRUE; }

// A long press fires as soon as it is long enough; a short press fires on release.
static void buttonTask(void *) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bool pressed = false;
  bool longSent = false;
  bool setupSent = false;
  uint32_t pressedAt = 0;

  for (;;) {
    bool down = digitalRead(BUTTON_PIN) == LOW;
    uint32_t now = millis();
    if (down && !pressed) {
      pressed = true;
      longSent = false;
      setupSent = false;
      pressedAt = now;
    } else if (down && !longSent && now - pressedAt >= LONG_PRESS_MS) {
      longSent = true;
      commandSend(CommandType::ToggleHold);
    } else if (down && !setupSent && now - pressedAt >= SETUP_PRESS_MS) {
      setupSent = true;
      commandSend(CommandType::WifiSetup);
    } else if (!down && pressed) {
      pressed = false;
      if (!longSent && now - pressedAt >= DEBOUNCE_MS) {
        commandSend(CommandType::Next);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
  }
}

void commandsBegin() {
  queue = xQueueCreate(QUEUE_LENGTH, sizeof(Command));
  TaskHandle_t task = nullptr;
  xTaskCreate(buttonTask, "button", BUTTON_STACK_BYTES, nullptr, 2, &task);
  telemetryRegisterTask("button", task, BUTTON_STACK_BYTES);
}
