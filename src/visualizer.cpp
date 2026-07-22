#include "visualizer.h"

#include <QPainter>
#include <QPainterPath>
#include <QAudioFormat>
#include <QtMath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QTimerEvent>
#include <QFileInfo>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

// ════════════════════════════════════════════════════════════
//  静态成员
// ════════════════════════════════════════════════════════════

QVector<double> AudioVisualizer::s_hannWindow;

QVector<double> AudioVisualizer::buildHannWindow(int size)
{
    QVector<double> w(size);
    for (int i = 0; i < size; ++i)
        w[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (size - 1)));
    return w;
}

// ════════════════════════════════════════════════════════════
//  音频探测初始化（Qt6 使用 QAudioDecoder 解码文件）
// ════════════════════════════════════════════════════════════

void AudioVisualizer::initAudioProbe()
{
    m_decoder = new QAudioDecoder(this);
    connect(m_decoder, &QAudioDecoder::bufferReady,
            this, &AudioVisualizer::onDecoderBufferReady);
    connect(m_decoder, &QAudioDecoder::finished,
            this, &AudioVisualizer::onDecoderFinished);
}

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

AudioVisualizer::AudioVisualizer(QMediaPlayer *player, QWidget *parent)
    : QWidget(parent)
    , m_mediaPlayer(player)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 初始化音频探测
    initAudioProbe();

    // 控制按钮栏
    setupControlBar();
    // 跟踪播放状态变化（暂停时完整冻结可视化）
    if (m_mediaPlayer) {
        connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
                this, [this](QMediaPlayer::PlaybackState state) {
            m_isPlaying = (state == QMediaPlayer::PlayingState);
        });
        m_isPlaying = (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState);
    }
    // 启动动画定时器（24fps ≈43ms）
    m_animTimerId = startTimer(43, Qt::PreciseTimer);
}

AudioVisualizer::~AudioVisualizer()
{
    if (m_animTimerId)
        killTimer(m_animTimerId);
    if (m_decoder)
        m_decoder->stop();
}

// ════════════════════════════════════════════════════════════
//  控制按钮栏
// ════════════════════════════════════════════════════════════

void AudioVisualizer::setupControlBar()
{
    m_btnBar = new QWidget(this);
    m_btnBar->setFixedHeight(36);

    auto *layout = new QHBoxLayout(m_btnBar);
    layout->setContentsMargins(12, 2, 12, 2);
    layout->setSpacing(6);

    auto makeBtn = [&](const QString &text, int flag) -> QPushButton* {
        auto *btn = new QPushButton(text, m_btnBar);
        btn->setCheckable(true);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255,255,255,0.08);"
            "  color: #aaa; border: 1px solid #444;"
            "  border-radius: 14px; padding: 0 16px;"
            "  font-size: 12px; font-weight: bold;"
            "}"
            "QPushButton:checked {"
            "  background: rgba(255,82,82,0.25);"
            "  color: #ff5252; border-color: #ff5252;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(255,255,255,0.15);"
            "  color: #fff;"
            "}"));
        connect(btn, &QPushButton::clicked, this, [this, flag]() {
            onModeBtnClicked(flag);
        });
        layout->addWidget(btn);
        return btn;
    };

    m_btnSpec = makeBtn(QStringLiteral("频谱"),   Spectrum);
    m_btnWave = makeBtn(QStringLiteral("波形"),   Waveform);
    m_btnPart = makeBtn(QStringLiteral("粒子"),   ParticleMode);
    m_btnSpec->setChecked(true);
    m_btnWave->setChecked(true);
    m_btnPart->setChecked(true);

    layout->addStretch();

    m_btnClose = new QPushButton(QStringLiteral("✕"), m_btnBar);
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent; color: #666;"
        "  border: none; border-radius: 14px;"
        "  font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 28, 28, 0.3); color: #ff5252;"
        "}"));
    connect(m_btnClose, &QPushButton::clicked, this, [this]() {
        hide();
        if (auto *pw = qobject_cast<QWidget*>(parent()))
            pw->adjustSize();
    });
}

void AudioVisualizer::setDisplayMode(int flags)
{
    m_displayFlags = flags;
    m_btnSpec->setChecked(flags & Spectrum);
    m_btnWave->setChecked(flags & Waveform);
    m_btnPart->setChecked(flags & ParticleMode);
    emit displayModeChanged(flags);
}

