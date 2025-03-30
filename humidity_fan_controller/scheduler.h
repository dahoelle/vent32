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

  using ErrorHandler = std::function<void(const String&)>;

  void setErrorHandler(ErrorHandler handler) {
    errorHandler = std::move(handler);
  }

  void setInterval(std::function<void()> callback, unsigned long interval) {
    if (!callback) {
      if (errorHandler) {
        errorHandler("setInterval: Callback is null");
      }
      return;
    }
    if (interval == 0) {
      if (errorHandler) {
        errorHandler("setInterval: Interval cannot be zero");
      }
      return;
    }
    tasks.push_back({interval, millis(), true, std::move(callback)});
  }

  void setTimeout(std::function<void()> callback, unsigned long delay) {
    if (!callback) {
      if (errorHandler) {
        errorHandler("setTimeout: Callback is null");
      }
      return;
    }
    tasks.push_back({delay, millis(), false, std::move(callback)});
  }

  void updateProcess() {
    unsigned long currentTime = millis();

    for (auto it = tasks.begin(); it != tasks.end(); ) {
      if (currentTime - it->lastExecution >= it->interval) {
        if (!it->callback) {
          handleError("Callback invalid during execution");
          it = tasks.erase(it);
          continue;
        }

        try {
          it->callback();  // Wrap callback execution in try-catch
        } 
        catch (const std::exception& e) {
          handleError(String("Exception: ") + e.what());
        } 
        catch (const String& e) {
          handleError(String("String Exception: ") + e);
        } 
        catch (...) {
          handleError("Unknown exception occurred");
        }

        it->lastExecution = currentTime;

        if (!it->repeat) {
          it = tasks.erase(it);
          continue;
        }
      }
      ++it;
    }
  }

private:
  void handleError(const String& message) {
    if (errorHandler) {
      errorHandler(message);
    }
  }
  std::vector<Task> tasks;
  ErrorHandler errorHandler;
};