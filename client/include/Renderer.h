#ifndef RENDERER_H
#define RENDERER_H

#include <thread>
#include <cstdint>

constexpr int NUM_PIPELINES = 4;
constexpr size_t LOOKUP_TABLE_SIZE = 0x2000;  // Pas aan indien nodig
constexpr size_t FRAME_INFO_SIZE = 0x2000;    // Pas aan indien nodig

constexpr uint16_t SPRITE_WIDTH = 400;
constexpr uint16_t SPRITE_HEIGHT = 400;
constexpr uint32_t SPRITE_DATA_BASE = 0x0E000000; // Voorbeeld, pas aan naar jouw situatie

class Renderer {
public:
    Renderer(const std::string& img_path);
    ~Renderer();

    void handleIRQ();

private:
    void loadSprite(const std::string& img_path);
    void init_lookup_tables();
    void init_frame_infos();
    void distribute_sprites_over_pipelines();
    void irqHandlerThread();
    void initUIO();

    void* lookup_table_ptrs[NUM_PIPELINES] = {nullptr};
    void* frame_info_ptrs[NUM_PIPELINES] = {nullptr};
    volatile uint64_t* lookup_tables[NUM_PIPELINES] = {nullptr};
    volatile uint64_t* frame_infos[NUM_PIPELINES] = {nullptr};

    int uio_fd;

    std::thread irq_thread;
    volatile bool stop_thread;

    static constexpr uint32_t LOOKUP_TABLE_ADDRS[NUM_PIPELINES] = {
        0x40000000, 0x46000000, 0x80002000, 0x80006000
    };

    static constexpr uint32_t FRAME_INFO_ADDRS[NUM_PIPELINES] = {
        0x42000000, 0x44000000, 0x80000000, 0x80004000
    };


    // Helpers die je zelf moet implementeren:
    void write_lookup_table_entry(volatile uint64_t* table, int index,
                                 uint32_t phys_addr, uint16_t width, uint16_t height);

    void write_sprite_to_frame_info(volatile uint64_t *frame_info_arr, int index, uint16_t x, uint16_t y, uint32_t sprite_id);
};

#endif // RENDERER_H
