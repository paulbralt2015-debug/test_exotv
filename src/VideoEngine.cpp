#include "VideoEngine.h"
#include "AudioOutput.h"
#include <QByteArray>
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
    const QByteArray utf8 = path.toUtf8();
    if (avformat_open_input(&fmt, utf8.constData(), nullptr, nullptr) < 0) return false;
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return false;
    }
    duration_ = fmt->duration > 0 ? double(fmt->duration) / AV_TIME_BASE : 0.0;
    avformat_close_input(&fmt);
    path_ = path;
    emit durationChanged(duration_);
    return true;
}

double VideoEngine::duration() const { return duration_; }

void VideoEngine::play() {
    if (path_.isEmpty()) return;
    if (running_) { paused_ = false; return; }
    if (worker_.joinable()) worker_.join();

    if (!audio_->start())
        emit errorOccurred("Aucune sortie audio Windows exploitable. Vérifie le périphérique audio par défaut.");
    running_ = true;
    paused_ = false;
    worker_ = std::thread(&VideoEngine::decodeLoop, this);
}

void VideoEngine::pause() { if (running_) paused_ = !paused_.load(); }

void VideoEngine::stop() {
    running_ = false;
    paused_ = false;
    if (worker_.joinable()) worker_.join();
    audio_->stop();
}

void VideoEngine::seek(double s) { seekTarget_ = qBound(0.0, s, duration_); }
void VideoEngine::setVolume(float v) { audio_->setVolume(v); }

static AVSampleFormat toAvSampleFormat(QAudioFormat::SampleFormat f) {
    switch (f) {
    case QAudioFormat::UInt8: return AV_SAMPLE_FMT_U8;
    case QAudioFormat::Int16: return AV_SAMPLE_FMT_S16;
    case QAudioFormat::Int32: return AV_SAMPLE_FMT_S32;
    case QAudioFormat::Float: return AV_SAMPLE_FMT_FLT;
    default: return AV_SAMPLE_FMT_NONE;
    }
}

