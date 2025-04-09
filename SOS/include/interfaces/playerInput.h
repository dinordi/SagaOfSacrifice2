#ifndef INPUT_H
#define INPUT_H

#define PLAYER_VAR(type, member) \
private:                                   \
    type member;                           \
    type last_##member;                    \
public:                                    \
    type& get_##member() { return member; } \
    void set_##member(const type& value) { member = value; } \
    void set_last_##member(const type& value) { last_##member = value; } \
    type get_last_##member() { return last_##member; } \


class PlayerInput {
public:
    virtual ~PlayerInput() = default;
    virtual void readInput() = 0;

protected:
    PLAYER_VAR(bool, jump);
    PLAYER_VAR(bool, left);
    PLAYER_VAR(bool, right);
    PLAYER_VAR(bool, attack);
};

#endif // INPUT_H