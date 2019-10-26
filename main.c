#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#define BUFFER_SAMPLECOUNT    (100000)

uint8_t bm_channel_clk;
uint8_t bm_channel_cmd;
uint8_t bm_channel_d0;
uint8_t bm_channel_d1;
uint8_t bm_channel_d2;
uint8_t bm_channel_d3;

#define BYTESPERSAMPLE (1ull)

float sample_frequency_MHz;



unsigned char buffer[BYTESPERSAMPLE*BUFFER_SAMPLECOUNT];
FILE *ptr, *ptr2;
uint32_t samplecount;
uint32_t bufferstartsample;

const char CMDNAMES[64][255] =
{
  "GO_IDLE_STATE",
  "SEND_OP_COND",
  "ALL_SEND_CID",
  "SEND_RELATIVE_ADDR",
  "SET_DSR",
  "IO_SEND_OP_COND", // CMD5
  "",
  "SELECT/DESLECT_CARD",
  "SEND_IF_COND",
  "SEND_CSD",
  "SEND_CID", // 10
  "RESERVED",
  "STOP_TRANSMISSION",
  "SEND_STATUS",
  "RESERVED",
  "GO_INACTIVE_STATE", //15
  "SET_BLOCKLEN",
  "READ_SINGLE_BLOCK",
  "READ_MULTIPLE_BLOCK",
  "",
  "", // 20
  "",
  "",
  "",
  "WRITE_BLOCK",
  "WRITE_MULTIPLE_BLOCK", // 25
  "",
  "PROGRAM_CSD",
  "SET_WRITE_PROT",
  "CLR_WRITE_PROT",
  "SEND_WRITE_PROT", // 30
  "",
  "ERASE_WR_BLK_START",
  "ERASE_WR_BLK_END",
  "",
  "", // 35
  "",
  "",
  "ERASE",
  "",
  "", // 40
  "",
  "LOCK_UNLOCK",
  "",
  "",
  "", // 45
  "",
  "",
  "",
  "",
  "", // 50
  "",
  "IO_RW_DIRECT",
  "IO_RW_EXTENDED",
  "",
  "APP_CMD", // 55
  "GEN_CMD",
  "",
  "",
  "",
  "", // 60FFFFFFFFCC92C9E5
  "",
  "",
  "", // 63
};



void setfilename(const char *filename)
{
    off64_t filesize;
    volatile uint64_t d;
    uint64_t tmp;

    printf("Opening file %s\n", filename);
    ptr = open(filename, O_RDONLY);  // r for read, b for binary
    ptr = fdopen(ptr, "rb");
    if (ptr)
    {
        if (fseeko64(ptr, 0, SEEK_END))
        {
            printf('Determination the filesize failed\n');
            exit(-2);
        }
        filesize = ftello64(ptr);
        rewind(ptr);
        printf("File size: %lld\n", filesize);
        samplecount = filesize / BYTESPERSAMPLE;
        printf("Sample count: %d\n", samplecount);
        bufferstartsample = 0xf0000000ul;
    }
    else
    {
        printf('unable to open file');
        exit(-1);
    }
}

void closefile(void)
{
    fclose(ptr);
}

bool get_sample(uint32_t samplenumber, uint64_t *sampletime, uint8_t *psample)
{
    /* check, if samplenumber is in current buffer */
    if ( !((samplenumber >= bufferstartsample) && (samplenumber < (bufferstartsample+BUFFER_SAMPLECOUNT) )) )
    {
        uint32_t bytesleft, cursize;
        bufferstartsample = (samplenumber/BUFFER_SAMPLECOUNT) * BUFFER_SAMPLECOUNT;

        fseeko64(ptr, (uint64_t)bufferstartsample*BYTESPERSAMPLE, SEEK_SET);
        bytesleft = (samplecount*BYTESPERSAMPLE) - bufferstartsample*BYTESPERSAMPLE;
        cursize = bytesleft;
        if (cursize > sizeof(buffer))
        {
            cursize = sizeof(buffer);
        }

        fread(buffer, cursize, 1, ptr);
    }
    if (BYTESPERSAMPLE == 9ull)
    {
        memcpy(sampletime, &buffer[(samplenumber-bufferstartsample)*BYTESPERSAMPLE], 8);
    }
    else
    {
        *sampletime = samplenumber;
    }


    *psample = buffer[(samplenumber-bufferstartsample)*BYTESPERSAMPLE+(BYTESPERSAMPLE-1)];
}

uint8_t lastcmd = 0xff;
bool    lastcmd_write = false;
uint16_t get_bitcount(uint8_t bits[])
{
    uint16_t res;

    res = 0;
    res |= bits[2]<<5;
    res |= bits[3]<<4;
    res |= bits[4]<<3;
    res |= bits[5]<<2;
    res |= bits[6]<<1;
    res |= bits[7]<<0;
    if (bits[1] == 0) /* This is a card response*/
    {
        if ((lastcmd == 2) || (lastcmd == 10))
            return 136; /* CID/CSD are 136 bit in length */
        else
            return 48;
    }
    else
    { /* SDIO command */
        lastcmd = res;

        lastcmd_write = bits[8]; /* 0 = READ, 1 = WRITE */
        return 48;
    }
}

