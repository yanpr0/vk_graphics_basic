#include <chrono>
#include <iomanip>
#include <numeric>
#include <random>
#include <vector>

#include "simple_compute.h"

void execute_cpu_computation(int length)
{
  std::mt19937 gen(0);
  std::uniform_real_distribution<float> distr(-1.0, 1.0);
  std::vector<float> values(length);
  for (uint32_t i = 0; i < values.size(); ++i) {
    values[i] = distr(gen);
  }

  auto t1 = std::chrono::high_resolution_clock::now();

  std::vector<float> out(length, 0);

  for (int i = 0; i < length; ++i)
  {
    float val = 0.0f;
    for (int j = -3; j <= 3; ++j)
    {
      val += (i + j >= 0 && i + j < length) ? values[i + j] : 0.0f;
    }
    out[i] = values[i] - val / 7.0f;
  }

  auto t2 = std::chrono::high_resolution_clock::now();

  const std::chrono::duration<double, std::milli> ms = t2 - t1;

  float avg = std::reduce(out.begin(), out.end()) / out.size();

  std::cout <<
      std::setw(15) << "CPU: "  <<
      std::setw(15) << "time: " << std::setw(15) << ms.count() << "; " <<
      std::setw(15) << "avg: "  << std::setw(15) << avg        << '\n';
}


int main()
{
  constexpr int LENGTH = 100'000'000;
  constexpr int VULKAN_DEVICE_ID = 0;

  {
    std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(LENGTH, false);
    if(app == nullptr)
    {
      std::cout << "Can't create render of specified type" << std::endl;
      return 1;
    }

    app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

    app->Execute();
  }

  {
    std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(LENGTH, true);
    if(app == nullptr)
    {
      std::cout << "Can't create render of specified type" << std::endl;
      return 1;
    }

    app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

    app->Execute();
  }

  execute_cpu_computation(LENGTH);

  return 0;
}
