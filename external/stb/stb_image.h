// Minimal stb_image-compatible interface backed by Windows WIC
// Public domain / MIT-like: this stub is provided solely for this project.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char stbi_uc;

void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
const char* stbi_failure_reason(void);
void stbi_image_free(void* retval_from_stbi_load);
stbi_uc* stbi_load(char const* filename, int* x, int* y, int* comp, int req_comp);

#ifdef __cplusplus
}
#endif