void AudioVisualizer::onModeBtnClicked(int flag)
{
    int newFlags = m_displayFlags;
    if (m_displayFlags & flag) {
        // 至少保留一个模式
        if ((m_displayFlags & ~flag) != 0)
            newFlags &= ~flag;
    } else {
        newFlags |= flag;
    }
    setDisplayMode(newFlags);
}

// ════════════════════════════════════════════════════════════
//  音频解码器（Qt6 路径）
// ════════════════════════════════════════════════════════════

void AudioVisualizer::setSourceFile(const QString &filePath)
{
    m_sourcePath = filePath;
    m_decodedBuffer.clear();
    m_decodedBytes = 0;
    m_decChannels = 2;
    m_decSampleRate = 44100;
    m_decoderFinished = false;

    if (!m_decoder || !QFileInfo::exists(filePath))
        return;

    m_decoder->stop();
    m_decoder->setSource(QUrl::fromLocalFile(filePath));
    m_decoder->start();
}

void AudioVisualizer::onDecoderBufferReady()
{
    if (!m_decoder) return;

    const QAudioBuffer buf = m_decoder->read();
    const QAudioFormat fmt = buf.format();
    const int channels = fmt.channelCount();
    const int sampleRate = fmt.sampleRate();
    const int byteCount = buf.byteCount();
    const QAudioFormat::SampleFormat sampleFmt = fmt.sampleFormat();

    // 将解码后的 PCM 数据转换为统一的 16-bit 有符号小端格式
    QByteArray converted;
    converted.reserve(byteCount);

    const char *src = buf.constData<char>();

    if (sampleFmt == QAudioFormat::Int16) {
        // 已经是 Int16，直接使用
        converted = QByteArray(src, byteCount);
    } else if (sampleFmt == QAudioFormat::Float) {
        // 32-bit float → 16-bit int
        const float *fSrc = reinterpret_cast<const float*>(src);
        int sampleCount = byteCount / sizeof(float);
        converted.resize(sampleCount * sizeof(qint16));
        auto *dst = reinterpret_cast<qint16*>(converted.data());
        for (int i = 0; i < sampleCount; ++i)
            dst[i] = static_cast<qint16>(qBound(-1.0f, fSrc[i], 1.0f) * 32767.0f);
    } else if (sampleFmt == QAudioFormat::Int32) {
        // 32-bit int → 16-bit int
        const qint32 *iSrc = reinterpret_cast<const qint32*>(src);
        int sampleCount = byteCount / sizeof(qint32);
        converted.resize(sampleCount * sizeof(qint16));
        auto *dst = reinterpret_cast<qint16*>(converted.data());
        for (int i = 0; i < sampleCount; ++i)
            dst[i] = static_cast<qint16>(iSrc[i] >> 16);
    } else {
        // 不支持的格式，跳过
        return;
    }

    // 记录声道和采样率
    m_decChannels = channels;
    m_decSampleRate = sampleRate;

    // 追加到完整缓冲区（不限制大小，支撑整首歌曲）
    m_decodedBuffer.append(converted);
    m_decodedBytes = m_decodedBuffer.size();

    // 解码期间也实时处理
    processPcmData(converted, sampleRate, channels);
}

void AudioVisualizer::onDecoderFinished()
{
    m_decoderFinished = true;
    m_decodedBytes = m_decodedBuffer.size();
}

// ════════════════════════════════════════════════════════════
//  按播放位置提取帧（解码完成后驱动可视化）
// ════════════════════════════════════════════════════════════

void AudioVisualizer::feedFrameAtPosition()
{
    if (m_decodedBytes < 64 || m_decChannels == 0)
        return;

    qint64 posMs = m_mediaPlayer->position();
    qint64 durMs = m_mediaPlayer->duration();
    if (durMs <= 0) return;

    // 计算当前播放位置在缓冲区中的字节偏移
    double bytesPerMs = double(m_decodedBytes) / durMs;
    int byteOffset = qBound(0, int(posMs * bytesPerMs), m_decodedBytes - 1);

    // 取一帧：4096 字节 ≈ 1024 个 16-bit 采样（单声道）
    const int frameBytes = qMin(4096, m_decodedBytes - byteOffset);
    if (frameBytes < 64) return;

    const QByteArray frame = m_decodedBuffer.mid(byteOffset, frameBytes);
    processPcmData(frame, m_decSampleRate, m_decChannels);
}

