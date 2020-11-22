#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

#include <string>

/*
H = {P|I} H*Ct = 0
1001010010011110101011000000000
1101111011010001111110100000000
1111101111110110010100010000000
0111110111111011001010001000000
1010101001100011001110000100000
1100000110101111001100000010000
0110000011010111100110000001000
1010010011110101011000000000100
0101001001111010101100000000010
0010100100111101010110000000001
*/
const uint32_t h[10] = {
0b1001010010011110101011000000000,
0b1101111011010001111110100000000,
0b1111101111110110010100010000000,
0b0111110111111011001010001000000,
0b1010101001100011001110000100000,
0b1100000110101111001100000010000,
0b0110000011010111100110000001000,
0b1010010011110101011000000000100,
0b0101001001111010101100000000010,
0b0010100100111101010110000000001
};

uint32_t err_map[1024] = {0};

int even_parity(uint32_t m) {
    uint32_t x = m;
    x ^= x >> 16;
	x ^= x >> 8;
	x ^= x >> 4;
	x &= 0xf;
    return (0x6996 >> x) & 1;
}

int bin_cnt(uint32_t m) {
    int ret = 0;
    uint32_t x = m;
    while (x) {
        x &= x - 1;
        ret += 1;
    }
    return ret;
}


#define BLOCKSIZE 1*1024*1024
int    readSize;

#pragma pack(1)
struct fourccValue{
    char    fourcc[4];
    int32_t size;
};

struct fmtHead{
    int16_t format;
    int16_t channel;
    int32_t samplerate;
    int32_t bytePerSec;
    int16_t bytePerSample;
    int16_t bitDepth;
};

int fmtProcess(int fd, int len, struct fmtHead* head)
{
    int ret = 0;
    int readSize = read(fd, head, 16);
    if (readSize) ret += readSize;
    printf("format:0x%04x %dch %dHz %dByte/Sec %dByte/Sample %dbit\n",
            head->format & 0xffff,
            head->channel,
            head->samplerate,
            head->bytePerSec,
            head->bytePerSample,
            head->bitDepth
            );
    if (len - 16 > 0) {
        readSize += lseek(fd, len - 16, SEEK_CUR);
        if (readSize) ret += len - 16;
    }
    return 0;
}

