#include "fleeteffects.h"

#include <QPainter>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QRandomGenerator>
#include <QFileInfo>
#include <QMovie>
#include <QResizeEvent>
#include <cmath>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

FleetEffects::FleetEffects(QWidget *parentWidget)
    : QObject(parentWidget)
    , m_parent(parentWidget)
{
    initParticleImages();
    initOverlay();
    initPet();
    startTimers();
}

FleetEffects::~FleetEffects()
{
    stop();
}

// ════════════════════════════════════════════════════════════
//  启动 / 停止
// ════════════════════════════════════════════════════════════

void FleetEffects::start()
{
    m_enabled = true;
    if (m_overlay) {
        // 先更新覆盖层大小以匹配当前窗口尺寸
        m_overlay->setGeometry(m_parent->rect());
        m_overlay->raise();
        m_overlay->show();
    }
    if (m_petLabel) {
        m_petLabel->raise();
        m_petLabel->show();
    }
    if (m_fallTimer) m_fallTimer->start();
    if (m_animTimer) m_animTimer->start();
}

void FleetEffects::stop()
{
    m_enabled = false;
    if (m_fallTimer) m_fallTimer->stop();
    if (m_animTimer) m_animTimer->stop();
    if (m_overlay)   m_overlay->hide();
    if (m_petMovie)  m_petMovie->stop();
    if (m_petLabel)  m_petLabel->hide();
    m_fallParticles.clear();
    m_burstParticles.clear();
}

void FleetEffects::setEnabled(bool on)
{
    on ? start() : stop();
}

// ════════════════════════════════════════════════════════════
//  初始化
// ════════════════════════════════════════════════════════════

void FleetEffects::initParticleImages()
{   
    auto findpng = [](const QString &name) -> QString {
        const QStringList dirs = {
            QStringLiteral("assets/"),
            QStringLiteral("../assets/"),
            QStringLiteral("../../assets/"),
        };
        for (const auto &d : dirs) {
            QString path = d + name;
            if (QFileInfo::exists(path))
                return path;
        }
        return {};
    };
    static const QColor kFallbackColors[] = {
        QColor(255,183,197), QColor(160,196,255),
        QColor(200,150,255), QColor(255,220,150),
    };
    for (int i = 0; i < 4; ++i) {
        QPixmap pm(findpng(QString::fromLatin1("icon%1.png").arg(i)));
        if (pm.isNull()) {
            // 文件不存在时回退到内建像素方块
            pm = QPixmap(16, 16);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(kFallbackColors[i]);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(2, 2, 12, 12, 3, 3);
            p.end();
        }
        m_particleImages.append(pm);
    }
}   



void FleetEffects::initPet()
{
    m_petLabel = new QLabel(m_parent);
    m_petLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_petLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    m_petLabel->resize(80, 80);
    m_petLabel->setAlignment(Qt::AlignCenter);
    m_petLabel->hide();

    // 查找可用 GIF 素材
    auto findGif = [](const QString &name) -> QString {
        const QStringList dirs = {
            QStringLiteral("assets/Aemeath/"),
            QStringLiteral("../assets/Aemeath/"),
            QStringLiteral("../../assets/Aemeath/"),
        };
        for (const auto &d : dirs) {
            QString path = d + name;
            if (QFileInfo::exists(path))
                return path;
        }
        return {};
    };
    m_petGifFly   = findGif(QStringLiteral("Aemeath_FLY.gif"));
    m_petGifGlass = findGif(QStringLiteral("Aemeath_GLASS.gif"));
    m_petGifJump  = findGif(QStringLiteral("Aemeath_JUMP.gif"));
    m_petGifUp    = findGif(QStringLiteral("Aemeath_UP.gif"));

    // 无素材用 emoji 占位
    if (m_petGifFly.isEmpty()) {
        m_petLabel->setText(QStringLiteral("🌸"));
        m_petLabel->setStyleSheet(QStringLiteral("background:transparent;font-size:48px;"));
        return;
    }
    // 初始显示 idle
    setPetGif(m_petGifGlass);
}

void FleetEffects::startTimers()
{
    // 背景飘落生成定时器（每 1.2s 生成一个）
    m_fallTimer = new QTimer(this);
    m_fallTimer->setInterval(1200);
    connect(m_fallTimer, &QTimer::timeout, this, [this]() {
        spawnFallParticle();
    });

    // 动画帧定时器（60fps）— 同时驱动粒子 + 桌宠
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        updateFallParticles();
        updateBurstParticles();
        updatePet();
        // 每帧同步覆盖层大小和层级，确保覆盖层始终覆盖整个父窗口且在最上层
        if (m_overlay) {
            if (m_overlay->size() != m_parent->size())
                m_overlay->setGeometry(m_parent->rect());
            m_overlay->raise();
            m_overlay->update();
        }
    });
}

