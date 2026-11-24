#pragma once
#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <cstdint>
class QueueDevice;

class AudioOutput final : public QObject {
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput() override;
    bool start();
    QAudioFormat format() const;
    void writePcm(const uint8_t *data, int bytes);
    void stop();
    void setVolume(float volume);
    qint64 readQueuedData(char *data, qint64 maxSize);
private:
    friend class QueueDevice;
    QAudioSink *sink_ = nullptr;
    QIODevice *device_ = nullptr;
    QAudioFormat format_;
    QByteArray queue_;
    QMutex mutex_;
    QWaitCondition ready_;
    bool stopping_ = false;
    float volume_ = 1.0f;
};
