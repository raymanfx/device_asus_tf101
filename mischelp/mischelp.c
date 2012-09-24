#define LOG_TAG "mischelp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>

#define MAX_COM_LENGTH 14

int main(int argc, char *argv[])
{
    char bootcom[MAX_COM_LENGTH] = {};
    printf("\n----------------------------------------------\n");
    printf("-------- %s by androidroot.mobi --------\n", LOG_TAG);
    printf("----------------------------------------------\n");

    if (argc < 2 || argc > 3){
        printf("\nUsage : %s <path of misc> <boot command>\n\n", LOG_TAG);
        ALOGE("ERROR : Usage : %s <path of misc> <boot command>\n", LOG_TAG);
        return 0;
    } else if (argc == 2) {
        ALOGI("Eraseing boot coommand to partition %s\n", argv[1]);
    } else if (argc == 3) {
        strncpy(bootcom, argv[2], MAX_COM_LENGTH);
        ALOGI("Writeing boot coommand \"%s\" from partition \"%s\"\n", bootcom, argv[1]);
    }

    FILE *f;
    f=fopen(argv[1], "wb");
    if (f == NULL) {
        fclose(f);
        ALOGE("ERROR : partition not found \"%s\"\n", argv[1]);
        return 0;
    }

    fwrite (bootcom , 1 , sizeof(bootcom) , f );
    fclose(f);

    return 0;
}