int dataProcess(int fd, int len, const struct fmtHead &head, std::string &ret) {
    int byteToRead = len;
    int readSize = 0;
    // int blockInSize = 60 * head.bytePerSec;
    int blockInSize = len;
    uint8_t* blockIn = new uint8_t[blockInSize];
    int read_time = 0;
    while (byteToRead) {
        readSize = read(fd, blockIn, blockInSize);
        if (readSize > 0) {
            byteToRead -= readSize;
            printf("dataProcess() fd = %5d readSize=%10d byteToRead=%10d\n",fd, readSize, byteToRead);
        } else {
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


        int ch = 2;
        int srate = 32000;
        int baud = 1200;

        int32_t last_sample = 0; //TODO
        int sample_idx = 0;
        int last_through_zero = -1;
        int cnt = 0;
        int32_t level_trigger = 0;
        int fir_size = 65536;
        int32_t last[fir_size] = {0};
        memset(last, 0, fir_size * sizeof(int32_t));
        int32_t iir_tmp = 0;
        for (int i = 0; i < readSize; i+=ch) {
            iir_tmp -= last[(sample_idx+fir_size-4) % fir_size];
            last[sample_idx % fir_size] = (uint8_t(blockIn[i]) - 0x80);
            iir_tmp += last[sample_idx % fir_size];

            /*int sample = (last[sample_idx % 16] + last[(sample_idx+15) % 16] + last[(sample_idx+14) % 16] + last[(sample_idx+13) % 16]
                        + last[(sample_idx+12) % 16] + last[(sample_idx+11) % 16] + last[(sample_idx+10) % 16] + last[(sample_idx+9) % 16]);
                        + last[(sample_idx+8) % 16] + last[(sample_idx+7) % 16] + last[(sample_idx+6) % 16] + last[(sample_idx+5) % 16]
                        + last[(sample_idx+4) % 16] + last[(sample_idx+3) % 16] + last[(sample_idx+2) % 16] + last[(sample_idx+1) % 16]) >> 4;*/
            //iir_tmp = iir_tmp + last[i % 16] - last[(i + 2) % 16];*/
            int32_t sample = iir_tmp >> 2;
 
            
            //debug for iir
            /*if (sample_idx >= 0 && sample_idx <= 64) {
                printf("%8d %8d %5d:%05d -%5d %5d\n", (uint8_t(blockIn[i]) - 0x80), iir_tmp, sample_idx/srate,sample_idx%srate, last[(sample_idx+fir_size-8) % fir_size], last[sample_idx % fir_size]);
            }*/
            

            //if ((last_sample >= level_trigger && sample < level_trigger) || (last_sample < level_trigger && sample >= level_trigger)) {  // down/up ET
            if ((last_sample < level_trigger && sample >= level_trigger)) {  // up ET
                int a = sample_idx - last_through_zero;
                    /*if (sample_idx/srate >= 456 && sample_idx/srate <= 456) {
                        printf("%5d.%05d %10d %3d\n", sample_idx/srate,sample_idx%srate*100000/srate, a, cnt);
                    }*/
                //if (a >= 19 && a <= 35) {
                if (a >= 46 && a <= 60) { //44...62
                    cnt++;
                    if (cnt > 10) {
                        //printf("%5d %5d %10d %10d\n",sample_idx/srate,sample_idx%srate, a, cnt);
                    }
                } else {
                    //if (cnt >= 4 && a >= 46 && a <= 60) {
                    if (cnt >= 3 && a >= 176 && a <= 196) { //171...201
                        printf("block:%3d %5d.%03d %10d_len %10d_sync\n",read_time, sample_idx/srate,sample_idx%srate*1000/srate, a, cnt);
                        // i -= 26*ch;
                        int sample_sync_idx = sample_idx - a*6/7;
                        
                        if ((sample_sync_idx + (4 * 32 + 32)*srate/baud + 21) * ch > readSize) {
                            continue;
                        }
                        for (int word_idx = 0; word_idx < 5; word_idx++) {
                            uint32_t m = 0;
                            char rstr[33] = {'\0'};
                            for (int j = 0; j < 32; j++) {
                                int32_t symbol = 0;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 6) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 7) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 8) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 9) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 10) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 11) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 12) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 13) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 14) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 15) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 16) * ch]) - 0x80;
                                symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 17) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 18) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 19) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 20) * ch]) - 0x80;
                                // symbol += uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 21) * ch]) - 0x80;
                                symbol /= 8;
                                //int32_t symbol = uint8_t(blockIn[(sample_sync_idx + (word_idx * 32 + j)*srate/baud + 13) * ch]) - 0x80;
                                bool b = symbol < level_trigger ? 1:0;
                                rstr[j] = b ? '1' : '0';
                                m |= b << (31 - j);
                            }

                            uint32_t ebin = 0;
                            char estr[11] = {'\0'};
                            for (int ii = 0; ii < 10; ii++) {
                                bool bit = even_parity(h[ii] & (m>>1));
                                estr[ii] = bit ? '1' : ' ';
                                ebin <<= 1;
                                ebin |= bit;
                            }
                            // err = arr_map[ebin]

                            // if (word_idx == 0 && m != 0x7cd215d8) break;

                            if (m >> 31) {
                                int n = m ^ (err_map[ebin] << 1);
                                n >>= 11;
                                n &= 0xfffff;
                                n = (n&0x55555555)<<1|(n&0xAAAAAAAA)>>1;
                                n = (n&0x33333333)<<2|(n&0xCCCCCCCC)>>2;
                                const char c[] = "0123456789*U -][";
                                char text[6] = {'\0'};
                                for (int ii = 0; ii < 5; ii++) {
                                    int bcd = (n >> (4*(4-ii))) & 0xf;
                                    if (bcd >= 0 && bcd < 16) {
                                        text[ii] = c[bcd];
                                    }
                                }
                                printf("%32s [%10s] msg = %08x %08x payload %05x %5s", rstr, estr, m, m >> 11, n, text);
                                char log[256];
                                sprintf(log, "%5s", text);
                                ret.append(log);
                            } else {
                                if (m == 0x7a89c197) {
                                    printf(" msg = %08x empty break\n", m);
                                    break;
                                }
                                switch (m ^ (err_map[ebin] << 1)) {
                                    case 0x7cd215d8:
                                        //("block:%3d %5d.%03d %10d_len %10d_sync\n",read_time, sample_idx/srate,sample_idx%srate*1000/srate, a, cnt);//
                                        printf(" msg = %08x %08x SC head", m, m >> 11);
                                        break;
                                    case 0x7a89c197:
                                        printf(" msg = %08x empty", m);
                                        break;
                                    default:
                                        printf("%32s [%10s] msg = %08x %08x address %7d %02x", rstr, estr, m, m >> 11, ((m >> 13) & 0x3ffff) << 3, (m >> 11) & 0x3);
                                        char log[256];
                                        switch (((m >> 13) & 0x3ffff)  << 3) {
                                            case 1234008:
                                                printf(" time");
                                                sprintf(log, "%5d.%03d %7d %02x: ", sample_idx/srate,(sample_idx%srate)*1000/srate, ((m >> 13) & 0x3ffff) << 3, (m >> 11) & 0x3);
                                                ret.append(log);
                                                break;
                                            case 1234000:
                                                sprintf(log, "%5d.%03d %7d %02x: ", sample_idx/srate,(sample_idx%srate)*1000/srate, ((m >> 13) & 0x3ffff) << 3, (m >> 11) & 0x3);
                                                ret.append(log);
                                                switch ((m >> 11) & 0x3) {
                                                    case 0b11:
                                                        printf(" up");
                                                        break;
                                                    case 0b01:
                                                        printf(" down");
                                                        break;
                                                }
                                                break;
                                            default:
                                                break;
                                        }
                                        break;
                                }
                                
                            }

                            if (ebin > 0) {
                                printf(" err:%08x", err_map[ebin]);
                            }

                            if (even_parity(m)) {
                                printf(" !even_parity");
                            }


                            printf("\n");
                        }
                        //printf("\n");
                        ret.append("\n");
                    }
                    cnt = 0;
                }

                /*if (sample_idx / 3200 == 200) {
                    printf("%5d %5d %10d %10d\n",sample_idx/srate,sample_idx%srate, a, cnt);
                }*/
                last_through_zero = sample_idx;
            } else {
                
            }
            last_sample = sample;
            sample_idx += 1;
        }
        printf("sample_idx %d\n", sample_idx);
        read_time++;
    }
    delete[] blockIn;
    return len - byteToRead;
}