// ════════════════════════════════════════════════════════════
//  事件过滤 — 捕获父窗口点击 → 粒子爆散
// ════════════════════════════════════════════════════════════

bool FleetEffects::eventFilter(QObject *obj, QEvent *event)
{
    if (m_enabled && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        // 检查鼠标点击是否发生在当前父窗口内（包括其所有子控件）
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && (w == m_parent || m_parent->isAncestorOf(w))) {
            // 所有位置都触发爆散（不影响控件交互，因为 eventFilter 返回 false 不吞事件）
            QPoint localPos = m_parent->mapFromGlobal(me->globalPosition().toPoint());
            emitBurst(localPos);
        }
    }
    // 返回 false 表示不拦截事件，控件交互不受影响
    return QObject::eventFilter(obj, event);
}

// ════════════════════════════════════════════════════════════
//  鼠标点击 — 粒子爆散
// ════════════════════════════════════════════════════════════

void FleetEffects::onMousePress(const QPoint &localPos)
{
    //if (!m_enabled) return;
    emitBurst(localPos);
}

void FleetEffects::emitBurst(const QPoint &pos)
{
    if (!m_enabled) return;

    int count = 6;
    for (int i = 0; i < count && m_burstParticles.size() < m_maxBurstNodes; ++i) {
        BurstParticle bp;
        bp.pos = QPointF(pos);
        double angle = QRandomGenerator::global()->bounded(2.0 * M_PI);
        double speed = 3.0 + QRandomGenerator::global()->bounded(6.0);
        bp.vel = QPointF(std::cos(angle) * speed, std::sin(angle) * speed);
        bp.life = 1.0f;
        bp.rot = QRandomGenerator::global()->bounded(360.0f);
        bp.imageIndex = QRandomGenerator::global()->bounded(m_particleImages.size());
        m_burstParticles.append(bp);
    }
}

void FleetEffects::updateBurstParticles()
{
    for (int i = m_burstParticles.size() - 1; i >= 0; --i) {
        auto &p = m_burstParticles[i];
        p.pos += p.vel;
        p.vel *= 0.92;
        p.vel += QPointF(0, 0.2);    // 重力
        p.rot += 5;
        p.life -= 0.03f;
        if (p.life <= 0)
            m_burstParticles.remove(i);
    }
}

// ════════════════════════════════════════════════════════════
//  背景飘落粒子
// ════════════════════════════════════════════════════════════

void FleetEffects::spawnFallParticle()
{
    if (!m_enabled || m_fallParticles.size() >= m_maxFallParticles)
        return;

    int w = m_parent->width();
    FallParticle fp;
    fp.pos = QPointF(QRandomGenerator::global()->bounded(w), -30);
    fp.speedY = 0.5 + QRandomGenerator::global()->bounded(1.5);
    fp.driftX = (QRandomGenerator::global()->bounded(200) - 100) / 100.0f * 0.3f;
    fp.rotation = QRandomGenerator::global()->bounded(360.0f);
    fp.opacity = 0.2f + QRandomGenerator::global()->bounded(50) / 100.0f;
    fp.imageIndex = QRandomGenerator::global()->bounded(m_particleImages.size());
    m_fallParticles.append(fp);
}

void FleetEffects::updateFallParticles()
{
    int h = m_parent->height();
    for (int i = m_fallParticles.size() - 1; i >= 0; --i) {
        auto &p = m_fallParticles[i];
        p.pos += QPointF(p.driftX, p.speedY);
        p.rotation += 0.5f;
        if (p.pos.y() > h + 30)
            m_fallParticles.remove(i);
    }
}

// ════════════════════════════════════════════════════════════
//  桌宠
// ════════════════════════════════════════════════════════════

