#include "tinyalsa/asoundlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <utils/Thread.h>

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164
#define FORMAT_PCM 1

int capturingFlag  = 0;
int playingType = 0;   
int playSuccess = 1; 

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};


static void InitTinyMixer(void)
{
    struct mixer* mixer;
    struct mixer_ctl* ctl;
    int retVal;

    mixer = mixer_open(0);
    if (mixer == NULL) {
        printf("open mixer failed\n");
    }

    ctl = mixer_get_ctl(mixer, 16);
    retVal = mixer_ctl_set_value(ctl, 0, 2);    // set speaker = spk(1) 0: headet, 1:spk, 2:headset-spk
    if (retVal < 0) {
        printf("mixer_ctl_set_value(Speaker) failed\n");
    }

    ctl = mixer_get_ctl(mixer, 1);
    retVal = mixer_ctl_set_value(ctl, 0, 31);   // set Line Volume to max
    if (retVal < 0) {
        printf("mixer_ctl_set_value(Line Volume) failed\n");
    }

    mixer_close(mixer);
}

int CaptureSample(FILE* fp, unsigned int card, unsigned int device, unsigned int channels, unsigned int rate, unsigned int bits, unsigned int period_size, unsigned int period_count)
{
    struct pcm_config config;
    struct pcm* pcm;
    char* buffer;
    unsigned int size;
    unsigned int captureBytes = 0;

    config.channels = channels;
    config.in_init_channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    if (bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;
    pcm = pcm_open_req(card, device, PCM_IN, &config, rate);
    if (pcm == NULL || pcm_is_ready(pcm) == 0) {
        printf("open PCM device failed\n");
        return 0;
    }
    size = pcm_get_buffer_size(pcm);
    buffer = (char*)malloc(size);
    if (buffer == NULL) {
        printf("malloc %d bytes failed\n", size);
        pcm_close(pcm);
        return 0;
    }

    printf("capturing sound: %u channels, %u hz, %u bits\n", channels, rate, bits);
    while (capturingFlag && !pcm_read(pcm, buffer, size)) {
        if (fwrite(buffer, 1, size, fp) != size) {
            printf("capturing sound error\n");
            break;
        }
        captureBytes += size;
    }
    free(buffer);
    pcm_close(pcm);

    return captureBytes / ((bits / 8) * channels);
}

void* TinyCaptureThread(void* argv)
{
    FILE* recFp;
    unsigned int card = 0;
    unsigned int device = 0;
    unsigned int channels = 2;
    unsigned int rate = 44100;
    unsigned int bits = 16;
    unsigned int frames;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;

    recFp = fopen((char*)argv, "wb");
    if (recFp == NULL) {
        printf("open file %s failed\n", (char*)argv);
        return NULL;
    }

    struct wav_header header = {
        ID_RIFF,
        0,
        ID_WAVE,
        ID_FMT,
        16,
        FORMAT_PCM,
        channels,
        rate,
        (bits / 8) * channels * rate,
        (bits / 8) * channels,
        bits,
        ID_DATA,
        0
    };
    fseek(recFp, sizeof(struct wav_header), SEEK_SET);
    capturingFlag  = 1;     // elapse for 20s, then reset to 0 by main func
    frames = CaptureSample(recFp, card, device, header.num_channels, header.sample_rate, header.bits_per_sample, period_size, period_count);
    printf("capture %d frames\n", frames);
    header.data_sz = frames * header.block_align;
    fseek(recFp, 0, SEEK_SET);
    fwrite(&header, sizeof(struct wav_header), 1, recFp);
    fclose(recFp);

    return NULL;
}

static void tinymix_set_value(struct mixer *mixer, unsigned int id,
                              char *string)
{
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_values;
    unsigned int i;
	
    ctl = mixer_get_ctl(mixer, id);
    type = mixer_ctl_get_type(ctl);
    num_values = mixer_ctl_get_num_values(ctl);

    if (isdigit(string[0])) {
        int value = atoi(string);

        for (i = 0; i < num_values; i++) {
            if (mixer_ctl_set_value(ctl, i, value)) {
                fprintf(stderr, "Error: invalid value\n");
                return;
            }
        }
    } else {
        if (type == MIXER_CTL_TYPE_ENUM) {
            if (mixer_ctl_set_enum_by_string(ctl, string))
                fprintf(stderr, "Error: invalid enum value\n");
        } else {
            fprintf(stderr, "Error: only enum types can be set with strings\n");
        }
    }
}

void play_sample(FILE *fp, unsigned int card, unsigned int device, unsigned int channels, unsigned int rate, unsigned int bits, unsigned int period_size, unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    char *buffer;
    int size;
    int num_read;

    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    if (bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (bits == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        printf("open PCM device %u (%s) failed\n", device, pcm_get_error(pcm));
        playSuccess = 0;
        return;
    }
    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = (char *)malloc(size);
    if (buffer == NULL) {
        printf("allocate %d bytes failed\n", size);
        playSuccess = 0;
        pcm_close(pcm);
        return;
    }
    printf("playing wav file: %u ch, %u hz, %u bit\n", channels, rate, bits);
    do {
        num_read = fread(buffer, 1, size, fp);
        if (num_read > 0) {
            if (pcm_write(pcm, buffer, num_read)) {
                printf("Error playing sample\n");
                playSuccess = 0;
                break;
            }
        }
    } while (num_read > 0);
    printf("end of the wav file\n");

    free(buffer);
    pcm_close(pcm);
}

void* TinyPlayThread(void* argv)
{
    FILE *fp;
    struct riff_wave_header riff_wave_header;
    struct chunk_header chunk_header;
    struct chunk_fmt chunk_fmt;
    unsigned int device = 0;
    unsigned int card = 0;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;
    int more_chunks = 1;
    int fd;

    printf("open file %s and ready to play\n", (char*)argv);
    if ((fd = open((char *)argv, O_RDONLY | O_NONBLOCK)) > 0) {
        close(fd);
    } else {
        playingType = 1;
        printf("open file '%s' failed\n", (char*)argv);
        return NULL;
    }
    fp = fopen((char*)argv, "rb");
    if (fp == NULL) {
        printf("open file '%s' failed\n", (char*)argv);
        return NULL;
    }
    fread(&riff_wave_header, sizeof(riff_wave_header), 1, fp);
    if ((riff_wave_header.riff_id != ID_RIFF) || (riff_wave_header.wave_id != ID_WAVE)) {
        printf("Error: '%s' is not a riff/wave file\n", (char*)argv);
        fclose(fp);
        return NULL;
    }
	//InitTinyMixer(); 
    //system("tinymix 1 31");
    //system("tinymix 16 1");
	struct mixer *mixer = mixer_open(card);
	tinymix_set_value(mixer, 16, "1");
	tinymix_set_value(mixer, 12, "1");
    do {
        fread(&chunk_header, sizeof(chunk_header), 1, fp);
        switch (chunk_header.id) {
            case ID_FMT:
                fread(&chunk_fmt, sizeof(chunk_fmt), 1, fp);
                /* If the format header is larger, skip the rest */
                if (chunk_header.sz > sizeof(chunk_fmt))
                    fseek(fp, chunk_header.sz - sizeof(chunk_fmt), SEEK_CUR);
                break;
            case ID_DATA:
                /* Stop looking for chunks */
                more_chunks = 0;
                break;
            default:
                /* Unknown chunk, skip bytes */
                fseek(fp, chunk_header.sz, SEEK_CUR);
        }
    } while (more_chunks);

    play_sample(fp, card, device, chunk_fmt.num_channels, chunk_fmt.sample_rate, chunk_fmt.bits_per_sample, period_size, period_count);
    fclose(fp);
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char **argv)
{
	pthread_t captureTid;
	char* captureFile = "/mnt/sdcard/capture.wav";
	if(!strcmp("record_mic", argv[1])){
		printf("by hero is record mic... start\n");
		pthread_create(&captureTid, NULL, TinyCaptureThread, captureFile);
		while(capturingFlag == 0) {
			sleep(10);
		}
		capturingFlag = 0; 
		sleep(1);
		printf("by hero is record mic... end\n");
    }else if(!strcmp("play_audio", argv[1])){
		printf("by hero is play audio...\n");
		if(!access(captureFile, R_OK)) {		
			TinyPlayThread(captureFile);
		}else{
			printf("by hero playaudio not find\n");
		}
	}else{
		printf("by hero not thing... see you\n");
	}
}

