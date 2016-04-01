#include <iostream>
#include <dof/oal.h>

#ifndef DEMO_SPARKFUNOLED_H
#define DEMO_SPARKFUNOLED_H

#include "eboled.h"
#include "gpio.h"

class SparkfunGamepad
{
public:
    SparkfunGamepad():  BUTTON_UP(47, INPUT), BUTTON_DOWN(44, INPUT), BUTTON_LEFT(165, INPUT),
                        BUTTON_RIGHT(45, INPUT), BUTTON_SELECT(48, INPUT), BUTTON_A(49, INPUT),
                        BUTTON_B(46, INPUT)
    {}

    enum SPARKFUN_BUTTON_STATE
    {
        SELECT = 0x01,
        B = 0x02,
        A = 0x04,
        RIGHT = 0x08,
        LEFT = 0x10,
        DOWN = 0x20,
        UP = 0x40
    };

    uint8 GetKeyState()
    {
        uint8 ret = 0;

        if(BUTTON_SELECT.pinRead() == LOW)
        {
            ret |= SELECT;
        }

        if(BUTTON_B.pinRead() == LOW)
        {
            ret |= B;
        }

        if(BUTTON_A.pinRead() == LOW)
        {
            ret |= A;
        }

        if(BUTTON_RIGHT.pinRead() == LOW)
        {
            ret |= RIGHT;
        }

        if(BUTTON_LEFT.pinRead() == LOW)
        {
            ret |= LEFT;
        }

        if(BUTTON_DOWN.pinRead() == LOW)
        {
            ret |= DOWN;
        }

        if(BUTTON_UP.pinRead() == LOW)
        {
            ret |= UP;
        }

        return ret;
    }

private:
    gpio BUTTON_UP;
    gpio BUTTON_DOWN;
    gpio BUTTON_LEFT;
    gpio BUTTON_RIGHT;
    gpio BUTTON_SELECT;
    gpio BUTTON_A;
    gpio BUTTON_B;
};

class SparkFunOled
{
public:
    SparkFunOled(): lcd(new upm::EBOLED())
    {
        console.reserve(6);
        lcd->clearScreenBuffer();
        //lcd->setTextWrap(true);
    }

    ~SparkFunOled()
    {
        delete lcd;
    }

    void clear()
    {
        console.clear();
        update();
    }

    void Display(const std::string& message)
    {
        if(console.size() >= 6)
            console.erase(console.begin());
        console.push_back(message);
        update();
    }

private:
    void update()
    {
        lcd->clearScreenBuffer();
        for(std::size_t i = 0; i < console.size(); ++i)
        {
            lcd->setCursor(i*font_height, 0);
            lcd->write(console[i]);
        }
        lcd->refresh();
    }

    static const uint8 cols = 10;
    static const uint8 rows = 6;
    static const uint8 font_height = 8;

    std::vector<std::string> console;
    upm::EBOLED* lcd;
};

#endif //DEMO_SPARKFUNOLED_H
