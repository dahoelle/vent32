#include <vector>
#include <functional>

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
        it->callback();
        it->lastExecution = currentTime;

        if (!it->repeat) {
          it = tasks.erase(it);
          continue;
        }
      }
      ++it;
    }
  }
};
