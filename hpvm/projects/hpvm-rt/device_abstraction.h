#ifndef __DEVICE_ABSTRACTION__
#define __DEVICE_ABSTRACTION__

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <vector>

#define MIN_INTERVAL 2
#define MAX_INTERVAL 8
#define NUM_INTERVALS 10

// Device status variable: true if the device is available for use
volatile bool deviceStatus = true;
// Intervals at which to change the device status
std::vector<unsigned> Intervals;

// Set to true when program execution ends and so we can end the device
// simulation
volatile bool executionEnd = false;

void initializeDeviceStatusIntervals() {

  unsigned sz = 0;
  unsigned tmp = 0;

  const char *fn = "/home/kotsifa2/HPVM/hpvm/build/projects/hpvm-rt/"
                   "deviceStatusSwitchIntervals.txt";
  std::ifstream infile;
  infile.open(fn);
  if (!infile.is_open()) {
    std::cout << "Failed to open " << fn << " for reading\n";
    return;
  }
  infile >> sz;

  if (sz) {
    // We have data. Read them into the vector
    for (unsigned i = 0; i < sz; i++) {
      infile >> tmp;
      Intervals.push_back(tmp);
    }
    infile.close();
  } else {
    // We have no data. Create random data and write them into the file
    infile.close();
    std::ofstream outfile;
    outfile.open(fn);
    if (!outfile.is_open()) {
      std::cout << "Failed to open " << fn << " for writing\n";
      return;
    }
    sz = 1 + rand() % NUM_INTERVALS;
    outfile << sz;
    for (unsigned i = 0; i < sz; i++) {
      Intervals.push_back(MIN_INTERVAL +
                          rand() % (MAX_INTERVAL - MIN_INTERVAL));
      outfile << Intervals[i];
    }
    outfile.close();
  }

  return;
}

void updateDeviceStatus() {

  unsigned i = 0;
  while (!executionEnd) {
    std::this_thread::sleep_for(std::chrono::seconds(Intervals[i]));
    deviceStatus = !deviceStatus;
    std::cout << "Changed device status to " << deviceStatus << "\n";
    i = (i + 1) % Intervals.size();
  }
}

#endif // __DEVICE_ABSTRACTION__