uint32_t getbs(uint8_t bits[], uint16_t messagelen, uint16_t startbit, uint8_t bitcount)
{
    uint16_t i;
    uint32_t decoded;

    startbit = messagelen - startbit - 1;

    decoded = 0;
    while (bitcount > 0)
    {
        decoded <<= 1;
        decoded |= bits[startbit++];
        bitcount--;
    }
    return decoded;
}

void decodecommand(uint8_t bits[], uint16_t bitcounter)
{
    uint8_t cmdnum;

    cmdnum = getbs(bits, bitcounter, 45, 6);
    //printf("DECODE\t");
    printf("CMD%d\t", cmdnum);
    printf("%s\t", CMDNAMES[cmdnum]);
    if (cmdnum == 53)
    {
        if ( getbs(bits, bitcounter, 45-6, 1))
            printf("WRITE\t");
        else
            printf("READ\t");
        printf("FunctionNumber %d\t", getbs(bits, bitcounter, 45-7, 3));

        if ( getbs(bits, bitcounter, 45-10, 1))
            printf("BlockMode\t");
        else
            printf("ByteMode\t");

        if ( getbs(bits, bitcounter, 45-11, 1))
            printf("IncrementAddress\t");
        else
            printf("FixedAddress\t");

        printf("RegAddr=%d\t", getbs(bits, bitcounter, 45-12, 17));

        if (getbs(bits, bitcounter, 45-12-17, 9) == 0)
            printf("Byte/BlockCount=0 (=512)\t");
        else
            printf("Byte/BlockCount=%d\t", getbs(bits, bitcounter, 45-12-17, 9));
    }
    else if (cmdnum == 52)
    {
        if ( getbs(bits, bitcounter, 45-6, 1))
            printf("WRITE\t");
        else
            printf("READ\t");
        printf("FunctionNumber=%d\t", getbs(bits, bitcounter, 45-7, 3));
        printf("RegAddr=%d\t", getbs(bits, bitcounter, 45-12, 17));
        printf("Data=%d\t", getbs(bits, bitcounter, 45-30, 8));
    }
    else
    {
        printf("ARG=%d\t", getbs(bits, bitcounter, 39, 32));
    }
    printf("CRC7=%d\t", getbs(bits, bitcounter, 7, 7));

    printf("\n");
}

void decoderesponse(uint8_t bits[], uint16_t bitcounter)
{
    uint8_t cmdnum, tmp, cnt;
    cmdnum = getbs(bits, bitcounter, 45, 6);

    if (lastcmd == 52)
    {
        printf("CmdIdx%d\t", cmdnum);
        printf("ResponseFlagsBits=");
        if (bits[45-6-16-0]) printf("COM_CRC_ERROR ");
        if (bits[45-6-16-1]) printf("ILLEGAL_COMMAND ");
        tmp = getbs(bits, bitcounter, 45-6-16-2, 2);
        if (tmp == 0)
            printf("IO_CURRENT_STATE=DIS ");
        else if (tmp == 1)
            printf("IO_CURRENT_STATE=CMD ");
        else if (tmp == 2)
            printf("IO_CURRENT_STATE=TRN ");
        else if (tmp == 3)
            printf("IO_CURRENT_STATE=RFU ");
        if (bits[45-6-16-2]) printf("IO_CURRENT_STATE ");
        if (bits[45-6-16-3]) printf(" ");

        if (bits[45-6-16-4]) printf("ERROR ");
        if (bits[45-6-16-5]) printf("RFU ");
        if (bits[45-6-16-6]) printf("FUNCTION_NUMBER ");
        if (bits[45-6-16-7]) printf("OUT_OF_RANGE ");

        printf("Data %d\t", getbs(bits, bitcounter, 7+8, 8));
    }
    printf("CRC7=%d\t", getbs(bits, bitcounter, 7, 7));

    printf("\n");
}

void decoddata(uint8_t nibbles[], uint32_t datbitcounter, uint64_t dattimestamp)
{
    uint32_t i;
    printf("DAT\t");
    printf("%3.9fs\t", ((float)dattimestamp)/(sample_frequency_MHz*1.0e6f));
    for(i=0; i<datbitcounter; i++)
    {
        printf("%1x", nibbles[i]);
        if (i == 0)
            printf(" ");
        if (i == datbitcounter-2)
            printf(" ");
    }
    printf("\n");
}


