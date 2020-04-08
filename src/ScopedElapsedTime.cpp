#include "ScopedElapsedTime.h"
#include <iostream>

ScopedElapsedTime::ScopedElapsedTime()
    : _start(std::chrono::steady_clock::now()) {}

double ScopedElapsedTime::getElapsedTime() const {
  const auto end = std::chrono::steady_clock::now();
  const auto diff = end - _start;
  const auto msTime = std::chrono::duration<double, std::milli>(diff).count();
  return msTime;
}

ScopedElapsedTime::~ScopedElapsedTime() {
  std::cout << "Elapsed time: " << getElapsedTime() << std::endl;
}