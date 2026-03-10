#include <Arduino.h>

#include "AppController.h"

namespace {
wt::AppController app;
}

void setup() {
  app.begin();
}

void loop() {
  app.update();
  delay(1);
}
