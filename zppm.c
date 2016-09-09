#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define HEADER_SIZE 25
#define BUFFER_SIZE 4096
#define IMAGE_WIDTH 512

int
main(void)
{
    char bufin[BUFFER_SIZE];
    char bufout[BUFFER_SIZE];

    /* Determine header size and skip over it. */
    z_stream z = {
        .next_in = (void *)bufin,
        .avail_in = HEADER_SIZE,
        .next_out = (void *)bufout,
        .avail_out = sizeof(bufout),
    };
    deflateInit(&z, Z_NO_COMPRESSION);
    memset(bufin, 0, HEADER_SIZE);
    deflate(&z, Z_FULL_FLUSH); // assume output buffer is sufficiently large
    if (fseek(stdout, sizeof(bufout) - z.avail_out, SEEK_SET)) {
        fprintf(stderr, "zppm: stdout must be seekable\n");
        return -1;
    }

    /* Compress all input. */
    uint64_t bytes_in = 0;
    deflateParams(&z, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY);
    z.adler = adler32(0, 0, 0); // reset checksum
    do {
        z.next_out = (void *)bufout;
        z.avail_out = sizeof(bufout);
        if (z.avail_in == 0) {
            z.next_in = (void *)bufin;
            z.avail_in = fread(bufin, 1, sizeof(bufin), stdin);
            bytes_in += z.avail_in;
            if (feof(stdin)) {
                /* Pad out last pixel row. */
                size_t padding = bytes_in % (3 * IMAGE_WIDTH);
                memset(bufin + z.avail_in, 0, padding);
                z.avail_in += padding;
                bytes_in += padding;
            }
        }
        deflate(&z, Z_NO_FLUSH);
        fwrite(bufout, 1, sizeof(bufout) - z.avail_out, stdout);
    } while (!feof(stdin));

    /* Flush all output, but don't Z_FINISH yet. */
    do {
        z.next_out = (void *)bufout;
        z.avail_out = sizeof(bufout);
        deflate(&z, Z_SYNC_FLUSH);
        fwrite(bufout, 1, sizeof(bufout) - z.avail_out, stdout);
    } while (z.avail_out == 0);

    /* Go back and rewrite the header. */
    uint64_t height = bytes_in / (3 * IMAGE_WIDTH);
    sprintf(bufin, "P6\n%-6d\n%-10" PRIu64 "\n255\n", IMAGE_WIDTH, height);
    uint32_t adler = adler32(adler32(0, 0, 0), (void *)bufin, HEADER_SIZE);
    z_stream zh = {
        .next_in = (void *)bufin,
        .avail_in = HEADER_SIZE,
        .next_out = (void *)bufout,
        .avail_out = sizeof(bufout),
    };
    deflateInit(&zh, Z_NO_COMPRESSION);
    deflate(&zh, Z_FULL_FLUSH);
    fseek(stdout, 0, SEEK_SET);
    fwrite(bufout, 1, sizeof(bufout) - zh.avail_out, stdout);
    fseek(stdout, 0, SEEK_END);
    deflateEnd(&zh);

    /* Set corrected checksum and finish. */
    z.adler = adler32_combine(adler, z.adler, bytes_in);
    z.next_out = (void *)bufout;
    z.avail_out = sizeof(bufout);
    deflate(&z, Z_FINISH); // assume output buffer is sufficiently large
    fwrite(bufout, 1, sizeof(bufout) - z.avail_out, stdout);
    deflateEnd(&z);
    return 0;
}