// ════════════════════════════════════════════════════════════
//  音频数据处理
// ════════════════════════════════════════════════════════════

void AudioVisualizer::processPcmData(const QByteArray &data,
                                     int sampleRate, int channels)
{
    m_sampleRate = sampleRate;

    // 数据必须为 16-bit 有符号小端
    const int bytesPerSample = 2;
    const int sampleCount = data.size() / bytesPerSample;
    if (sampleCount < channels * 2) return;

    const qint16 *samples = reinterpret_cast<const qint16*>(data.constData());

    // ── 参考 getPcmDB16 方法计算音量 ——
    // avg = sum(abs(value)) / n,  db = 20·log10(avg)
    {
        double sumAbs = 0.0;
        int totalFrames = sampleCount / channels;
        for (int i = 0; i < totalFrames; ++i) {
            double frameVal = 0.0;
            for (int ch = 0; ch < channels; ++ch)
                frameVal += std::abs(samples[i * channels + ch]);
            sumAbs += frameVal / channels;
        }
        double avgAmplitude = sumAbs / totalFrames;
        // avgAmplitude 范围 0~32768, 20*log10(32768) ≈ 90.3 dB
        if (avgAmplitude > 1.0) {
            double db = 20.0 * std::log10(avgAmplitude);
            // 映射 -10..80 dB 到 0..1（80 dB = 满量程附近）
            m_volumeLevel = std::clamp((db + 10.0) / 70.0, 0.0, 1.0);
        } else {
            m_volumeLevel = 0.0;
        }
    }
    m_smoothVolume += (m_volumeLevel - m_smoothVolume) * 0.3;

    // 降采样波形数据（取所有声道平均）
    const int waveLen = 256;
    m_waveformData.resize(waveLen);
    const int frameCount = sampleCount / channels;
    const int step = qMax(1, frameCount / waveLen);
    for (int i = 0; i < waveLen; ++i) {
        int frameIdx = i * step;
        if (frameIdx < frameCount) {
            double avg = 0.0;
            for (int ch = 0; ch < channels; ++ch)
                avg += samples[frameIdx * channels + ch];
            m_waveformData[i] = avg / (channels * 32768.0);
        } else {
            m_waveformData[i] = 0.0;
        }
    }

    // FFT 用 512 个采样（多声道平均）
    const int fftSize = 512;
    QVector<double> fftSamples(fftSize, 0.0);
    if (s_hannWindow.size() != fftSize)
        s_hannWindow = buildHannWindow(fftSize);

    int fftFrames = qMin(fftSize, frameCount);
    for (int i = 0; i < fftFrames; ++i) {
        double avg = 0.0;
        for (int ch = 0; ch < channels; ++ch)
            avg += samples[i * channels + ch];
        fftSamples[i] = (avg / (channels * 32768.0)) * s_hannWindow[i];
    }

    computeFft(fftSamples);
    detectBeat(m_fftData);
}

