#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>

#define BLOCKSIZE 1 * 1024 * 1024
int readSize;

#pragma pack(1)
struct fourccValue
{
    char fourcc[4];
    int32_t size;
};

struct fmtHead
{
    int16_t format;
    int16_t channel;
    int32_t samplerate;
    int32_t bytePerSec;
    int16_t bytePerSample;
    int16_t bitDepth;
};

int fmtProcess(int fd, int fd2, int len, struct fmtHead *head)
{
    int ret = 0;
    int readSize = read(fd, head, 16);
    write(fd2, head, 16);
    if (readSize)
        ret += readSize;
    printf("format:0x%04x %dch %dHz %dByte/Sec %dByte/Sample %dbit\n",
           head->format & 0xffff,
           head->channel,
           head->samplerate,
           head->bytePerSec,
           head->bytePerSample,
           head->bitDepth);
    if (len - 16 > 0)
    {
        char *a = (char *)malloc(len - 16);
        readSize = read(fd, a, len - 16);
        write(fd2, a, len - 16);

        //ret += lseek(fd, len - 16, SEEK_CUR);
        //printf("error skip by lssek!");
        if (readSize)
            ret += readSize;
    }
    return 0;
}

int dataProcess(int fd, int fd2, int len, const struct fmtHead &head)
{
    int byteToRead = len;
    int readSize = 0;
    // int blockInSize = 60 * head.bytePerSec;
    int blockInSize = len;
    uint8_t *blockIn = new uint8_t[blockInSize];
    int read_time = 0;
    while (byteToRead)
    {
        readSize = read(fd, blockIn, blockInSize);
        if (readSize > 0)
        {
            byteToRead -= readSize;
            printf("dataProcess() fd = %5d readSize=%10d byteToRead=%10d\n", fd, readSize, byteToRead);
        }
        else
        {
            break;
        }
        /*for (int i = 0; i < readSize; i++) {
            if (i % 16 == 0) {
                printf("\n0x%08x ", i);
            } else {
                printf(" ");
            }
            printf("%02x", blockIn[i] & 0xff);
        }*/

        int ch = head.channel;
        int srate = 32000;
        int baud = 1200;

        int sample_idx = 0;
        int fir_size = 65536;
        int32_t last[fir_size] = {0};
        memset(last, 0, fir_size * sizeof(int32_t));
        int32_t iir_tmp = 0;
        int32_t iir_tmp_2 = 0;
        for (int i = 0; i < readSize; i += ch)
        {
            iir_tmp -= last[(sample_idx + fir_size - 16) % fir_size];
            iir_tmp_2 -= last[(sample_idx + fir_size - 4096) % fir_size];
            last[sample_idx % fir_size] = (uint8_t(blockIn[i]) - 0x80);
            iir_tmp += last[sample_idx % fir_size];
            iir_tmp_2 += last[sample_idx % fir_size];

            int32_t sample = (iir_tmp >> 4) - (iir_tmp_2 >> 12);

            blockIn[i] = uint8_t(sample + 0x80);

            sample_idx += 1;
        }
        printf("sample_idx %d\n", sample_idx);
        write(fd2, blockIn, readSize);
        read_time++;
    }
    delete[] blockIn;
    return len - byteToRead;
}

void displayFourcc(const fourccValue &fourcc)
{
    printf("[%c%c%c%c] %08x %10d\n",
           fourcc.fourcc[0],
           fourcc.fourcc[1],
           fourcc.fourcc[2],
           fourcc.fourcc[3],
           fourcc.size,
           fourcc.size);
}

int openwav(const char *filename)
{
    int fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        perror(filename);
        return -1;
    }
    else
    {
        printf("fd=%d %s\n", fd, filename);
    }

    char fn2[255] = {'\0'};
    sprintf(fn2, "%s_iir4.wav", filename);
    int fd2 = open(fn2, O_WRONLY | O_BINARY | O_CREAT);
    if (fd2 < 0)
    {
        perror(fn2);
        return -1;
    }
    else
    {
        printf("fd=%d %s\n", fd2, fn2);
    }

    struct fourccValue riff;
    char wave[4];
    struct fmtHead head;

    readSize = read(fd, &riff, 8);
    readSize = read(fd, &wave, 4);
    displayFourcc(riff);

    write(fd2, &riff, 8);
    write(fd2, &wave, 4);

    while (readSize > 0)
    {
        struct fourccValue packHead;
        readSize = read(fd, &packHead, 8);
        write(fd2, &packHead, 8);
        if (readSize > 0)
        {
            displayFourcc(packHead);
        }
        else
        {
            break;
        }

        switch (*(int *)&packHead.fourcc)
        {
        case 0x20746d66: //'f' 'm' 't' ' '
            fmtProcess(fd, fd2, packHead.size, &head);
            break;
        case 0x61746164: //'d' 'a' 't' 'a'
            dataProcess(fd, fd2, packHead.size, head);
            break;
        default:
            lseek(fd, packHead.size, SEEK_CUR);
            printf("error skip by lssek!");
            break;
        }
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{

    char filename[1024] = "";
    if (argc > 1)
    {
        strcpy(filename, argv[1]);
    }

    openwav(filename);

    return 0;
}
