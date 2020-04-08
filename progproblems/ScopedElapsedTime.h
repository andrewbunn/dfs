#pragma once
#include <string>
#include <chrono>

class ScopedElapsedTime {
 public:
  ScopedElapsedTime();
  ~ScopedElapsedTime();

  double getElapsedTime() const;

private:
  const std::chrono::steady_clock::time_point _start;
};