void VideoEngine::decodeLoop() {
    AVFormatContext *fmt = nullptr;
    const QByteArray utf8 = path_.toUtf8();
    if (avformat_open_input(&fmt, utf8.constData(), nullptr, nullptr) < 0 ||
        avformat_find_stream_info(fmt, nullptr) < 0) {
        if (fmt) avformat_close_input(&fmt);
        emit errorOccurred("FFmpeg ne peut pas ouvrir ce média.");
        running_ = false;
        return;
    }

    const int vi = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int ai = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (vi < 0) {
        avformat_close_input(&fmt);
        emit errorOccurred("Aucune piste vidéo détectée dans ce fichier.");
        running_ = false;
        return;
    }

    auto makeDecoder = [&](int index)->AVCodecContext* {
        if (index < 0) return nullptr;
        AVCodecParameters *par = fmt->streams[index]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (!codec) return nullptr;
        AVCodecContext *ctx = avcodec_alloc_context3(codec);
        if (!ctx) return nullptr;
        if (avcodec_parameters_to_context(ctx, par) < 0 || avcodec_open2(ctx, codec, nullptr) < 0) {
            avcodec_free_context(&ctx);
            return nullptr;
        }
        return ctx;
    };

    AVCodecContext *vc = makeDecoder(vi);
    AVCodecContext *ac = makeDecoder(ai);
    if (!vc) {
        if (ac) avcodec_free_context(&ac);
        avformat_close_input(&fmt);
        running_ = false;
        emit errorOccurred("Décodeur vidéo FFmpeg indisponible pour le codec de ce fichier.");
        return;
    }

    SwsContext *sws = sws_getContext(vc->width, vc->height, vc->pix_fmt,
                                     vc->width, vc->height, AV_PIX_FMT_BGRA,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
        if (ac) avcodec_free_context(&ac);
        avcodec_free_context(&vc);
        avformat_close_input(&fmt);
        running_ = false;
        emit errorOccurred("Initialisation du convertisseur vidéo impossible.");
        return;
    }

    const QAudioFormat deviceFormat = audio_->format();
    const int outputRate = deviceFormat.sampleRate();
    const int outputChannels = deviceFormat.channelCount();
    const AVSampleFormat outputSampleFormat = toAvSampleFormat(deviceFormat.sampleFormat());
    SwrContext *swr = nullptr;
    if (ac && outputSampleFormat != AV_SAMPLE_FMT_NONE && ac->sample_rate > 0) {
#if LIBAVUTIL_VERSION_MAJOR >= 57
        AVChannelLayout outputLayout;
        av_channel_layout_default(&outputLayout, outputChannels);
        AVChannelLayout inputLayout = ac->ch_layout;
        if (inputLayout.nb_channels == 0)
            av_channel_layout_default(&inputLayout, ac->ch_layout.nb_channels > 0 ? ac->ch_layout.nb_channels : 2);
        const int swrResult = swr_alloc_set_opts2(&swr, &outputLayout, outputSampleFormat, outputRate,
                                                   &inputLayout, ac->sample_fmt, ac->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&outputLayout);
        if (inputLayout.nb_channels != ac->ch_layout.nb_channels || ac->ch_layout.nb_channels == 0)
            av_channel_layout_uninit(&inputLayout);
        if (swrResult < 0) swr = nullptr;
#else
        const int64_t outLayout = av_get_default_channel_layout(outputChannels);
        const int64_t inLayout = ac->channel_layout ? ac->channel_layout :
                                  av_get_default_channel_layout(ac->channels);
        swr = swr_alloc_set_opts(nullptr, outLayout, outputSampleFormat, outputRate,
                                 inLayout, ac->sample_fmt, ac->sample_rate, 0, nullptr);
#endif
        if (swr && swr_init(swr) < 0) swr_free(&swr);
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    QElapsedTimer clock;
    clock.start();
    double basePts = -1.0;
    const int bytesPerSample = outputSampleFormat == AV_SAMPLE_FMT_NONE ? 0 :
                               av_get_bytes_per_sample(outputSampleFormat);
    while (running_) {
        if (paused_) { QThread::msleep(10); continue; }
        const double requested = seekTarget_.exchange(-1.0);
        if (requested >= 0.0) {
            const int64_t ts = int64_t(requested * AV_TIME_BASE);
            av_seek_frame(fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(vc);
            if (ac) avcodec_flush_buffers(ac);
            if (swr) swr_close(swr), swr_init(swr);
            basePts = -1.0;
            clock.restart();
        }

        const int ret = av_read_frame(fmt, pkt);
        if (ret < 0) break;
        AVCodecContext *ctx = pkt->stream_index == vi ? vc : (pkt->stream_index == ai ? ac : nullptr);
        if (ctx) {
            const int sendResult = avcodec_send_packet(ctx, pkt);
            if (sendResult >= 0 || sendResult == AVERROR(EAGAIN)) {
                while (avcodec_receive_frame(ctx, frame) == 0) {
                    if (ctx == vc) {
                        const double pts = frame->best_effort_timestamp == AV_NOPTS_VALUE ? 0.0 :
                            frame->best_effort_timestamp * av_q2d(fmt->streams[vi]->time_base);
                        if (basePts < 0.0) { basePts = pts; clock.restart(); }
                        const int waitMs = int((pts-basePts)*1000.0 - clock.elapsed());
                        if (waitMs > 0 && waitMs < 1000) QThread::msleep(unsigned(waitMs));
                        QImage image(vc->width, vc->height, QImage::Format_ARGB32);
                        uint8_t *dst[4] = { image.bits(), nullptr, nullptr, nullptr };
                        int lines[4] = { int(image.bytesPerLine()), 0, 0, 0 };
                        sws_scale(sws, frame->data, frame->linesize, 0, vc->height, dst, lines);
                        emit frameReady(image);
                        emit positionChanged(pts);
                    } else if (ctx == ac && swr && bytesPerSample > 0) {
                        const int outSamples = int(av_rescale_rnd(
                            swr_get_delay(swr, ac->sample_rate) + frame->nb_samples,
                            outputRate, ac->sample_rate, AV_ROUND_UP));
                        QByteArray pcm(outSamples * outputChannels * bytesPerSample, Qt::Uninitialized);
                        uint8_t *out[] = { reinterpret_cast<uint8_t*>(pcm.data()) };
                        const int n = swr_convert(swr, out, outSamples,
                                                  const_cast<const uint8_t **>(frame->extended_data),
                                                  frame->nb_samples);
                        if (n > 0) audio_->writePcm(reinterpret_cast<const uint8_t*>(pcm.constData()),
                                                    n * outputChannels * bytesPerSample);
                    }
                    av_frame_unref(frame);
                }
            }
        }
        av_packet_unref(pkt);
    }

    running_ = false;
    av_frame_free(&frame);
    av_packet_free(&pkt);
    if (swr) swr_free(&swr);
    sws_freeContext(sws);
    if (ac) avcodec_free_context(&ac);
    avcodec_free_context(&vc);
    avformat_close_input(&fmt);
    emit playbackFinished();
}
