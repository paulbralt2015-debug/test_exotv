#include "VideoEngine.h"
#include "AudioOutput.h"
#include <QMetaObject>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

VideoEngine::VideoEngine(QObject *p) : QObject(p), audio_(new AudioOutput(this)) {}
VideoEngine::~VideoEngine() { stop(); }

bool VideoEngine::open(const QString &path) {
    stop();
    AVFormatContext *fmt = nullptr;
    QByteArray utf8 = path.toUtf8();
    if (avformat_open_input(&fmt, utf8.constData(), nullptr, nullptr) < 0) return false;
    if (avformat_find_stream_info(fmt, nullptr) < 0) { avformat_close_input(&fmt); return false; }
    duration_ = fmt->duration > 0 ? double(fmt->duration) / AV_TIME_BASE : 0.0;
    avformat_close_input(&fmt);
    path_ = path;
    emit durationChanged(duration_);
    return true;
}
void VideoEngine::play() {
    if (path_.isEmpty()) return;
    if (running_) { paused_ = false; return; }
    running_ = true; paused_ = false;
    worker_ = std::thread(&VideoEngine::decodeLoop, this);
}
void VideoEngine::pause() { paused_ = !paused_; }
void VideoEngine::stop() {
    running_ = false; paused_ = false;
    if (worker_.joinable()) worker_.join();
    audio_->stop();
}
void VideoEngine::seek(double s) { seekTarget_ = qBound(0.0, s, duration_); }

void VideoEngine::decodeLoop() {
    AVFormatContext *fmt = nullptr;
    QByteArray utf8 = path_.toUtf8();
    if (avformat_open_input(&fmt, utf8.constData(), nullptr, nullptr) < 0 ||
        avformat_find_stream_info(fmt, nullptr) < 0) {
        if (fmt) avformat_close_input(&fmt);
        emit errorOccurred("FFmpeg ne peut pas ouvrir ce média.");
        running_ = false; return;
    }
    int vi = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    int ai = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (vi < 0) { avformat_close_input(&fmt); emit errorOccurred("Aucune piste vidéo détectée."); running_=false; return; }
    auto makeDecoder = [&](int index)->AVCodecContext* {
        if (index < 0) return nullptr;
        AVCodecParameters *par = fmt->streams[index]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (!codec) return nullptr;
        AVCodecContext *ctx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(ctx, par);
        if (avcodec_open2(ctx, codec, nullptr) < 0) { avcodec_free_context(&ctx); return nullptr; }
        return ctx;
    };
    AVCodecContext *vc = makeDecoder(vi), *ac = makeDecoder(ai);
    if (!vc) { avformat_close_input(&fmt); running_=false; emit errorOccurred("Décodeur vidéo indisponible."); return; }
    SwsContext *sws = sws_getContext(vc->width, vc->height, vc->pix_fmt, vc->width, vc->height,
                                     AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    SwrContext *swr = nullptr;
    int audioRate=0, audioChannels=0;
    if (ac) {
        audioRate = ac->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        audioChannels = ac->ch_layout.nb_channels;
        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, audioChannels);
        swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_S16, audioRate,
                            &ac->ch_layout, ac->sample_fmt, ac->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&outLayout);
#else
        audioChannels = ac->channels;
        swr = swr_alloc_set_opts(nullptr, av_get_default_channel_layout(audioChannels), AV_SAMPLE_FMT_S16,
                                 audioRate, ac->channel_layout, ac->sample_fmt, ac->sample_rate, 0, nullptr);
#endif
        if (swr) swr_init(swr);
        if (swr && audioRate > 0) audio_->start(audioRate, audioChannels);
    }
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    QElapsedTimer clock; clock.start();
    double basePts = -1.0;
    while (running_) {
        if (paused_) { QThread::msleep(10); continue; }
        double requested = seekTarget_.exchange(-1.0);
        if (requested >= 0) {
            int64_t ts = int64_t(requested * AV_TIME_BASE);
            av_seek_frame(fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(vc); if (ac) avcodec_flush_buffers(ac);
            basePts = -1; clock.restart();
        }
        int ret = av_read_frame(fmt, pkt);
        if (ret < 0) break;
        AVCodecContext *ctx = pkt->stream_index == vi ? vc : (pkt->stream_index == ai ? ac : nullptr);
        if (ctx) {
            avcodec_send_packet(ctx, pkt);
            while (avcodec_receive_frame(ctx, frame) == 0) {
                if (ctx == vc) {
                    double pts = frame->best_effort_timestamp == AV_NOPTS_VALUE ? 0.0 :
                        frame->best_effort_timestamp * av_q2d(fmt->streams[vi]->time_base);
                    if (basePts < 0) { basePts = pts; clock.restart(); }
                    int waitMs = int((pts-basePts)*1000.0 - clock.elapsed());
                    if (waitMs > 0 && waitMs < 1000) QThread::msleep(unsigned(waitMs));
                    QImage image(vc->width, vc->height, QImage::Format_ARGB32);
                    uint8_t *dst[4] = { image.bits(), nullptr, nullptr, nullptr };
                    int lines[4] = { int(image.bytesPerLine()), 0, 0, 0 };
                    sws_scale(sws, frame->data, frame->linesize, 0, vc->height, dst, lines);
                    emit frameReady(image);
                    emit positionChanged(pts);
                } else if (ctx == ac && swr) {
                    int outSamples = int(av_rescale_rnd(swr_get_delay(swr, ac->sample_rate) + frame->nb_samples,
                                      audioRate, ac->sample_rate, AV_ROUND_UP));
                    QByteArray pcm(outSamples * audioChannels * 2, Qt::Uninitialized);
                    uint8_t *out[] = { reinterpret_cast<uint8_t*>(pcm.data()) };
                    int n = swr_convert(swr, out, outSamples, (const uint8_t**)frame->extended_data, frame->nb_samples);
                    if (n > 0) audio_->writePcm(reinterpret_cast<const uint8_t*>(pcm.constData()), n*audioChannels*2);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(pkt);
    }
    running_ = false;
    audio_->stop();
    av_frame_free(&frame); av_packet_free(&pkt);
    if (swr) swr_free(&swr);
    if (sws) sws_freeContext(sws);
    if (ac) avcodec_free_context(&ac);
    avcodec_free_context(&vc);
    avformat_close_input(&fmt);
    emit playbackFinished();
}
