#pragma once

#include <WiFiClientSecure.h>

#include "storage/card.h"

// A TLS connection that takes the card lock (storage/card.h) for each TLS
// operation on its own, as the live connection does, rather than for a whole
// request: a Makapix request held it for up to 4.6 s, mostly waiting on the
// network, and every card read behind it waited too, the control page's
// pictures included, which stalled the one-at-a-time web server for seconds
// (2026-10-18). Used for Makapix and for the weather.
class LockedTls : public WiFiClientSecure {
 public:
  int connect(IPAddress ip, uint16_t port) override {
    CardLock card;
    return WiFiClientSecure::connect(ip, port);
  }
  int connect(IPAddress ip, uint16_t port, int32_t timeout) override {
    CardLock card;
    return WiFiClientSecure::connect(ip, port, timeout);
  }
  int connect(const char *host, uint16_t port) override {
    CardLock card;
    return WiFiClientSecure::connect(host, port);
  }
  int connect(const char *host, uint16_t port, int32_t timeout) override {
    CardLock card;
    return WiFiClientSecure::connect(host, port, timeout);
  }
  size_t write(uint8_t byte) override {
    CardLock card;
    return WiFiClientSecure::write(byte);
  }
  size_t write(const uint8_t *buf, size_t size) override {
    CardLock card;
    return WiFiClientSecure::write(buf, size);
  }
  int available() override {
    CardLock card;
    return WiFiClientSecure::available();
  }
  int read() override {
    CardLock card;
    return WiFiClientSecure::read();
  }
  int read(uint8_t *buf, size_t size) override {
    CardLock card;
    return WiFiClientSecure::read(buf, size);
  }
  int peek() override {
    CardLock card;
    return WiFiClientSecure::peek();
  }
  void stop() override {
    CardLock card;
    WiFiClientSecure::stop();
  }
  uint8_t connected() override {
    CardLock card;
    return WiFiClientSecure::connected();
  }
};