void AudioVisualizer::computeFft(QVector<double> &samples)
{
    int n = samples.size();
    // 保证 2 的幂
    int pow2 = 1;
    while (pow2 < n) pow2 <<= 1;
    if (pow2 != n) {
        samples.resize(pow2);
        n = pow2;
    }

    // 位反转排序
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(samples[i], samples[j]);
    }

    // 蝶形运算
    QVector<double> re(n), im(n, 0.0);
    for (int i = 0; i < n; ++i) re[i] = samples[i];

    for (int len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * M_PI / len;
        double wRe = std::cos(ang);
        double wIm = -std::sin(ang);
        for (int i = 0; i < n; i += len) {
            double curRe = 1.0, curIm = 0.0;
            for (int j = 0; j < len / 2; ++j) {
                double uRe = re[i + j];
                double uIm = im[i + j];
                double vRe = re[i + j + len / 2] * curRe - im[i + j + len / 2] * curIm;
                double vIm = re[i + j + len / 2] * curIm + im[i + j + len / 2] * curRe;
                re[i + j] = uRe + vRe;
                im[i + j] = uIm + vIm;
                re[i + j + len / 2] = uRe - vRe;
                im[i + j + len / 2] = uIm - vIm;
                double newRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = newRe;
            }
        }
    }

    // ── 计算幅值谱，然后按频带分组求平均，再做 dB 转换（参考 getPcmDB16） ──
    int half = n / 2;
    QVector<double> mags(half);
    for (int i = 0; i < half; ++i)
        mags[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]) / half;

    // 保留完整 FFT 数据供 detectBeat 使用
    m_fftData = mags;

    // ── 按 3 段频率分段，柱子数按 FFT bin 之比动态计算 ──
    struct FreqSegment {
        double freqLo, freqHi;
    };
    constexpr FreqSegment kSegments[] = {
        {  20.0,  200.0  },   // 低频 20-200 Hz
        { 200.0, 2000.0  },   // 中频 200 Hz-2 kHz
        {2000.0, 20000.0 },   // 高频 2-20 kHz
    };
    constexpr int kSegCount = sizeof(kSegments) / sizeof(kSegments[0]);
    const double nyquist = m_sampleRate > 0 ? m_sampleRate / 2.0 : 22050.0;
    const double binHz = nyquist / half;   // 每个 FFT bin 的频率宽度

    // 先统计各段 bin 数，高频区均匀截取最多 20 个有效 bin
    int segBins[kSegCount];
    int totalBinsAll = 0;
    for (int s = 0; s < kSegCount; ++s) {
        int lo = qMax(1,       static_cast<int>(kSegments[s].freqLo / binHz));
        int hi = qMin(half - 1, static_cast<int>(kSegments[s].freqHi / binHz));
        if (hi < lo) hi = lo;
        segBins[s] = hi - lo + 1;
    }
    // 高频区：从完整 bin 范围中均匀抽取 20 个代表 bin，其余段保持原样
    segBins[kSegCount - 1] = qMin(segBins[kSegCount - 1], 20);
    for (int s = 0; s < kSegCount; ++s)
        totalBinsAll += segBins[s];

    // 按 bin 比例分配 kBarCount 根柱子，每段至少 1 根
    int segBars[kSegCount];
    int assigned = 0;
    for (int s = 0; s < kSegCount - 1; ++s) {
        segBars[s] = qMax(1, static_cast<int>(kBarCount * segBins[s] / totalBinsAll));
        assigned += segBars[s];
    }
    segBars[kSegCount - 1] = kBarCount - assigned;   // 最后一段补齐

    int barIdx = 0;
    for (int seg = 0; seg < kSegCount; ++seg) {
        int binLo = qMax(1,       static_cast<int>(kSegments[seg].freqLo / binHz));
        int binHi = qMin(half - 1, static_cast<int>(kSegments[seg].freqHi / binHz));
        if (binHi < binLo) binHi = binLo;

        int totalBins = binHi - binLo + 1;
        int segBarsCount = segBars[seg];

        // ── 高频区：从完整 bin 中均匀抽取 20 个代表点 ──
        QVector<double> sampledMags;
        if (seg == kSegCount - 1) {
            constexpr int kHfSamples = 20;
            sampledMags.resize(kHfSamples);
            for (int s = 0; s < kHfSamples; ++s) {
                int idx = binLo + s * totalBins / kHfSamples;
                // 取相邻 bins 平均作为代表值
                int r = qMin(totalBins - 1, totalBins / kHfSamples);
                double acc = 0.0;
                int n = 0;
                for (int j = qMax(binLo, idx - r/2); j <= qMin(binHi, idx + r/2); ++j) {
                    acc += mags[j];
                    n++;
                }
                sampledMags[s] = n > 0 ? acc / n : 0.0;
            }
        }

        const double *pMags = (seg == kSegCount - 1) ? sampledMags.constData() : mags.constData();
        int pMagCount = (seg == kSegCount - 1) ? sampledMags.size() : totalBins;

        for (int b = 0; b < segBarsCount && barIdx < kBarCount; ++b, ++barIdx) {
            // 浮点分布避免柱子数 > 代表点数时的缺失问题
            int barLo = (int)( (double)b       * pMagCount / segBarsCount );
            int barHi = (int)( (double)(b + 1) * pMagCount / segBarsCount ) - 1;
            if (barHi < barLo) barHi = barLo;
            barLo = qBound(0, barLo, pMagCount - 1);
            barHi = qBound(barLo, barHi, pMagCount - 1);

            double sum = 0.0;
            int count = 0;
            for (int i = barLo; i <= barHi; ++i) {
                sum += pMags[i];
                count++;
            }

            double avgMag = count > 0 ? sum / count : 0.0;

            // ── ① dB 转换：db = 20·log10(raw_mag + ε)，ε 防止 log(0) ──
            constexpr double kEps = 1e-10;
            m_barRaw[barIdx] = 20.0 * std::log10(avgMag + kEps);
        }
    }
}

