#pragma once
#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <cstdint>
class AudioOutput final : public QObject {
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput() override;
    bool start(int sampleRate, int channels);
    void writePcm(const uint8_t *data, int bytes);
    void stop();
    void setVolume(float volume);
private:
    QAudioSink *sink_ = nullptr;
    QIODevice *device_ = nullptr;
    QByteArray queue_;
    QMutex mutex_;
    QWaitCondition ready_;
    bool stopping_ = false;
};
