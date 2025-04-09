#ifndef COLLISION_HANDLER_H
#define COLLISION_HANDLER_H

#include "CollisionVisitor.h"
#include "CollisionInfo.h"
#include "Object.h"

class CollisionHandler : public CollisionVisitor {
private:
    Object* initiator;
    CollisionInfo info;

    void handleInteraction(Player* player);
    void handleInteraction(Enemy* enemy);
    void handleInteraction(Platform* platform);

public:
    CollisionHandler(Object* initiator, const CollisionInfo& info);

    void visit(Player* player) override;
    void visit(Enemy* enemy) override;
    void visit(Platform* platform) override;
};

#endif // COLLISION_HANDLER_H
