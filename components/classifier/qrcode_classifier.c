#include "qrcode_classifier.h"
#include "sys/types.h"
#include <regex.h>
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

static const char *TAG = "classifier";

struct classifier {
    regex_t re;
    char filename[64];

    TAILQ_ENTRY(classifier) list_entry;
};

static TAILQ_HEAD(classifier_list, classifier) s_classifiers = TAILQ_HEAD_INITIALIZER(s_classifiers);

void classifier_init(void)
{
    FILE* in = fopen("/data/qrclass.txt", "r");
    if (in == NULL) {
        ESP_LOGE(TAG, "Failed to open /data/qrclass.txt. No classifiers loaded.");
        return;
    }
    while(!feof(in)) {
        regex_t re;
        char regex[128];
        char filename[128];

        fscanf(in, "%s %s\n", regex, filename);
        ESP_LOGI(TAG, "Compiling regex '%s' for filename '%s'", regex, filename);
        if (regcomp(&re, regex, REG_EXTENDED|REG_NOSUB) != 0) {
            ESP_LOGE(TAG, "Failed to compile regex '%s'", regex);
            continue;
        }

        struct classifier* pcls = calloc(sizeof(*pcls), 1);
        assert(pcls);
        pcls->re = re;
        strlcpy(pcls->filename, filename, sizeof(pcls->filename));
        TAILQ_INSERT_TAIL(&s_classifiers, pcls, list_entry);
        ESP_LOGI(TAG, "Added classifier for file '%s'", filename);
    }
    fclose(in);
}

const char* classifier_get_pic_from_qrcode_data(const char* text)
{
    struct classifier *it;
    TAILQ_FOREACH(it, &s_classifiers, list_entry) {
        int res = regexec(&it->re, text, (size_t) 0, NULL, 0);
        if (res == 0) {
            return it->filename;
        }
    }
    
    return NULL;
}
