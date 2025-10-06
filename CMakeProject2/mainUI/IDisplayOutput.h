#ifndef IDISPLAYOUTPUT_H
#define IDISPLAYOUTPUT_H

#include <string>

class IDisplayOutput {
public:
  virtual ~IDisplayOutput() = default;
  virtual void displayText(const std::string& text) = 0;
};

#endif // IDISPLAYOUTPUT_H