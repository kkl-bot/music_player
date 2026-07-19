#ifndef STYLE_H
#define STYLE_H

#include <QString>

/// Style - 界面主题管理
///
/// 所有样式表集中在此管理，MainWindow 仅调用接口。
/// 后续可扩展多主题（如浅色主题、高对比主题等）。
class Style
{
public:
    /// 主题枚举（预留扩展）
    enum Theme {
        Dark,   // 深色主题（默认）
        // Light,    // 浅色主题（待实现）
    };

    /// 返回指定主题的完整样式表
    static QString styleSheet(Theme theme = Dark);

    // ── 主题色常量（方便取色引用） ──
    struct Colors {
        static constexpr auto bg         = "#1b1b2f";   // 主背景
        static constexpr auto panel      = "#232340";   // 面板背景
        static constexpr auto bar        = "#2a2a48";   // 控制栏
        static constexpr auto border     = "#363658";   // 边框
        static constexpr auto accent     = "#e34d4d";   // 强调色
        static constexpr auto accentOver = "#ff6b6b";   // 强调悬停
        static constexpr auto text       = "#f0f0f0";   // 主文字
        static constexpr auto textSec    = "#999999";   // 次要文字
        static constexpr auto textDim    = "#666666";   // 弱文字
    };

private:
    Style() = default;  // 工具类，禁止实例化

    static QString darkStyleSheet();
};

#endif // STYLE_H