void AudioVisualizer::detectBeat(const QVector<double> &fftMag)
{
    // 检测低频段 (前 8 个 bin) 的能量突增
    double lowEnergy = 0.0;
    int lowBins = qMin(8, fftMag.size());
    for (int i = 0; i < lowBins; ++i)
        lowEnergy += fftMag[i];
    lowEnergy /= lowBins;

    // 与历史比较
    static double s_history = 0.0;
    double threshold = s_history * 1.5 + 0.15;
    if (lowEnergy > threshold && s_history > 0.01) {
        m_beatStrength = 1.0;
        m_beatDecay = 20;

        // 爆发粒子
        QColor beatColor(
            200 + QRandomGenerator::global()->bounded(55),
            100 + QRandomGenerator::global()->bounded(100),
            150 + QRandomGenerator::global()->bounded(105));
        emitParticles(12 + QRandomGenerator::global()->bounded(8), beatColor);
    }

    s_history = s_history * 0.9 + lowEnergy * 0.1;
    if (m_beatDecay > 0) {
        m_beatStrength = m_beatDecay / 20.0;
        --m_beatDecay;
    } else {
        m_beatStrength *= 0.95;
    }
}

// ════════════════════════════════════════════════════════════
//  粒子系统
// ════════════════════════════════════════════════════════════

void AudioVisualizer::emitParticles(int count, const QColor &baseColor)
{
    int w = width();
    int h = height() - m_btnBar->height();
    int centerY = m_btnBar->height() + h / 2;

    for (int i = 0; i < count && m_particles.size() < kMaxParticles; ++i) {
        VisualizerParticle p;
        p.pos = QPointF(w / 2.0 + (QRandomGenerator::global()->bounded(40) - 20), centerY);
        double angle = QRandomGenerator::global()->bounded(360) * M_PI / 180.0;
        double speed = 1.0 + QRandomGenerator::global()->bounded(100) / 50.0;
        p.vel = QPointF(std::cos(angle) * speed, std::sin(angle) * speed - 2.0);
        p.life = 1.0f;
        p.maxLife = 0.6f + QRandomGenerator::global()->bounded(40) / 100.0f;
        p.color = baseColor;
        p.size = 2.0f + QRandomGenerator::global()->bounded(4);
        m_particles.append(p);
    }
}

void AudioVisualizer::updateParticles(float dt)
{
    for (int i = m_particles.size() - 1; i >= 0; --i) {
        auto &p = m_particles[i];
        p.vel += QPointF(0, 5.0f * dt);          // 重力
        p.pos += p.vel * dt * 60.0f;
        p.life -= dt / p.maxLife;
        if (p.life <= 0 || p.pos.y() > height() + 20) {
            m_particles.remove(i);
        }
    }
}

// ════════════════════════════════════════════════════════════
//  绘制
// ════════════════════════════════════════════════════════════

void AudioVisualizer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 透明背景
    p.fillRect(rect(), QColor(0, 0, 0, 40));

    int barHeight = m_btnBar->height();
    int areaTop = barHeight;
    int areaW = width();
    int areaH = height() - barHeight;

    if (areaH <= 0 || areaW <= 0) return;

    int activeModes = 0;
    if (m_displayFlags & Spectrum) activeModes++;
    if (m_displayFlags & Waveform) activeModes++;
    if (m_displayFlags & ParticleMode) activeModes++;
    if (activeModes == 0) return;

    // 计算每个区域的高度（等分）
    int cellH = areaH / activeModes;
    int curY = areaTop;

    if (m_displayFlags & Spectrum) {
        QRect cellRect(0, curY, areaW, cellH);
        drawSpectrum(p, cellRect);
        curY += cellH;
    }
    if (m_displayFlags & Waveform) {
        QRect cellRect(0, curY, areaW, cellH);
        drawWaveform(p, cellRect);
        curY += cellH;
    }
    if (m_displayFlags & ParticleMode) {
        QRect cellRect(0, curY, areaW, areaH - (curY - areaTop));
        drawParticles(p, cellRect);
    }
}

