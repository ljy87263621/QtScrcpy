#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

/* Runtime smoke check against the exact libraries shipped in the package. */
int main(int argc, char **argv)
{
    AVFormatContext *input = NULL;
    AVCodecContext *decoder = NULL;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int count = 0;
    if (argc != 2 || !packet || !frame) return 2;
    if (avformat_open_input(&input, argv[1], NULL, NULL) < 0 || avformat_find_stream_info(input, NULL) < 0) return 3;
    decoder = avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_H264));
    if (!decoder || avcodec_parameters_to_context(decoder, input->streams[0]->codecpar) < 0 || avcodec_open2(decoder, decoder->codec, NULL) < 0) return 4;
    do {
        int read = av_read_frame(input, packet);
        if (avcodec_send_packet(decoder, read < 0 ? NULL : packet) < 0) return 5;
        av_packet_unref(packet);
        while (avcodec_receive_frame(decoder, frame) == 0) {
            struct SwsContext *scale = sws_getContext(frame->width, frame->height, (enum AVPixelFormat)frame->format,
                frame->width, frame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
            uint8_t *rgb[4] = { NULL, NULL, NULL, NULL };
            int stride[4] = { frame->width * 3, 0, 0, 0 };
            rgb[0] = av_malloc((size_t)stride[0] * frame->height);
            if (!scale || !rgb[0] || sws_scale(scale, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, rgb, stride) != frame->height) return 6;
            av_free(rgb[0]); sws_freeContext(scale); ++count;
        }
        if (read < 0) break;
    } while (1);
    av_frame_free(&frame); av_packet_free(&packet); avcodec_free_context(&decoder); avformat_close_input(&input);
    printf("Decoded and converted %d frames; %s\n", count, avcodec_license());
    return count == 10 ? 0 : 7;
}
