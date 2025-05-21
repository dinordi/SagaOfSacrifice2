#pragma once

#include <memory>
#include <string>
#include "objects/player.h"
#include "Vec2.h"
#include "sprite_data.h"
#include "playerInput.h"
// Temporary input handler for players
class TempInput : public PlayerInput {
public:
    void readInput() override {
        // No-op: we drive this by network messages or external code
    }
    
    void setInputs(bool u, bool d, bool l, bool r, bool a) {
        set_up(u);
        set_down(d);
        set_left(l);
        set_right(r);
        set_attack(a);
    }
};

/**
 * Factory class for creating player objects consistently across the game
 */
class PlayerFactory {
public:
    static std::shared_ptr<Player> createPlayer(
        const std::string& playerId,
        const Vec2& position = Vec2{500, 100}
    ) {
        // Create sprite data
        auto sprite = new SpriteData("playermap", 128, 128, /*spriteId=*/5);
        // Create player
        auto player = std::make_shared<Player>(position, sprite, playerId);

        // Initialize player input
        auto* input = new TempInput();
        input->setInputs(false, false, false, false, false);
        player->setInput(input);

        return player;
    }
    
    /**
     * Create a player with custom sprite settings
     */
    static std::shared_ptr<Player> createPlayerWithSprite(
        const std::string& playerId,
        const std::string& spriteSheet,
        int width, int height,
        int spriteId,
        const Vec2& position = Vec2{500, 100}
    ) {
        // Create sprite data with the given sheet & frame
        auto sprite = new SpriteData(spriteSheet, width, height, spriteId);
        // Create player
        auto player = std::make_shared<Player>(position, sprite, playerId);

        // Initialize player input (all false by default)
        auto* input = new TempInput();
        input->setInputs(false, false, false, false, false);
        player->setInput(input);

        return player;
    }
};

