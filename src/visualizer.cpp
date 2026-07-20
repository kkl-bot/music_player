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

    // 启动动画定时器（60fps ≈ 16.6ms）
    m_animTimerId = startTimer(16, Qt::PreciseTimer);
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

    // 计算音量（RMS，取所有声道平均）
    double rms = 0.0;
    {
        double sum = 0.0;
        int validFrames = 0;
        for (int i = 0; i + channels <= sampleCount; i += channels) {
            double s = 0.0;
            for (int ch = 0; ch < channels; ++ch)
                s += samples[i + ch] / 32768.0;
            s /= channels;
            sum += s * s;
            validFrames++;
        }
        rms = validFrames > 0 ? std::sqrt(sum / validFrames) : 0.0;
    }
    m_volumeLevel = std::min(rms * 4.0, 1.0);
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

    // 计算幅值谱（只取前一半）
    int half = n / 2;
    m_fftData.resize(half);
    for (int i = 0; i < half; ++i) {
        m_fftData[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]) / half;
        // 转 dB
        if (m_fftData[i] > 1e-10)
            m_fftData[i] = 20.0 * std::log10(m_fftData[i]);
        else
            m_fftData[i] = -80.0;
        // 归一化到 0~1
        m_fftData[i] = std::clamp((m_fftData[i] + 80.0) / 80.0, 0.0, 1.0);
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
    if (m_fftData.isEmpty()) return;

    // ── 动态频率范围检测 ──
    // 从 FFT 数据中找到有实际能量的频段，使运动柱子居中占据 ≥80%
    const int fftLen = m_fftData.size();
    constexpr double kEnergyThresh = 0.04;

    int binStart = 1;   // 跳过 DC (bin 0)
    int binEnd   = fftLen - 1;

    // 找到第一个有能量的 bin
    while (binStart < fftLen - 2 && m_fftData[binStart] < kEnergyThresh)
        binStart++;

    // 找到最后一个有能量的 bin（从后往前）
    while (binEnd > binStart + 5 && m_fftData[binEnd] < kEnergyThresh)
        binEnd--;

    // 如果整个范围太小或者检测失败，回退到一个合理的默认范围
    int activeRange = binEnd - binStart;
    if (activeRange < 10) {
        binStart = 2;
        binEnd   = qMin(fftLen / 2, 200);
        activeRange = binEnd - binStart;
    }

    // 动态收窄：再取能量集中区域（去掉头尾 5% 的边缘）
    int margin = activeRange / 20;  // 5%
    binStart += margin;
    binEnd   -= margin;
    activeRange = binEnd - binStart;
    if (activeRange < 8) {
        binStart = 2;
        binEnd   = qMin(fftLen / 2, 180);
        activeRange = binEnd - binStart;
    }

    // ── 对数频率带分组 ──
    // 将活动频段映射到 kBarCount 个柱子上
    double bandEnergy[kBarCount] = {};
    for (int b = 0; b < kBarCount; ++b) {
        // 用 sigmoid-like 曲线让柱子分布更均匀地覆盖活动频段
        double f = double(b) / kBarCount;
        double curve = f * f * (3.0 - 2.0 * f);  // smoothstep: 中间密、两端疏
        int idx = binStart + int(curve * activeRange);
        idx = qBound(binStart, idx, binEnd);
        bandEnergy[b] = m_fftData[idx];
    }

    // ── Attack / Decay 平滑 ──
    constexpr double kAttack = 0.50;
    constexpr double kDecay  = 0.88;
    constexpr double kPeakDecay = 0.95;
    constexpr int    kPeakMaxAge = 25;

    for (int b = 0; b < kBarCount; ++b) {
        double raw = bandEnergy[b];
        if (raw > m_barLevel[b])
            m_barLevel[b] += (raw - m_barLevel[b]) * kAttack;
        else
            m_barLevel[b] += (raw - m_barLevel[b]) * (1.0 - kDecay);
        m_barLevel[b] = qBound(0.0, m_barLevel[b], 1.0);

        if (m_barLevel[b] >= m_barPeak[b]) {
            m_barPeak[b] = m_barLevel[b];
            m_barPeakAge[b] = 0;
        } else {
            m_barPeakAge[b]++;
            if (m_barPeakAge[b] > kPeakMaxAge)
                m_barPeak[b] *= kPeakDecay;
        }
    }

    // ── 绘制 ──
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

        // ── 顶部光晕 ──
        if (barH > 3) {
            QRadialGradient glow(x + drawW / 2, y, drawW * 2.5f);
            QColor glowColor = baseColor;
            glowColor.setAlpha(70);
            glow.setColorAt(0.0, glowColor);
            glowColor.setAlpha(0);
            glow.setColorAt(1.0, glowColor);
            p.fillRect(QRectF(x - drawW, y - 3, drawW * 3, 10), glow);
        }

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
        // 更新粒子
        updateParticles(1.0f / 60.0f);

        // ── 解码完成后，从缓冲区按播放位置持续提取数据 ──
        if (m_decoderFinished && m_decodedBytes > 0 && m_mediaPlayer)
            feedFrameAtPosition();

        // 触发重绘
        update();
        m_beatStrength *= 0.97;
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