void displayFourcc(const fourccValue& fourcc)
{
    printf("[%c%c%c%c] %08x %10d\n",
           fourcc.fourcc[0],
           fourcc.fourcc[1],
           fourcc.fourcc[2],
           fourcc.fourcc[3],
           fourcc.size,
           fourcc.size);
}

int openwav(const char* filename, std::string &ret) {
    int fd = open(filename, O_RDONLY|O_BINARY);
    if (fd < 0) {
        perror(filename);
        return -1;
    } else {
        printf("fd=%d %s\n", fd, filename);
    }

    struct fourccValue riff;
    char               wave[4];
    struct fmtHead     head;

    readSize = read(fd, &riff, 8);
    readSize = read(fd, &wave, 4);
    displayFourcc(riff);

    while (readSize > 0) {
        struct fourccValue packHead;
        readSize = read(fd, &packHead, 8);
        if (readSize > 0) {
            displayFourcc(packHead);
        } else {
            break;
        }

        switch (*(int*)&packHead.fourcc) {
        case 0x20746d66: //'f' 'm' 't' ' '
            fmtProcess(fd, packHead.size, &head);
            break;
        case 0x61746164: //'d' 'a' 't' 'a'
            dataProcess(fd, packHead.size, head, ret);
            break;
        default:
            lseek(fd, packHead.size, SEEK_CUR);
            break;
        }
    }

    close(fd);
    return 0;
}

int main(int argc, char* argv[])
{

                            // uint32_t err_map[1024] = {0};

                            for (int i=0;i<31;i++) {
                                uint32_t ebin = 0;
                                uint32_t m = (1<<i);
                                for (int ii = 0; ii < 10; ii++) {
                                    int bit = even_parity(h[ii] & m);
                                    // printf("%d", bit);
                                    ebin <<= 1;
                                    ebin |= bit;
                                }
                                // printf(" %04x %08x\n",ebin, (1<<i));
                                err_map[ebin] = (1<<i);
                            }

                            for (int i=0;i<31;i++) {
                            for (int j=0;j<31;j++) {
                                uint32_t ebin = 0;
                                uint32_t m = (1<<i | 1<<j);
                                for (int ii = 0; ii < 10; ii++) {
                                    int bit = even_parity(h[ii] & m);
                                    // printf("%d", bit);
                                    ebin <<= 1;
                                    ebin |= bit;
                                }
                                // printf(" %04x %08x\n", ebin, (1<<i | 1<<j));
                                err_map[ebin] = (1<<i | 1<<j);
                            }
                            }

                            for (int i = 0; i < 1024; i++) {
                                if (i % 32 == 0) {
                                    printf("\n");
                                }
                                if (err_map[i] == 0) {
                                    printf(" ");
                                } else {
                                    printf("1");
                                }
                            }
                            printf("\n");
                            // printf("%08x\n", err_map[0b0011101101]);


    char filename[1024] = "";
    if (argc > 1) {
        strcpy(filename, argv[1]);
    }

    std::string ret = "";
    openwav(filename, ret);
    printf("%s\n", ret.c_str());

    return 0;
}
