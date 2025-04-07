#ifndef INPUT_H
#define INPUT_H

#define DEFINE_GETTER_SETTER(type, member) \
private:                                   \
    type member;                           \
public:                                    \
    type& get##member() { return member; } \
    void set##member(type& value) { member = value; }


class Input {
public:
    virtual ~Input() = default;
    virtual void read() = 0;

protected:
    DEFINE_GETTER_SETTER(bool, position);
};

#endif // INPUT_H