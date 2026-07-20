#ifndef STYLE_H
#define STYLE_H

#include <QString>

class Style
{
public:
    enum Theme {
        Dark  = 0,
        Light = 1,
    };

    static QString styleSheet(Theme theme);
    static QString darkStyleSheet();
    static QString lightStyleSheet();
};

#endif // STYLE_H
