#include <assert.h>
#include "env_sensor.h"

int main(void)
{
  EnvData_t data = {0};

  assert(!data.temperature_valid);
  assert(!data.humidity_valid);
  assert(!data.light_valid);
  assert(!data.air_valid);
  return 0;
}
