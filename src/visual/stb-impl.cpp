// Single translation unit that provides the stb_image / stb_image_write
// implementations. Every other file in this project only ever includes the
// headers in declaration form.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
