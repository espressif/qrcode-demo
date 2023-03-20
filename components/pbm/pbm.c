#include <stdio.h>
#include <assert.h>
#include "pbm.h"
#include "esp_heap_caps.h"

static char* s_pbm_buf;
size_t s_pbm_buf_size;

void pbm_buf_alloc(void) {
    if (s_pbm_buf == NULL) {
        s_pbm_buf_size = 240*240*3+240+10;
        s_pbm_buf = heap_caps_malloc(240*240*3+240+10, MALLOC_CAP_SPIRAM);
        assert(s_pbm_buf);
    }
}

void pgm_save(const char* path, int w, int h, const uint8_t* data)
{
    pbm_buf_alloc();

    FILE* f = fopen(path, "wb");
    if (s_pbm_buf) {
        setvbuf(f, s_pbm_buf, _IOFBF, s_pbm_buf_size);
    }

    fprintf(f, "P2\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            fprintf(f, "%d ", *data);
            ++data;
        }
        fputs("\n", f);
    }
    fclose(f);
}
