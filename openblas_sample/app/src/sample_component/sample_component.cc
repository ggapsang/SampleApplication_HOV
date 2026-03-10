#include "sample_component.h"

#include <cblas.h>


SampleComponent::SampleComponent() : SampleComponent(kID, kName.data()) {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    default:
      Component::ProcessAEvent(event);
      break;
  }
  return true;
}

void SampleComponent::Start() {
  Component::Start();

  TestOpenBLAS();
}

void SampleComponent::TestOpenBLAS() {
  const int n = 3;

  std::vector<double> x{1.0, 2.0, 3.0};
  std::vector<double> y{4.0, 5.0, 6.0};
  double alpha = 2.0;

  int incx = 1;
  int incy = 1;

  //vector dot product calc
  // cblas_ddot: double precision dot product
  double result = cblas_ddot(n, x.data(),incx, y.data(), incy);

  // expected result : 1.0 * 4.0 + 2.0 * 5.0 + 3.0 + 6.0 = 32
  LOG(GetObjectName(), __func__, __LINE__, "Result : " + std::to_string(result));

  cblas_daxpy(n, alpha, x.data(), incx, y.data(), incy);

  for(auto value : y) {
    LOG(GetObjectName(), __func__, __LINE__, std::to_string(value));
  }
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}
