#include <blink/raster/raster.h>
#include <iostream>

int main()
{
  namespace br = blink::raster;
  auto raster = br::open<int>("demo.tif"); // assuming file exists and is valid
  int sum = 0;
  for (auto&& value : raster) {
    sum += value;
  }
  std::cout << "Mean value: " << static_cast<double>(sum) / raster.size() << std::endl;
 
  return 0;
}