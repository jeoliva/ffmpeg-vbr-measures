#include <libavformat/avformat.h>

#define DEFAULT_BUFDEFAULT_BUFFER_LENFER_LEN 2 * 1024 * 1024 // 2 MB

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;
static unsigned long bufferLen = DEFAULT_BUFFER_LEN;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;

// Measure helper variables
static unsigned long partialSize = 0;
static int64_t partialDuration = 0;
static float bps, maxBps = -1, minBps = -1, accBps = 0.0f;
static unsigned long measuresCount = 0;

static int open_video_stream(AVFormatContext *fmt_ctx)
{
  int ret;
  enum AVMediaType type = AVMEDIA_TYPE_VIDEO;

  ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
  if (ret < 0) {
    fprintf(stderr, "Could not find %s stream in input file '%s'\n",
            av_get_media_type_string(type), src_filename);
    return ret;
  } else {
    video_stream_idx = ret;
    video_stream = fmt_ctx->streams[video_stream_idx];
  }

  return 0;
}

static void do_calculation(AVPacket *pkt) {
  partialSize += pkt->size;
  partialDuration += pkt->duration;

  if (pkt->side_data != NULL) {
    partialSize += pkt->side_data->size;
  }
  if (partialSize > bufferLen) {
    bps = (partialSize * 8) / (av_q2d(video_stream->time_base) * partialDuration);
    if (minBps == -1) {
      minBps = bps;
    } else {
      minBps = MIN(minBps, bps);
    }
    if (maxBps == -1) {
      maxBps = bps;
    } else {
      maxBps = MAX(maxBps, bps);
    }
    accBps += bps;
    measuresCount++;
    fprintf(stderr, "Measure: %.2f kbps\n", bps / 1024.0f);
    partialSize = 0;
    partialDuration = 0;
  }
}

int main(int argc, char **argv)
{
  int ret = 0;
  AVPacket pkt = { 0 };

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <video> [buffer size in KB]\n", argv[0]);
    exit(1);
  }
  src_filename = argv[1];

  if (argc >= 3) {
    bufferLen = 1024 * atol(argv[2]);
    if (bufferLen <= 0) {
      bufferLen = DEFAULT_BUFFER_LEN;
    }
  }

  av_register_all();

  if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open source file %s\n", src_filename);
    exit(1);
  }

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  open_video_stream(fmt_ctx);

  if (!video_stream) {
    fprintf(stderr, "Could not find video stream in the input, aborting\n");
    ret = 1;
    goto end;
  }

  frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "Could not allocate frame\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  /* read frames from the file */
  while (av_read_frame(fmt_ctx, &pkt) >= 0) {
    if (pkt.stream_index == video_stream_idx) {
      do_calculation(&pkt);
    }
    av_packet_unref(&pkt);
  }

  // calculate remaining data (if there is something)
  do_calculation(&pkt);
  float avgBps = accBps / (1024.0f * measuresCount);
  maxBps /= 1024.0f;
  minBps /= 1024.0f;
  fprintf(stderr, "Avg Bitrate: %.2f kbps, Max: %.2f (%.2f %%), Min: %.2f (%.2f %%), Max var: %.2f %%\n",
          avgBps, maxBps, 100 * (maxBps - avgBps) / avgBps, minBps,
          100 * (minBps - avgBps) / avgBps, 100 * (maxBps - minBps) / minBps);

  end:
  avcodec_free_context(&video_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_frame_free(&frame);
  return ret < 0;
}