// ════════════════════════════════════════════════════════════
//  频谱柱状图
// ════════════════════════════════════════════════════════════

void AudioVisualizer::drawSpectrum(QPainter &p, const QRect &rect)
{
    // ════════════════════════════════════════════════════════════════
    //  Y 轴计算管线（全链路重写）
    // ════════════════════════════════════════════════════════════════
    //
    //  ① dB 转换    → 已于 computeFft 中完成，m_barRaw 存的是原始 dB 值
    //  ② 范围裁剪    → min_db ~ max_db
    //  ③ 归一化      → (db - min_db) / (max_db - min_db)  → [0, 1]
    //  ④ 指数曲线    → y = normalized ^ power
    //  ⑤ 平滑（EMA） → smoothed = factor * y + (1-factor) * prev
    //  ⑥ 像素映射    → pixel_h = smoothed * max_height
    // ════════════════════════════════════════════════════════════════

    constexpr double kMinDb     = -80.0;
    constexpr double kMaxDb     =   0.0;
    constexpr double kDbRange   = kMaxDb - kMinDb;   // 80
    constexpr double kPower     =   1.0;              // 指数曲线（1=线性）
    constexpr double kSmooth    =   0.35;             // EMA 平滑因子

    for (int b = 0; b < kBarCount; ++b) {
        // ② 范围裁剪
        double db = qMax(m_barRaw[b], kMinDb);

        // ③ 归一化到 [0, 1]
        double normalized = (db - kMinDb) / kDbRange;

        // ④ 指数曲线调整
        double curved = std::pow(normalized, kPower);

        // ⑤ EMA 平滑（替换旧的 attack/decay）
        m_barLevel[b] = kSmooth * curved + (1.0 - kSmooth) * m_barLevel[b];
        if (m_barLevel[b] < 1e-6) m_barLevel[b] = 0.0;

        // 峰值跟踪
        if (m_barLevel[b] >= m_barPeak[b]) {
            m_barPeak[b] = m_barLevel[b];
            m_barPeakAge[b] = 0;
        } else {
            m_barPeakAge[b]++;
            if (m_barPeakAge[b] > 25)
                m_barPeak[b] *= 0.95;
        }
    }

    // ⑥ 映射到像素高度
    const float totalW = float(rect.width());
    const float barW   = totalW / kBarCount;
    const float gap    = barW * 0.15f;
    const float drawW  = barW - gap * 2;
    const float hMax   = rect.height() * 0.88f;

    p.save();
    p.setClipRect(rect);

    for (int b = 0; b < kBarCount; ++b) {
        double val = m_barLevel[b];
        float barH = float(val) * hMax;
        float x = rect.x() + b * barW + gap;
        float y = rect.bottom() - barH;

        // ── 频率→颜色 冷→暖渐变 ──
        float hue = 0.58f - float(b) / kBarCount * 0.48f;
        float sat = 0.70f + float(val) * 0.30f;
        float lig = 0.35f + float(val) * 0.55f;
        QColor baseColor;
        baseColor.setHsvF(hue, sat, lig);
        QColor peakColor = baseColor;
        peakColor.setAlpha(120);

        // 柱子
        QLinearGradient grad(x, y, x, rect.bottom());
        grad.setColorAt(0.0, baseColor.lighter(180));
        grad.setColorAt(0.2, baseColor);
        grad.setColorAt(1.0, baseColor.darker(130));
        p.fillRect(QRectF(x, y, drawW, barH), grad);

        

        // ── 峰值标记：同色半透明方块 ──
        if (m_barPeak[b] > 0.02f) {
            float peakY = rect.bottom() - float(m_barPeak[b]) * hMax;
            float blockSize = drawW * 0.6f;
            p.fillRect(QRectF(x + (drawW - blockSize) / 2,
                              peakY - blockSize / 2,
                              blockSize, blockSize),
                       peakColor);
        }
    }

    p.restore();
}

// ════════════════════════════════════════════════════════════
//  圆形波形图
// ════════════════════════════════════════════════════════════