static inline uint8_t getnibble(uint8_t data)
{
    uint8_t val;
    val = 0;

    if (data & bm_channel_d0)
        val |= 1;
    if (data & bm_channel_d1)
        val |= 2;
    if (data & bm_channel_d2)
        val |= 4;
    if (data & bm_channel_d3)
        val |= 8;

    return val;
}


void process_file(void)
{
    uint32_t i, j;
    uint64_t sampletime;
    uint8_t oldsampleval, sampleval;
    uint32_t cnt;
    int16_t totalbitcount;
    int16_t bitcounter;
    uint64_t cmdtimestamp, dattimestamp;
    uint8_t bits[1024];
    uint8_t nibbles[1024*1024];
    int32_t datbitcounter, dattotalbitcount;

    printf("got here\n");

    printf("Amount of samples: %d\n", samplecount);

    // preload oldsamplevalue
    get_sample(0, &sampletime, &oldsampleval);
    cnt = 0;
    bitcounter = -1; // we are not in a packet decoding now
    datbitcounter = -1;
    // parse full file
    for (i=1; i<samplecount; i++)
    {
        get_sample(i, &sampletime, &sampleval);

        // check for rising edge of CLK line
        if (  (sampleval    & bm_channel_clk) &&
             ((oldsampleval & bm_channel_clk)==0)    )
        {
            cnt++;
            if ((bitcounter == -1) && (!(sampleval & bm_channel_cmd)) )
            { // check, if we received startbit
                totalbitcount = 48;
                bitcounter = 0;
                bits[bitcounter++] = 0;
                cmdtimestamp = sampletime;
            }
            else if (bitcounter >= 0)
            {
                if (sampleval & bm_channel_cmd)
                {
                    bits[bitcounter++] = 1;
                }
                else
                {
                    bits[bitcounter++] = 0;
                }
                if (bitcounter == 10)
                {
                    totalbitcount = get_bitcount(bits);
                    /* update totalbitcount to expected value */
                }

                if (bitcounter == totalbitcount)
                {
                    if (bits[1] == 1)
                        printf("CMD \t");
                    else
                        printf("RESP\t");
                    printf("%3.9fs\t", ((float)cmdtimestamp)/(sample_frequency_MHz*1.0e6f));
                    for (j=0; j<totalbitcount; j++)
                    {
                        if (bits[j])
                            printf("1"); // FE
                        else
                            printf("0"); // FE
                    }
                    printf("\t");

                    if (bits[1] == 1)
                        decodecommand(bits, bitcounter);
                    else
                        decoderesponse(bits, bitcounter);

                    bitcounter = -1;
                }
            }


        }
        // check for falling edge of CLK line
        /* for WRITE commands, sample on falling edge, for READ, sample on rising edge */
        if  ( (  lastcmd_write && ( (sampleval    & bm_channel_clk) && ((oldsampleval & bm_channel_clk)==0) ) ) ||
              ( !lastcmd_write && ( ((sampleval    & bm_channel_clk)==0) && (oldsampleval & bm_channel_clk) ) )    )
        {
            if ( (datbitcounter == -1) && (getnibble(sampleval) == 0) )
            { /* start of data transfer detected */
                dattotalbitcount = (512+9) * 2;
                datbitcounter = 0;
                nibbles[datbitcounter++] = getnibble(sampleval);
                dattimestamp = sampletime;
            }
            else if (datbitcounter > 0)
            {
                nibbles[datbitcounter++] = getnibble(sampleval);
                if (datbitcounter == dattotalbitcount)
                {
                    if (lastcmd == 53)
                    {
                        decoddata(nibbles, datbitcounter, dattimestamp);
                    }
                    datbitcounter = -1;
                }
            }
        }

        oldsampleval = sampleval;
    }
    printf("\ndone, rising edges on CLK: %d!\n", cnt);

}



int main(int argc, char* argv[])
{
    if (argc != 9)
    {
        printf("argc: %d %s\n", argc, argv[0]);
        printf("Usage:\n");
        printf("SDIOAnalyzer <FILENAME> <Samplerate[MHz]> <SDIO CLK channel> <SDIO CMD channel> <SDIO D0 channel> <SDIO D1 channel> <SDIO D2 channel> <SDIO D3 channel>\n");
        printf("Example:\n");
        printf("SDIOAnalyzer export.bin 250 7 6 0 1 2 3\n\n");

        return -1;
    }
    else
    {
        setfilename(argv[1]);
        sample_frequency_MHz = atof(argv[2]);
        bm_channel_clk = 1<<atoi(argv[3]);
        bm_channel_cmd = 1<<atoi(argv[4]);
        bm_channel_d0  = 1<<atoi(argv[5]);
        bm_channel_d1  = 1<<atoi(argv[6]);
        bm_channel_d2  = 1<<atoi(argv[7]);
        bm_channel_d3  = 1<<atoi(argv[8]);

        printf("Response/Command/Data\ttimestamp\tRaw Bits\tDecoded information\n");
        process_file();
        closefile();
    }
    return 0;
}
