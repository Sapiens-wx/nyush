#pragma once
//#define DEBUG

#ifdef DEBUG
#define dprintf(...) dprintf(__VA_ARGS__)
#else
#define dprintf(...) do{}while(0)
#endif
