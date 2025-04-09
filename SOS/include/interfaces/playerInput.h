#ifndef INPUT_H
#define INPUT_H

#define DEFINE_GETTER_SETTER(type, member) \
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
    virtual void read() = 0;

protected:
    DEFINE_GETTER_SETTER(bool, jump);
};

#endif // INPUT_H