#pragma once

#include "typedef_application.h"

class ISampleComponent {
 public:
  static constexpr ClassID kID = GET_CLASS_UID(_ELayer_Application::_eSampleComponent);
  static constexpr std::string_view kName = "SampleComponent";

  enum class EEventType { eBegin = kID, eEnd };
};
