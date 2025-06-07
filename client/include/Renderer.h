#ifndef RENDERER_H
#define RENDERER_H

#include <thread>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <map>
#include <filesystem>
#include <mutex>

#include "game.h"
#include "object.h"
#include "graphics/Camera.h"
#include "graphics/RenderManager.h"

constexpr int NUM_PIPELINES = 4;
constexpr size_t LOOKUP_TABLE_SIZE = 0x2000;  // Pas aan indien nodig
constexpr size_t FRAME_INFO_SIZE = 0x2000;    // Pas aan indien nodig

constexpr uint16_t SPRITE_WIDTH = 400; // Voorbeeld, pas aan naar jouw situatie
constexpr uint16_t SPRITE_HEIGHT = 400;
constexpr uint32_t SPRITE_DATA_BASE = 0x30000000; // Voorbeeld, pas aan naar jouw situatie

constexpr size_t MAX_FRAME_INFO_SIZE = 4096;

struct FrameInfo {
    int16_t x;
    int16_t y;
    uint32_t sprite_id;
    bool is_tile; // true if this is a tile sprite, false if it's an actor sprite
};

class Renderer {
public:
    Renderer(const std::filesystem::path& basePath, Camera* cam, bool devMode = false);
    ~Renderer();

    void initUIO();
    
    private:
    void drawScreen();
    void renderObjects(Game& game);
    void renderActors(Game& game);
    void handleIRQ();
    int loadSprite(const std::string& img_path, uint32_t* sprite_data, std::map<int, uint32_t>* spriteAddressMap, uint32_t* phys_addr_out);
    void loadAllSprites(const std::filesystem::path& basePath);
    void init_lookup_tables();
    void init_frame_infos();
    void distribute_sprites_over_pipelines();
    void irqHandlerThread();
    void fakeIRQHandlerThread();

    void* lookup_table_ptrs[NUM_PIPELINES] = {nullptr};
    void* frame_info_ptrs[NUM_PIPELINES] = {nullptr};
    volatile uint64_t* lookup_tables[NUM_PIPELINES] = {nullptr};
    volatile uint64_t* frame_infos[NUM_PIPELINES] = {nullptr};
    std::unordered_map<std::string, std::map<int, uint32_t>> spriteSheetMap;
    std::map<std::string, int> lookup_table_map;

    std::vector<FrameInfo> frame_info_data;
    std::mutex frame_info_mutex;

    Camera* camera_;
    RenderManager* renderManager_;

    bool devMode_ = false; // For development mode, where we just load an image and quit

    int uio_fd;

    std::thread irq_thread;
    volatile bool stop_thread = false;

    static constexpr uint32_t LOOKUP_TABLE_ADDRS[4] = {
        0x82000000, 0x86000000, 0x8A000000, 0x8E000000
    };

    static constexpr uint32_t FRAME_INFO_ADDRS[4] = {
        0x80000000, 0x84000000, 0x88000000, 0x8C000000
    };


    // Helpers die je zelf moet implementeren:
    void write_lookup_table_entry(volatile uint64_t* table, int index,
                                 uint32_t phys_addr, uint16_t width, uint16_t height);

    void write_sprite_to_frame_info(volatile uint64_t *frame_info_arr, int index, int16_t x, int16_t y, uint32_t sprite_id);
};

#endif // RENDERER_H
