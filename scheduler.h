#include <vector>
#include <functional>
#include <Arduino.h>

class Scheduler {
public:
  struct Task {
    unsigned long interval;
    unsigned long lastExecution;
    bool repeat;
    std::function<void()> callback;
  };

private:
  std::vector<Task> tasks;
  
  using ErrorHandler = void(*)(const char*);
  static ErrorHandler currentErrorHandler;
  
  static void defaultErrorHandler(const char* err) {
    Serial.println(err);
  }

public:
  void setInterval(std::function<void()> callback, unsigned long interval) {
    tasks.push_back({interval, millis(), true, callback});
  }

  void setTimeout(std::function<void()> callback, unsigned long delay) {
    tasks.push_back({delay, millis(), false, callback});
  }

  void updateProcess(unsigned long delta) {
    unsigned long currentTime = millis();

    for (auto it = tasks.begin(); it != tasks.end(); ) {
      if (currentTime - it->lastExecution >= it->interval) {
        try {it->callback();} 
        catch (const char* err) {currentErrorHandler(err);}
        
        it->lastExecution = currentTime;

        if (!it->repeat) {
          it = tasks.erase(it);
          continue;
        }
      }
      ++it;
    }
  }

  static void setErrorHandler(ErrorHandler handler) {
    currentErrorHandler = handler;
  }
};

Scheduler::ErrorHandler Scheduler::currentErrorHandler = Scheduler::defaultErrorHandler;