void FleetEffects::setPetGif(const QString &path)
{
    if (path.isEmpty() || path == m_petCurGif) return;
    m_petCurGif = path;

    // 停止旧动画
    if (m_petMovie) {
        m_petMovie->stop();
        m_petLabel->clear();
    }

    auto *movie = new QMovie(path);
    movie->setScaledSize(QSize(80, 80));
    movie->setCacheMode(QMovie::CacheAll);
    if (!movie->isValid()) {
        delete movie;
        // 回退到静态图片
        QPixmap pm(path);
        if (!pm.isNull())
            m_petLabel->setPixmap(pm.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

    m_petMovie = movie;
    m_petLabel->setMovie(m_petMovie);
    m_petMovie->start();
}

void FleetEffects::pickPetTarget()
{
    int pw = m_parent->width();
    int ph = m_parent->height();
    int pad = 60;
    int x = pad + QRandomGenerator::global()->bounded(pw - pad * 2);
    int y = pad + QRandomGenerator::global()->bounded(ph - pad * 2);
    // 避开底部控制栏
    int ctrlY = ph - 90;
    if (y > ctrlY) y = ctrlY - 30;

    m_petTarget = QPointF(x, y);
    // 移动到目标的速度 (像素/秒) 随机
    m_petSpeed = 20.0f + QRandomGenerator::global()->bounded(40);
}

void FleetEffects::applyPetDirection()
{
    // 根据水平方向翻转图片（CSS transform: scaleX）
    if (m_petVel.x() > 0.5f)
        m_petLabel->setStyleSheet(QStringLiteral(
            "background: transparent; transform: scaleX(1);"));
    else if (m_petVel.x() < -0.5f)
        m_petLabel->setStyleSheet(QStringLiteral(
            "background: transparent; transform: scaleX(-1);"));
}

void FleetEffects::updatePet()
{
    if (!m_enabled || !m_petLabel) return;

    if (m_petGifFly.isEmpty()) {
        // 无素材时 emoji 原地呼吸
        return;
    }

    // 首次需要显示
    if (!m_petLabel->isVisible()) {
        m_petLabel->raise();
        m_petLabel->show();
        int pw = m_parent->width();
        int ph = m_parent->height();
        m_petPos = QPointF(QRandomGenerator::global()->bounded(pw),
                           QRandomGenerator::global()->bounded(ph));
        m_petLabel->move(m_petPos.toPoint());
        pickPetTarget();
    }

    // ── 停留 / 移动 ──
    QPointF diff = m_petTarget - m_petPos;
    float dist = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());

    if (m_petWait > 0) {
        // 停留倒计时
        m_petWait--;
        setPetGif(m_petGifGlass);       // idle 动画
        m_petVel = QPointF(0, 0);
        if (m_petWait == 0)
            pickPetTarget();
        return;
    }

    if (dist < 5.0f) {
        // 到达目标 → 停留 1~3 秒
        m_petWait = 60 + QRandomGenerator::global()->bounded(120);  // 60fps 帧数
        setPetGif(m_petGifGlass);
        m_petVel = QPointF(0, 0);
        return;
    }

    // ── 朝目标移动 ──
    float step = m_petSpeed / 60.0f;      // 每帧移动距离
    QPointF dir = diff / dist;
    m_petPos += dir * step;
    m_petVel = dir * m_petSpeed;
    m_petLabel->move(qRound(m_petPos.x()), qRound(m_petPos.y()));

    // ── 根据运动方向切换 GIF ──
    bool movingUp = dir.y() < -0.3f;
    bool movingHoriz = std::abs(dir.x()) > 0.4f;

    if (movingUp && m_petGifUp.isEmpty())
        setPetGif(m_petGifJump);    // 无 UP 时用 JUMP 代替上升
    else if (movingUp)
        setPetGif(m_petGifUp);
    else if (movingHoriz)
        setPetGif(m_petGifFly);
    else
        setPetGif(m_petGifJump);    // 斜下/垂直下用跳跃

    applyPetDirection();
}

// ════════════════════════════════════════════════════════════
//  OverlayWidget — 透明覆盖层，仅绘制粒子，鼠标穿透
// ════════════════════════════════════════════════════════════

class OverlayWidget : public QWidget
{
public:
    FleetEffects *m_effects = nullptr;
    explicit OverlayWidget(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);  // 鼠标穿透
        setAttribute(Qt::WA_AlwaysStackOnTop);
        setStyleSheet(QStringLiteral("background: transparent;"));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (!m_effects) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        for (const auto &fp : m_effects->m_fallParticles) {
            if (fp.imageIndex < 0 || fp.imageIndex >= m_effects->m_particleImages.size())
                continue;
            QPixmap pm = m_effects->m_particleImages[fp.imageIndex];
            p.save();
            p.translate(fp.pos);
            p.rotate(fp.rotation);
            p.setOpacity(fp.opacity);
            p.drawPixmap(-pm.width() / 2, -pm.height() / 2, pm);
            p.restore();
        }
        for (const auto &bp : m_effects->m_burstParticles) {
            if (bp.imageIndex < 0 || bp.imageIndex >= m_effects->m_particleImages.size())
                continue;
            QPixmap pm = m_effects->m_particleImages[bp.imageIndex];
            p.save();
            p.translate(bp.pos);
            p.rotate(bp.rot);
            p.setOpacity(std::max(0.0, (double)bp.life));
            float s = 0.5f + (1.0f - bp.life) * 0.5f;
            p.scale(s, s);
            p.drawPixmap(-pm.width() / 2, -pm.height() / 2, pm);
            p.restore();
        }
    }
};

void FleetEffects::initOverlay()
{
    auto *overlay = new OverlayWidget(m_parent);
    overlay->m_effects = this;
    overlay->setGeometry(m_parent->rect());
    overlay->hide();
    m_overlay = overlay;

    // 在 QApplication 上安装事件过滤器，捕获整个窗口内的点击用于爆散效果
    // 注：不能只装在 m_parent 上，因为 Qt 鼠标事件发给子控件而非父控件
    qApp->removeEventFilter(this);
    qApp->installEventFilter(this);
}
