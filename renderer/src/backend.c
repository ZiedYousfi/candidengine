/**
 * @file backend.c
 * @brief Backend selection and common functionality
 */

#include <candid/backend.h>
#include <string.h>

/*******************************************************************************
 * Backend Registration
 ******************************************************************************/

/* Forward declarations for backend interfaces */
#if defined(__APPLE__)
extern const Candid_BackendInterface candid_metal_backend;
#endif

#if defined(CANDID_VULKAN_SUPPORT)
extern const Candid_BackendInterface candid_vulkan_backend;
#endif

#if defined(_WIN32)
extern const Candid_BackendInterface candid_d3d12_backend;
#endif

static const Candid_BackendInterface *s_backends[CANDID_BACKEND_COUNT] = {0};
static bool s_initialized = false;

static void init_backends(void) {
  if (s_initialized)
    return;

#if defined(__APPLE__)
  s_backends[CANDID_BACKEND_METAL] = &candid_metal_backend;
#endif

#if defined(CANDID_VULKAN_SUPPORT)
  s_backends[CANDID_BACKEND_VULKAN] = &candid_vulkan_backend;
#endif

#if defined(_WIN32)
  // s_backends[CANDID_BACKEND_D3D12] = &candid_d3d12_backend;
#endif

  s_initialized = true;
}

const Candid_BackendInterface *candid_backend_get(Candid_Backend backend) {
  init_backends();

  if (backend == CANDID_BACKEND_AUTO) {
    backend = candid_backend_get_preferred();
  }

  if (backend >= CANDID_BACKEND_COUNT)
    return NULL;

  return s_backends[backend];
}

Candid_Backend candid_backend_get_preferred(void) {
  init_backends();

#if defined(__APPLE__)
  if (s_backends[CANDID_BACKEND_METAL])
    return CANDID_BACKEND_METAL;
#endif

#if defined(CANDID_VULKAN_SUPPORT)
  if (s_backends[CANDID_BACKEND_VULKAN])
    return CANDID_BACKEND_VULKAN;
#endif

#if defined(_WIN32)
  if (s_backends[CANDID_BACKEND_D3D12])
    return CANDID_BACKEND_D3D12;
#endif

  return CANDID_BACKEND_AUTO; // No backend available
}

bool candid_backend_is_available(Candid_Backend backend) {
  init_backends();

  if (backend == CANDID_BACKEND_AUTO)
    return candid_backend_get_preferred() != CANDID_BACKEND_AUTO;

  if (backend >= CANDID_BACKEND_COUNT)
    return false;

  return s_backends[backend] != NULL;
}

uint32_t candid_backend_get_available(Candid_Backend *out, uint32_t max_count) {
  init_backends();

  uint32_t count = 0;

  for (uint32_t i = 1; i < CANDID_BACKEND_COUNT && count < max_count; ++i) {
    if (s_backends[i]) {
      if (out)
        out[count] = (Candid_Backend)i;
      ++count;
    }
  }

  return count;
}