void AudioVisualizer::drawWaveform(QPainter &p, const QRect &rect)
{
    if (m_waveformData.isEmpty()) return;

    p.save();
    p.setClipRect(rect);

    int cx = rect.center().x();
    int cy = rect.center().y();
    int baseRadius = qMin(rect.width(), rect.height()) / 3;

    // 呼吸缩放
    double breathe = 0.85 + m_smoothVolume * 0.15;
    int radius = static_cast<int>(baseRadius * breathe);

    int n = m_waveformData.size();
    QPainterPath path;

    // 外圈波形
    for (int i = 0; i <= n; ++i) {
        int idx = i % n;
        double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
        double r = radius + m_waveformData[idx] * radius * 0.4;
        double x = cx + r * std::cos(angle);
        double y = cy + r * std::sin(angle);
        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    path.closeSubpath();

    // 霓虹渐变
    QLinearGradient grad(rect.left(), rect.top(), rect.right(), rect.bottom());
    grad.setColorAt(0.0, QColor(0, 200, 255, 180));
    grad.setColorAt(0.5, QColor(255, 100, 200, 180));
    grad.setColorAt(1.0, QColor(100, 255, 100, 180));

    // 发光效果
    QPen glowPen(QColor(0, 200, 255, 40), radius * 0.3);
    p.setPen(glowPen);
    p.drawPath(path);

    // 主线条
    QPen pen(grad, 2.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(path);

    // 中心光晕
    QRadialGradient centerGlow(cx, cy, radius * 0.3);
    centerGlow.setColorAt(0.0, QColor(255, 255, 255, 60 + static_cast<int>(m_smoothVolume * 80)));
    centerGlow.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(rect, centerGlow);

    // 内圈小波形
    QPainterPath innerPath;
    for (int i = 0; i <= n; ++i) {
        int idx = i % n;
        double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
        double r = radius * 0.4 + m_waveformData[idx] * radius * 0.15;
        double x = cx + r * std::cos(angle);
        double y = cy + r * std::sin(angle);
        if (i == 0)
            innerPath.moveTo(x, y);
        else
            innerPath.lineTo(x, y);
    }
    innerPath.closeSubpath();

    QPen innerPen(QColor(200, 230, 255, 100), 1.5);
    innerPen.setCapStyle(Qt::RoundCap);
    p.setPen(innerPen);
    p.drawPath(innerPath);

    p.restore();
}

// ════════════════════════════════════════════════════════════
//  节拍粒子系统
// ════════════════════════════════════════════════════════════

void AudioVisualizer::drawParticles(QPainter &p, const QRect &rect)
{
    p.save();
    p.setClipRect(rect);
    p.setPen(Qt::NoPen);

    for (const auto &pt : m_particles) {
        float alpha = qBound(0.0f, pt.life, 1.0f);
        QColor c = pt.color;
        c.setAlpha(static_cast<int>(alpha * 200));

        // 拖尾效果：用较低透明度的更大圆
        if (alpha > 0.3f) {
            QColor trail = c;
            trail.setAlpha(static_cast<int>(alpha * 60));
            p.setBrush(trail);
            float trailSize = pt.size * 3.0f * (1.0f + (1.0f - alpha));
            p.drawEllipse(pt.pos, trailSize, trailSize);
        }

        p.setBrush(c);
        p.drawEllipse(pt.pos, pt.size, pt.size);
    }

    // 无粒子时显示提示
    if (m_particles.isEmpty()) {
        p.setPen(QColor(100, 100, 100, 150));
        QFont f = font();
        f.setPointSize(11);
        p.setFont(f);
        p.drawText(rect, Qt::AlignCenter,
                   QStringLiteral("♪ 等待节拍"));
    }

    p.restore();
}

// ════════════════════════════════════════════════════════════
//  动画定时器
// ════════════════════════════════════════════════════════════

void AudioVisualizer::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_animTimerId) {
        // 双重确认：信号驱动 + 直接查询，确保暂停时真正冻结
        const bool actuallyPlaying = m_mediaPlayer &&
            (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState);

        if (actuallyPlaying) {
            // ── 播放中：正常更新粒子、音频数据、节拍 ──
            updateParticles(1.0f / 24.0f);
            if (m_decoderFinished && m_decodedBytes > 0)
                feedFrameAtPosition();
            m_beatStrength *= 0.97;
            m_isPlaying = true;
        } else {
            // ── 暂停/停止：同步 m_isPlaying，完整冻结 ──
            m_isPlaying = false;
        }
        // 保持重绘以显示最后一帧
        update();
    }
    QWidget::timerEvent(event);
}

void AudioVisualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 控制按钮栏置于顶部
    if (m_btnBar)
        m_btnBar->setGeometry(0, 0, width(), 36);
}
