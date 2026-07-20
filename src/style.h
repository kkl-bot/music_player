#ifndef STYLE_H
#define STYLE_H

#include <QString>

class Style
{
public:
    enum Theme {
        Dark    = 0,
        Light   = 1,
        Fleet   = 2,  // Fleet-Snowfluff
    };

    static QString styleSheet(Theme theme);
    static QString darkStyleSheet();
    static QString lightStyleSheet();
    static QString fleetSnowfluffStyleSheet();
};

#endif // STYLE_H
