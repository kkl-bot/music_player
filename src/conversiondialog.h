#ifndef CONVERSIONDIALOG_H
#define CONVERSIONDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFileInfo>

#include "formatconverter.h"

/// ConversionDialog - 格式转换对话框
///
/// 显示支持的输出格式列表供用户选择，
/// 选择后调用 FormatConverter 异步转码，
/// 实时显示进度条，开始/结束时均有状态提示。
class ConversionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConversionDialog(const QString &songPath,
                              const QString &songTitle,
                              QWidget *parent = nullptr);
    ~ConversionDialog() override;

private slots:
    void onStartConversion();
    void onProgressChanged(int percent);
    void onConversionFinished(bool success, const QString &outputPath);
    void onConversionError(const QString &errorMessage);
    void onCancel();

private:
    void showFormatSelection();
    void showProgress();
    void showResult(bool success, const QString &message);

    QString m_songPath;
    QString m_songTitle;
    FormatConverter *m_converter = nullptr;

    // ── 选择界面 ──
    QLabel    *m_titleLabel  = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QPushButton *m_startBtn  = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    // ── 进度界面 ──
    QLabel      *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton  *m_closeBtn   = nullptr;

    // ── 状态 ──
    bool m_converting = false;
    int  m_lastPercent = 0;
};

#endif // CONVERSIONDIALOG_H
