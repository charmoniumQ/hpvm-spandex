#ifndef __POLICY__
#define __POLICY__

#include "device_abstraction.h"
#include <string>

/************************* Policies *************************************/
class Policy {
public:
  virtual int getVersion(const char *, int64_t) = 0;
  virtual ~Policy(){};
};

class ConstPolicy : public Policy {
public:
  ConstPolicy(int deviceID) : deviceID(deviceID) {}

  int getVersion(const char *, int64_t) override { return deviceID; }

private:
  int deviceID;
};

class NodePolicy : public Policy {
  virtual int getVersion(const char *name, int64_t it) override {
    std::string s(name);
    std::string NodeNames[] = {
        "WrapperGaussianSmoothing_cloned",
        "WrapperlaplacianEstimate_cloned",
        "WrapperComputeZeroCrossings_cloned",
        "WrapperComputeGradient_cloned",
        "WrapperComputeMaxGradient_cloned",
        "WrapperRejectZeroCrossings_cloned",
    };
    return 2;
  }
};

class IterationPolicy : public Policy {
  virtual int getVersion(const char *name, int64_t it) override {
    if ((it % 10 == 0) || (it % 10 == 1))
      return 0;
    else
      return 2;
  }
};

class DeviceStatusPolicy : public Policy {
  virtual int getVersion(const char *name, int64_t it) override {
    if (deviceStatus) {
      return 2;
    } else {
      return 0;
    }
  }
};

/* ------------------------------------------------------------------------- */

class InteractivePolicy : public Policy {
private:
  // 0 :for CPU, 1 for GPU, 2 for Vector
  unsigned int userTargetDeviceChoice;
  // Used to end thread execution
  bool end;
  // Thread that will update userTargetDeviceChoice
  std::thread userTargetDeviceChoiceThread;
  // Thread function
  void updateUserTargetChoice() {
    while (!end) {
      std::cout << "Select target device (0 for CPU, 1 fpr GPU): ";
      std::cin >> userTargetDeviceChoice;
      if (userTargetDeviceChoice > 1) {
        std::cout << "Invalid target device. Selecting GPU instead.\n";
        userTargetDeviceChoice = 1;
      }
    }
  }

public:
  // Inherited method, erquired for every policy object
  virtual int getVersion(const char *name, int64_t it) {
    return userTargetDeviceChoice;
  }

  InteractivePolicy() {
    userTargetDeviceChoice = 1;
    end = false;
    userTargetDeviceChoiceThread =
        std::thread(&InteractivePolicy::updateUserTargetChoice, this);
  }

  ~InteractivePolicy() {
    end = true;
    userTargetDeviceChoiceThread.join();
  }
};

#endif // __POLICY__
