//example_8.cpp

#include <pronto/raster/io.h>
#include <pronto/raster/nodata_transform.h>
#include <pronto/raster/plot_raster.h>

namespace br = pronto::raster;

int main()
{
  // Create some data
  auto in = br::create_temp<int>(4,5);
  int i = 0;
  for (auto&& v : in) {
    v = i;
    i = (i + 3) % 7;
  }

  // Treat value 6 as nodata
  auto nodata = br::nodata_to_optional(in, 6);

  // Treat nodata as value -99
  auto un_nodata = br::optional_to_nodata(nodata, -99);

  plot_raster(in);
  plot_raster(nodata);
  plot_raster(un_nodata);
  
  return 0;
}