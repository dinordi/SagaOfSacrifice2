#ifndef COLLISION_VISITOR_H
#define COLLISION_VISITOR_H

class Player;
class Enemy;
class Platform;
class RemotePlayer;

class CollisionVisitor {
public:
    virtual ~CollisionVisitor() = default;
    virtual void visit(Player* player) = 0;
    virtual void visit(Enemy* enemy) = 0;
    virtual void visit(Platform* platform) = 0;
    virtual void visit(RemotePlayer* remotePlayer) = 0;
};

#endif // COLLISION_VISITOR_H
