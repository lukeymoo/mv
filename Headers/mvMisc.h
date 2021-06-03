#pragma once

struct QueueIndexInfo
{
  uint32_t graphics = UINT32_MAX;
  uint32_t compute = UINT32_MAX;
  uint32_t transfer = UINT32_MAX;
};