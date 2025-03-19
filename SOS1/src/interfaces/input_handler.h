#ifndef INPUT_H
#define INPUT_H

class InputHandler {
public:
    virtual ~InputHandler() = default;
    virtual void read() = 0;

    bool isUp() const { return up; }
    bool isDown() const { return down; }
    bool isLeft() const { return left; }
    bool isRight() const { return right; }
    bool isAction() const { return action; }
    bool isJump() const { return jump; }
    bool isDash() const { return dash; }
    bool isReset() const { return reset; }
    bool isStart() const { return start; }

//Protected, so they can be accessed by derived classes
protected:
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool action = false;
    bool jump = false;
    bool dash = false;
    bool reset = false;
    bool start = false;
};

#endif // INPUT_H