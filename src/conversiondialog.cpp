#include "conversiondialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QDebug>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

ConversionDialog::ConversionDialog(const QString &songPath,
                                   const QString &songTitle,
                                   QWidget *parent)
    : QDialog(parent)
    , m_songPath(songPath)
    , m_songTitle(songTitle)
{
    setObjectName(QStringLiteral("ConversionDialog"));
    setWindowTitle(QStringLiteral("格式转换"));
    setMinimumSize(420, 260);
    setMaximumSize(520, 360);

    m_converter = new FormatConverter(this);

    connect(m_converter, &FormatConverter::progressChanged,
            this, &ConversionDialog::onProgressChanged);
    connect(m_converter, &FormatConverter::conversionFinished,
            this, &ConversionDialog::onConversionFinished);
    connect(m_converter, &FormatConverter::conversionError,
            this, &ConversionDialog::onConversionError);

    showFormatSelection();
}

ConversionDialog::~ConversionDialog() = default;

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void ConversionDialog::showFormatSelection()
{
    // 清除旧内容
    if (layout()) {
        QLayout *oldLayout = layout();
        // 递归删除子部件
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 标题
    m_titleLabel = new QLabel(
        QStringLiteral("选择输出格式\n歌曲: %1").arg(m_songTitle), this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(12);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    mainLayout->addSpacing(8);

    // 格式选择
    auto *fmtLayout = new QHBoxLayout;
    fmtLayout->addStretch();

    auto *fmtLabel = new QLabel(QStringLiteral("输出格式:"), this);
    fmtLayout->addWidget(fmtLabel);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems(FormatConverter::formatDisplayNames());
    m_formatCombo->setMinimumWidth(140);
    fmtLayout->addWidget(m_formatCombo);

    fmtLayout->addStretch();
    mainLayout->addLayout(fmtLayout);

    mainLayout->addSpacing(16);

    // 按钮
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_startBtn = new QPushButton(QStringLiteral("开始转换"), this);
    m_startBtn->setMinimumWidth(100);
    m_startBtn->setDefault(true);
    connect(m_startBtn, &QPushButton::clicked, this, &ConversionDialog::onStartConversion);
    btnLayout->addWidget(m_startBtn);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // 调整窗口
    adjustSize();
}

void ConversionDialog::showProgress()
{
    // 清除并切换到进度界面
    if (layout()) {
        QLayout *oldLayout = layout();
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 状态文字
    m_statusLabel = new QLabel(
        QStringLiteral("正在转换: %1").arg(m_songTitle), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(11);
    m_statusLabel->setFont(statusFont);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addSpacing(12);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(28);
    mainLayout->addWidget(m_progressBar);

    mainLayout->addSpacing(12);

    // 按钮
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_cancelBtn->setMinimumWidth(80);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ConversionDialog::onCancel);
    btnLayout->addWidget(m_cancelBtn);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    adjustSize();
}

void ConversionDialog::showResult(bool success, const QString &message)
{
    // 清除旧内容
    if (layout()) {
        QLayout *oldLayout = layout();
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 状态图标 + 消息
    m_statusLabel = new QLabel(message, this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont f = m_statusLabel->font();
    f.setPointSize(success ? 12 : 11);
    m_statusLabel->setFont(f);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addSpacing(16);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_closeBtn = new QPushButton(
        success ? QStringLiteral("关闭") : QStringLiteral("返回"), this);
    m_closeBtn->setMinimumWidth(80);
    m_closeBtn->setDefault(true);
    connect(m_closeBtn, &QPushButton::clicked, this, [this, success]() {
        if (success)
            accept();   // 成功则关闭对话框
        else
            showFormatSelection();  // 失败可重新选择
    });
    btnLayout->addWidget(m_closeBtn);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    adjustSize();
}

// ════════════════════════════════════════════════════════════
//  槽
// ════════════════════════════════════════════════════════════

void ConversionDialog::onStartConversion()
{
    if (m_converting)
        return;

    const QString fmtName = m_formatCombo->currentText();
    const auto fmt = FormatConverter::formatFromDisplayName(fmtName);

    // 检查是否选择了与源文件相同的格式
    const QFileInfo fi(m_songPath);
    const QString srcSuffix = fi.suffix().toLower();
    const QString dstSuffix = FormatConverter::formatSuffix(fmt).mid(1).toLower();
    if (srcSuffix == dstSuffix && srcSuffix == QStringLiteral("mp3")) {
        // 同格式转换给出提示但仍允许
    }

    m_converting = true;

    // 切换到进度界面
    showProgress();

    // 触发转换开始提示
    m_statusLabel->setText(
        QStringLiteral("⏳ 开始转换: %1\n目标格式: %2")
            .arg(m_songTitle, FormatConverter::formatDisplayName(fmt)));

    // 开始异步转换
    m_converter->convert(m_songPath, fmt);
}

void ConversionDialog::onProgressChanged(int percent)
{
    if (!m_progressBar)
        return;

    m_lastPercent = percent;
    m_progressBar->setValue(percent);

    if (percent < 100) {
        m_statusLabel->setText(
            QStringLiteral("正在转换: %1\n进度: %2%")
                .arg(m_songTitle).arg(percent));
    }
}

void ConversionDialog::onConversionFinished(bool success, const QString &outputPath)
{
    m_converting = false;

    if (success) {
        const QFileInfo fi(outputPath);
        const QString sizeStr = fi.size() > 1024 * 1024
            ? QStringLiteral("%1 MB").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 1)
            : QStringLiteral("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 0);

        showResult(true,
            QStringLiteral("✅ 转换完成！\n\n输出文件:\n%1\n\n文件大小: %2")
                .arg(outputPath, sizeStr));
    }
}

void ConversionDialog::onConversionError(const QString &errorMessage)
{
    m_converting = false;

    showResult(false,
        QStringLiteral("❌ 转换失败\n\n%1").arg(errorMessage));
}

void ConversionDialog::onCancel()
{
    if (m_converting) {
        m_converter->cancel();
        m_converting = false;
        reject();  // 直接关闭对话框
    }
}
