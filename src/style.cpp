#include "style.h"

// ════════════════════════════════════════════════════════════
//  接口
// ════════════════════════════════════════════════════════════

QString Style::styleSheet(Theme theme)
{
    switch (theme) {
    case Dark:
        return darkStyleSheet();
    default:
        return darkStyleSheet();
    }
}

// ════════════════════════════════════════════════════════════
//  深色主题
// ════════════════════════════════════════════════════════════

QString Style::darkStyleSheet()
{
    // ── 颜色变量 ──
    // 主背景      #1b1b2f    深蓝黑
    // 面板背景    #232340    稍亮
    // 控件栏      #2a2a48
    // 边框        #363658
    // 强调色      #e34d4d    网易云红
    // 强调悬停    #ff6b6b
    // 主文字      #f0f0f0
    // 次要文字    #999999
    // 弱文字      #666666

    return QStringLiteral(R"(
        /* ════════════════════════════════════════════════════════
           全局
           ════════════════════════════════════════════════════════ */
        QMainWindow {
            background-color: #1b1b2f;
        }
        QWidget {
            color: #f0f0f0;
            font-family: "Segoe UI","Microsoft YaHei","PingFang SC",sans-serif;
            font-size: 13px;
        }

        /* ════════════════════════════════════════════════════════
           菜单栏
           ════════════════════════════════════════════════════════ */
        QMenuBar {
            background-color: #232340;
            color: #ccc;
            border-bottom: 1px solid #363658;
            padding: 2px 0;
            font-size: 13px;
        }
        QMenuBar::item {
            padding: 4px 14px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #363658;
            color: #fff;
        }
        QMenu {
            background-color: #232340;
            border: 1px solid #363658;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 16px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #e34d4d;
            color: #fff;
        }
        QMenu::separator {
            height: 1px;
            background: #363658;
            margin: 4px 8px;
        }

        /* ════════════════════════════════════════════════════════
           左侧播放列表
           ════════════════════════════════════════════════════════ */
        QListWidget {
            background-color: #232340;
            border: none;
            border-right: 1px solid #363658;
            font-size: 13px;
            outline: none;
            padding: 4px 0;
        }
        QListWidget::item {
            padding: 7px 14px;
            border-radius: 4px;
            margin: 1px 4px;
            border: none;
        }
        QListWidget::item:selected {
            background-color: #e34d4d;
            color: #fff;
        }
        QListWidget::item:hover:!selected {
            background-color: #2f2f50;
        }

        #searchBox {
            background-color: #2a2a48;
            color: #f0f0f0;
            font-size: 13px;
            border: 1px solid #363658;
            border-radius: 16px;
            padding: 4px 14px;
            margin: 6px 8px;
        }
        #searchBox:focus {
            border-color: #e34d4d;
        }
        #searchBox::placeholder {
            color: #666;
        }

        #countLabel {
            background-color: #232340;
            padding: 4px 14px;
            font-size: 12px;
            color: #666;
            border-top: 1px solid #363658;
        }

        /* ════════════════════════════════════════════════════════
           右侧面板 — 歌曲信息
           ════════════════════════════════════════════════════════ */
        #songTitle {
            font-size: 20px;
            font-weight: bold;
            color: #f0f0f0;
        }
        #songArtist {
            font-size: 14px;
            color: #999;
        }

        /* ════════════════════════════════════════════════════════
           歌词
           ════════════════════════════════════════════════════════ */
        #lyricsWidget {
            font-size: 14px;
            color: #666;
            background: transparent;
            border: none;
            padding: 4px 0;
        }
        #lyricsWidget::item {
            padding: 6px 20px;
            border: none;
            border-radius: 4px;
        }
        #lyricsWidget::item:selected {
            background: transparent;
            color: #e34d4d;
        }

        #albumArt {
            font-size: 64px;
            background-color: #232340;
            border-radius: 16px;
        }

        #lyricsPlayBtn {
            background-color: #e34d4d;
            color: #fff;
            font-size: 14px;
            border: none;
            border-radius: 18px;
        }
        #lyricsPlayBtn:hover {
            background-color: #ff6b6b;
        }

        #lyricsOffsetBtn {
            background-color: #2a2a48;
            color: #ccc;
            font-size: 14px;
            font-weight: bold;
            border: 1px solid #363658;
            border-radius: 12px;
        }
        #lyricsOffsetBtn:hover {
            background-color: #e34d4d;
            border-color: #e34d4d;
            color: #fff;
        }
        #lyricsOffsetLabel {
            font-size: 12px;
            color: #999;
            min-width: 40px;
        }

        /* ════════════════════════════════════════════════════════
           底部控制栏
           ════════════════════════════════════════════════════════ */
        #controlBar {
            background-color: #2a2a48;
            border-top: 1px solid #363658;
        }

        /* ── 进度条 ── */
        #progressSlider::groove:horizontal {
            height: 4px;
            background: #363658;
            border-radius: 2px;
        }
        #progressSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -5px 0;
            background: #e34d4d;
            border-radius: 7px;
        }
        #progressSlider::handle:horizontal:hover {
            background: #ff6b6b;
            width: 16px;
            height: 16px;
            margin: -6px 0;
        }
        #progressSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #e34d4d, stop:1 #ff6b6b);
            border-radius: 2px;
        }
        #progressSlider {
            background: transparent;
        }

        /* ── 音量条 ── */
        #volumeSlider::groove:horizontal {
            height: 3px;
            background: #363658;
            border-radius: 2px;
        }
        #volumeSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -4px 0;
            background: #e34d4d;
            border-radius: 6px;
        }
        #volumeSlider::handle:horizontal:hover {
            background: #ff6b6b;
        }
        #volumeSlider::sub-page:horizontal {
            background: #e34d4d;
            border-radius: 2px;
        }
        #volumeSlider {
            background: transparent;
        }

        #timeLabel {
            font-size: 12px;
            color: #999;
            min-width: 36px;
        }

        /* ── 控制按钮 ── */
        #ctrlBtn {
            background: transparent;
            border: none;
            font-size: 18px;
            color: #ccc;
            border-radius: 18px;
            padding: 4px;
        }
        #ctrlBtn:hover {
            background: rgba(227,77,77,0.15);
            color: #fff;
        }
        #ctrlBtn:checked {
            color: #e34d4d;
        }
        #ctrlBtn:pressed {
            background: rgba(227,77,77,0.25);
        }

        #playBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #e34d4d, stop:1 #ff6b6b);
            border: none;
            font-size: 20px;
            color: #fff;
            border-radius: 22px;
        }
        #playBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #ff5252, stop:1 #ff8a8a);
        }
        #playBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #c0392b, stop:1 #e34d4d);
        }

        #volumeLabel {
            font-size: 12px;
            color: #999;
        }

        /* ════════════════════════════════════════════════════════
           状态栏 / 分割器 / 滚动条
           ════════════════════════════════════════════════════════ */
        QStatusBar {
            background-color: #232340;
            color: #666;
            border-top: 1px solid #363658;
            font-size: 12px;
        }

        QSplitter::handle {
            background: #363658;
            width: 1px;
        }

        QScrollBar:vertical {
            width: 6px;
            background: transparent;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #363658;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #e34d4d;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: none;
        }

        QScrollBar:horizontal {
            height: 6px;
            background: transparent;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #363658;
            border-radius: 3px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #e34d4d;
        }
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0;
        }
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: none;
        }
    )");
}
