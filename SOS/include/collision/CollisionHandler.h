#ifndef COLLISION_HANDLER_H
#define COLLISION_HANDLER_H

#include "CollisionVisitor.h"
#include "CollisionInfo.h"
#include "object.h"

class CollisionHandler : public CollisionVisitor {
private:
    Object* initiator;
    CollisionInfo info;

    void handleInteraction(Player* player);
    void handleInteraction(Enemy* enemy);
    void handleInteraction(Tile* platform);

public:
    CollisionHandler(Object* initiator, const CollisionInfo& info);

    void visit(Player* player) override;
    void visit(Enemy* enemy) override;
    void visit(Tile* platform) override;
};

#endif // COLLISION_HANDLER_H
