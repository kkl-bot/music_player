#include "style.h"

QString Style::styleSheet(Theme theme)
{
    switch (theme) {
    case Light:
        return lightStyleSheet();
    default:
        return darkStyleSheet();
    }
}

QString Style::darkStyleSheet()
{
    return QStringLiteral(R"(
        /*************************************************
         *  全局基础
         *************************************************/
        QWidget {
            background-color: #121212;
            color: #eeeeee;
            font-family: "Segoe UI","Microsoft YaHei","PingFang SC",sans-serif;
            font-size: 14px;
        }
        QMainWindow {
            background-color: #121212;
        }

        /*************************************************
         *  菜单栏
         *************************************************/
        QMenuBar {
            background: #181818;
            color: #dddddd;
            border-bottom: 1px solid #333;
        }
        QMenuBar::item {
            padding: 6px 15px;
        }
        QMenuBar::item:selected {
            background: #333333;
        }
        QMenu {
            background: #202020;
            border: 1px solid #444;
        }
        QMenu::item {
            padding: 8px 25px;
        }
        QMenu::item:selected {
            background: #e53935;
        }

        /*************************************************
         *  左侧播放列表（Fleet-Snowfluff 风格）
         *************************************************/
        #leftPanel {
            background: #151515;
            border-right: 1px solid #333;
        }
        #searchBox {
            background: #222;
            border: 1px solid #444;
            border-radius: 18px;
            padding: 8px 15px;
            color: white;
        }
        #searchBox:focus {
            border: 1px solid #e53935;
        }
        #searchBox::placeholder {
            color: #888;
        }
        #countLabel {
            color: #aaaaaa;
            padding: 8px;
        }
        QListWidget {
            background: #171717;
            border: none;
            outline: none;
            font-size: 14px;
        }
        QListWidget::item {
            padding: 10px 16px;
            border-radius: 8px;
            border-left: 3px solid transparent;
            margin: 2px 6px;
        }
        QListWidget::item:hover {
            background: #2a2a2a;
            border-left-color: rgba(255,183,197,0.3);
        }
        QListWidget::item:selected {
            background: #3a2020;
            color: #ff5252;
            border-left: 3px solid #ff5252;
        }

        /*************************************************
         *  右侧歌曲信息
         *************************************************/
        #songTitle {
            font-size: 26px;
            font-weight: bold;
            color: white;
        }
        #songArtist {
            font-size: 16px;
            color: #aaaaaa;
        }
        #albumArt {
            font-size: 64px;
            border-radius: 15px;
            background: #222;
            border: 1px solid #444;
        }

        /*************************************************
         *  歌词区域
         *************************************************/
        #lyricsWidget {
            background: #111111;
            border: none;
            padding: 20px;
            font-size: 16px;
        }
        #lyricsWidget::item {
            height: 35px;
            color: #888888;
        }
        #lyricsWidget::item:selected {
            color: #ff3333;
            font-weight: bold;
        }
        #lyricsPlayBtn {
            background: #e53935;
            color: #fff;
            font-size: 14px;
            border: none;
            border-radius: 18px;
        }
        #lyricsPlayBtn:hover {
            background: #ff5252;
        }

        /*************************************************
         *  歌词时间微调
         *************************************************/
        #lyricsAdjustBtn {
            background: #252525;
            border-radius: 15px;
            padding: 6px 14px;
            font-size: 14px;
            font-weight: bold;
        }
        #lyricsAdjustBtn:hover {
            background: #e53935;
        }
        #lyricsOffsetLabel {
            font-size: 12px;
            color: #aaaaaa;
            min-width: 40px;
        }

        /*************************************************
         *  底部控制栏（胶囊风格）
         *************************************************/
        #controlBar {
            background: #181818;
            border-top: 1px solid #333;
        }
        #ctrlBtn {
            background: transparent;
            border: none;
            border-radius: 16px;
            min-width: 32px;
            min-height: 32px;
            font-size: 16px;
            color: #cccccc;
        }
        #ctrlBtn:hover {
            background: rgba(255,255,255,0.1);
            color: #fff;
        }
        #ctrlBtn:checked {
            color: #ff5252;
        }
        #ctrlBtn:pressed {
            background: rgba(255,255,255,0.15);
        }
        #playBtn {
            min-width: 40px;
            min-height: 40px;
            border-radius: 20px;
            color: white;
            font-size: 18px;
            border: none;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #ff5252, stop:1 #d50000);
        }
        #playBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #ff7777, stop:1 #ff1744);
        }
        #playBtn:pressed {
            background: #b71c1c;
        }
        /* 旋转唱片 */
        #albumDisc {
            background: #222;
            border: 2px solid rgba(255,255,255,0.15);
            border-radius: 18px;
            font-size: 18px;
        }
        /* 滚动歌名 */
        #songMarquee {
            font-size: 13px;
            color: #e0e6ff;
            min-width: 60px;
            font-family: "Segoe UI","Microsoft YaHei",sans-serif;
            letter-spacing: 0;
            padding: 0 2px;
        }
        #volumeLabel {
            font-size: 12px;
            color: #bbbbbb;
        }

        /*************************************************
         *  播放进度条
         *************************************************/
        #progressSlider::groove:horizontal {
            height: 6px;
            background: #444;
            border-radius: 3px;
        }
        #progressSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #ff5252, stop:1 #e53935);
            border-radius: 3px;
        }
        #progressSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -5px 0;
            background: #ffffff;
            border-radius: 7px;
        }
        #progressSlider::handle:horizontal:hover {
            background: #ff5252;
        }
        #progressSlider {
            background: transparent;
        }

        /*************************************************
         *  音量控制
         *************************************************/
        #volumeSlider::groove:horizontal {
            height: 5px;
            background: #555;
            border-radius: 2px;
        }
        #volumeSlider::sub-page:horizontal {
            background: #ff5252;
            border-radius: 2px;
        }
        #volumeSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -4px 0;
            background: #ffffff;
            border-radius: 6px;
        }
        #volumeSlider::handle:horizontal:hover {
            background: #ff5252;
        }
        #volumeSlider {
            background: transparent;
        }

        /*************************************************
         *  时间显示
         *************************************************/
        #timeLabel {
            color: #bbbbbb;
            font-size: 13px;
        }

        /*************************************************
         *  通用按钮
         *************************************************/
        QPushButton {
            background: #242424;
            border: none;
            border-radius: 8px;
            padding: 8px 14px;
            color: #eeeeee;
        }
        QPushButton:hover {
            background: #333333;
        }
        QPushButton:pressed {
            background: #444444;
        }
        QPushButton:disabled {
            background: #333;
            color: #666;
        }

        /*************************************************
         *  ComboBox
         *************************************************/
        QComboBox {
            background: #222;
            border: 1px solid #444;
            border-radius: 8px;
            padding: 6px 12px;
            color: #eee;
        }
        QComboBox:hover {
            border-color: #e53935;
        }
        QComboBox:focus {
            border-color: #e53935;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #444;
        }
        QComboBox QAbstractItemView {
            background: #222;
            color: #eee;
            border: 1px solid #444;
            selection-background-color: #e53935;
            selection-color: #fff;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            padding: 5px 10px;
        }
        QComboBox QAbstractItemView::item:hover {
            background: #333;
        }

        /*************************************************
         *  Dialog / ConversionDialog
         *************************************************/
        QDialog {
            background: #161616;
        }
        #ConversionDialog QLabel {
            color: #ddd;
        }
        #ConversionDialog QProgressBar {
            height: 12px;
            border: none;
            background: #333;
            border-radius: 6px;
            text-align: center;
            color: #eee;
            font-size: 12px;
            font-weight: bold;
        }
        #ConversionDialog QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #ff5252, stop:1 #d50000);
            border-radius: 6px;
        }

        /*************************************************
         *  Tooltip
         *************************************************/
        QToolTip {
            background: #222;
            color: white;
            border: 1px solid #555;
            padding: 5px;
        }

        /*************************************************
         *  状态栏 / 分割器 / 滚动条
         *************************************************/
        QStatusBar {
            background: #181818;
            color: #888;
            border-top: 1px solid #333;
            font-size: 12px;
        }
        QSplitter::handle {
            background: #333;
            width: 1px;
        }
        QScrollBar:vertical {
            width: 6px;
            background: transparent;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #444;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #e53935;
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
            background: #444;
            border-radius: 3px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #e53935;
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

QString Style::lightStyleSheet()
{
    return QStringLiteral(R"(
        /*************************************************
         *  全局基础
         *************************************************/
        QWidget {
            background-color: #f5f5f5;
            color: #333333;
            font-family: "Segoe UI","Microsoft YaHei","PingFang SC",sans-serif;
            font-size: 14px;
        }
        QMainWindow {
            background-color: #f5f5f5;
        }

        /*************************************************
         *  菜单栏
         *************************************************/
        QMenuBar {
            background: #ffffff;
            color: #555555;
            border-bottom: 1px solid #ddd;
        }
        QMenuBar::item {
            padding: 6px 15px;
        }
        QMenuBar::item:selected {
            background: #e8e8e8;
        }
        QMenu {
            background: #ffffff;
            border: 1px solid #ccc;
        }
        QMenu::item {
            padding: 8px 25px;
        }
        QMenu::item:selected {
            background: #e53935;
            color: #fff;
        }

        /*************************************************
         *  左侧播放列表
         *************************************************/
        #leftPanel {
            background: #ffffff;
            border-right: 1px solid #ddd;
        }
        #searchBox {
            background: #e8e8e8;
            border: 1px solid #ccc;
            border-radius: 18px;
            padding: 8px 15px;
            color: #333;
        }
        #searchBox:focus {
            border: 1px solid #e53935;
        }
        #searchBox::placeholder {
            color: #aaa;
        }
        #countLabel {
            color: #888888;
            padding: 8px;
        }
        QListWidget {
            background: #fafafa;
            border: none;
            outline: none;
        }
        QListWidget::item {
            height: 42px;
            padding-left: 15px;
            border-radius: 8px;
        }
        QListWidget::item:hover {
            background: #e8e8e8;
        }
        QListWidget::item:selected {
            background: #fce4e4;
            color: #e53935;
        }

        /*************************************************
         *  右侧歌曲信息
         *************************************************/
        #songTitle {
            font-size: 26px;
            font-weight: bold;
            color: #222222;
        }
        #songArtist {
            font-size: 16px;
            color: #888888;
        }
        #albumArt {
            font-size: 64px;
            border-radius: 15px;
            background: #ffffff;
            border: 1px solid #ddd;
        }

        /*************************************************
         *  歌词区域
         *************************************************/
        #lyricsWidget {
            background: #f0f0f0;
            border: none;
            padding: 20px;
            font-size: 16px;
        }
        #lyricsWidget::item {
            height: 35px;
            color: #999999;
        }
        #lyricsWidget::item:selected {
            color: #e53935;
            font-weight: bold;
        }
        #lyricsPlayBtn {
            background: #e53935;
            color: #fff;
            font-size: 14px;
            border: none;
            border-radius: 18px;
        }
        #lyricsPlayBtn:hover {
            background: #ff5252;
        }

        /*************************************************
         *  歌词时间微调
         *************************************************/
        #lyricsAdjustBtn {
            background: #e0e0e0;
            border-radius: 15px;
            padding: 6px 14px;
            font-size: 14px;
            font-weight: bold;
            color: #555;
        }
        #lyricsAdjustBtn:hover {
            background: #e53935;
            color: #fff;
        }
        #lyricsOffsetLabel {
            font-size: 12px;
            color: #888888;
            min-width: 40px;
        }

        /*************************************************
         *  底部控制栏（胶囊风格）
         *************************************************/
        #controlBar {
            background: #ffffff;
            border-top: 1px solid #ddd;
        }
        #ctrlBtn {
            background: transparent;
            border: none;
            border-radius: 16px;
            min-width: 32px;
            min-height: 32px;
            font-size: 16px;
            color: #555555;
        }
        #ctrlBtn:hover {
            background: rgba(0,0,0,0.08);
            color: #333;
        }
        #ctrlBtn:checked {
            color: #e53935;
        }
        #ctrlBtn:pressed {
            background: rgba(0,0,0,0.12);
        }
        #playBtn {
            min-width: 40px;
            min-height: 40px;
            border-radius: 20px;
            color: white;
            font-size: 18px;
            border: none;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #ff5252, stop:1 #d50000);
        }
        #playBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #ff7777, stop:1 #ff1744);
        }
        #playBtn:pressed {
            background: #b71c1c;
        }
        /* 旋转唱片 */
        #albumDisc {
            background: #eee;
            border: 2px solid rgba(0,0,0,0.15);
            border-radius: 18px;
            font-size: 18px;
        }
        /* 滚动歌名 */
        #songMarquee {
            font-size: 13px;
            color: #333;
            min-width: 60px;
            font-family: "Segoe UI","Microsoft YaHei",sans-serif;
            letter-spacing: 0;
            padding: 0 2px;
        }
        #volumeLabel {
            font-size: 12px;
            color: #888888;
        }

        /*************************************************
         *  播放进度条
         *************************************************/
        #progressSlider::groove:horizontal {
            height: 6px;
            background: #d0d0d0;
            border-radius: 3px;
        }
        #progressSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #ff5252, stop:1 #e53935);
            border-radius: 3px;
        }
        #progressSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -5px 0;
            background: #ffffff;
            border: 2px solid #e53935;
            border-radius: 7px;
        }
        #progressSlider::handle:horizontal:hover {
            background: #e53935;
        }
        #progressSlider {
            background: transparent;
        }

        /*************************************************
         *  音量控制
         *************************************************/
        #volumeSlider::groove:horizontal {
            height: 5px;
            background: #d0d0d0;
            border-radius: 2px;
        }
        #volumeSlider::sub-page:horizontal {
            background: #e53935;
            border-radius: 2px;
        }
        #volumeSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -4px 0;
            background: #ffffff;
            border: 2px solid #e53935;
            border-radius: 6px;
        }
        #volumeSlider::handle:horizontal:hover {
            background: #e53935;
        }
        #volumeSlider {
            background: transparent;
        }

        /*************************************************
         *  时间显示
         *************************************************/
        #timeLabel {
            color: #888888;
            font-size: 13px;
        }

        /*************************************************
         *  通用按钮
         *************************************************/
        QPushButton {
            background: #e0e0e0;
            border: none;
            border-radius: 8px;
            padding: 8px 14px;
            color: #333333;
        }
        QPushButton:hover {
            background: #cccccc;
        }
        QPushButton:pressed {
            background: #bbbbbb;
        }
        QPushButton:disabled {
            background: #f0f0f0;
            color: #aaa;
        }

        /*************************************************
         *  ComboBox
         *************************************************/
        QComboBox {
            background: #ffffff;
            border: 1px solid #ccc;
            border-radius: 8px;
            padding: 6px 12px;
            color: #333;
        }
        QComboBox:hover {
            border-color: #e53935;
        }
        QComboBox:focus {
            border-color: #e53935;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #ddd;
        }
        QComboBox QAbstractItemView {
            background: #ffffff;
            color: #333;
            border: 1px solid #ccc;
            selection-background-color: #e53935;
            selection-color: #fff;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            padding: 5px 10px;
        }
        QComboBox QAbstractItemView::item:hover {
            background: #f0f0f0;
        }

        /*************************************************
         *  Dialog / ConversionDialog
         *************************************************/
        QDialog {
            background: #f0f0f0;
        }
        #ConversionDialog QLabel {
            color: #444;
        }
        #ConversionDialog QProgressBar {
            height: 12px;
            border: none;
            background: #ddd;
            border-radius: 6px;
            text-align: center;
            color: #555;
            font-size: 12px;
            font-weight: bold;
        }
        #ConversionDialog QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #ff5252, stop:1 #d50000);
            border-radius: 6px;
        }

        /*************************************************
         *  Tooltip
         *************************************************/
        QToolTip {
            background: #444;
            color: white;
            border: 1px solid #888;
            padding: 5px;
        }

        /*************************************************
         *  状态栏 / 分割器 / 滚动条
         *************************************************/
        QStatusBar {
            background: #ffffff;
            color: #888;
            border-top: 1px solid #ddd;
            font-size: 12px;
        }
        QSplitter::handle {
            background: #ddd;
            width: 1px;
        }
        QScrollBar:vertical {
            width: 6px;
            background: transparent;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #ccc;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #e53935;
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
            background: #ccc;
            border-radius: 3px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #e53935;
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
