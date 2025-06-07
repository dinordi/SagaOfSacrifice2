#include <filesystem>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdexcept>
#include <cstring>

#include "Renderer.h"
#include "fpga/spriteloader.h"

#include "sprite_data.h"

Renderer::Renderer(const std::filesystem::path& basePath, Camera* cam, bool devMode)
    : uio_fd(-1), camera_(cam), devMode_(devMode)
{
    if(devMode_) {
        std::cout << "Development mode enabled. Running headless." << std::endl;
        return; // In dev mode, we don't initialize UIO or load sprites
    }
    else
    {
        init_frame_infos();
        loadAllSprites(basePath);
        init_lookup_tables();
    }
}

int Renderer::loadSprite(const std::string& img_path, uint32_t* sprite_data, std::map<int, uint32_t>* spriteAddressMap, uint32_t* phys_addr_out) {
    SpriteLoader spriteLoader;

    int width = 0, height = 0;
    size_t sprite_size = 0;

    // Remove "/spriteatlas/<name>.tpsheet" at the end of the string
    std::string png_path = img_path;
    size_t pos = png_path.rfind("/spriteatlas/");
    size_t ext_pos = png_path.rfind(".tpsheet");
    if (pos != std::string::npos && ext_pos != std::string::npos && ext_pos > pos) {
        png_path.erase(pos);
    }
    SpriteData* spData = SpriteData::getSharedInstance(img_path);
    std::string pngName = spData->getSpriteRect(0).id_;
    
    png_path += "/sprites/" + pngName + ".png"; // Assuming the PNG file is named like the sprite rect ID

    const char* png_file = png_path.c_str();
    
    
    
    std::cout << "Mapping " << spData->getSpriteRects().size() << " sprites from " << png_file << std::endl;
    for(const auto& pair : spData->getSpriteRects()) {
        const SpriteRect& rect = pair.second;
        sprite_size = rect.w * rect.h * sizeof(uint32_t);
        if (spriteLoader.load_png_spritesheet(png_file, sprite_data, rect.w, rect.h, rect.x, rect.y) != 0) {
            std::cerr << "Failed to load PNG file: " << png_file << std::endl;
            return -1;
        }
        // if(spriteLoader.load_png(png_file, sprite_data, &width, &height, &sprite_size) != 0) {
        //     std::cerr << "Failed to load PNG file: " << png_file << std::endl;
        //     return -1;
        // }
        (*spriteAddressMap)[rect.count] = *phys_addr_out;
        if (spriteLoader.map_sprite_to_memory(png_file, phys_addr_out, sprite_data, sprite_size) != 0) {
            std::cerr << "Failed to map sprite to memory: " << png_file << std::endl;
            return -2;
        }
    }


    std::cout << "Sprite '" << img_path << "' mapped to address: 0x"
              << std::hex << *phys_addr_out << std::dec << std::endl;
    return 0;
}

void Renderer::loadAllSprites(const std::filesystem::path& basePath) {

    uint32_t* sprite_data = new uint32_t[MAX_WIDTH * MAX_HEIGHT];
    uint32_t phys_addr = SPRITE_DATA_BASE; // Start address for sprite data
    
    for (const auto& entry : std::filesystem::directory_iterator(basePath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".tpsheet") {
            std::map<int, uint32_t> spriteAddressMap;
            std::filesystem::path fullPath = entry.path();
            std::string fileStem = fullPath.stem().string();  // "player" uit "player.png"
            int result = loadSprite(fullPath.string(), sprite_data, &spriteAddressMap, &phys_addr);
                
            if (result == 0) {
                spriteSheetMap[fileStem] = spriteAddressMap;
            } else {
                std::cerr << "Failed to load sprite: " << fullPath << std::endl;
            }
        }
    }

    delete[] sprite_data;
}

void Renderer::initUIO() {
    if(devMode_)
    {
        irq_thread = std::thread(&Renderer::fakeIRQHandlerThread, this);
        return;
    }
    uio_fd = open("/dev/uio0", O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
        throw std::runtime_error("Failed to open UIO device");
    }

    uint32_t clear_value = 1;
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear pending interrupt");
        close(uio_fd);
        throw std::runtime_error("Failed to clear pending interrupt");
    }

    std::cout << "Starting thread to handle interrupts..." << std::endl;
    irq_thread = std::thread(&Renderer::irqHandlerThread, this);
    if (!irq_thread.joinable()) {
        perror("Failed to create IRQ handler thread");
        close(uio_fd);
        throw std::runtime_error("Failed to create IRQ handler thread");
    }

    std::cout << "IRQ handler thread started." << std::endl;
}

Renderer::~Renderer()
{
    // Signal thread to stop
    stop_thread = true;
    
    // Wait for thread to finish first
   if (irq_thread.joinable()) {
       irq_thread.join();
   }

    // NOW it's safe to close the fd
    if (uio_fd >= 0) {
        close(uio_fd);
        uio_fd = -1;
    }

    
    // Clean up memory mappings
    for (int i = 0; i < NUM_PIPELINES; i++) {
        if (frame_info_ptrs[i] != nullptr && frame_info_ptrs[i] != MAP_FAILED) {
            munmap(frame_info_ptrs[i], FRAME_INFO_SIZE);
            frame_info_ptrs[i] = nullptr;
        }
    }
    
    std::cout << "Renderer cleanup completed" << std::endl;
}

void Renderer::handleIRQ()
{
    uint32_t irq_count;
    uint32_t clear_value = 1;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }
    // Clear the interrupt flag by writing to the UIO device
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear interrupt");
    }
    
    // Only proceed if game instance is available and running
    Game& game = Game::getInstance();
    if (game.isRunning()) {
        distribute_sprites_over_pipelines();
        drawScreen();
    }
    
    //std::cout << "Interrupt received! IRQ count: " << irq_count << std::endl;
    //update_and_write_animated_sprite(1);

}

void Renderer::irqHandlerThread()
{
    struct pollfd fds;
    fds.fd = uio_fd;
    fds.events = POLLIN;
    stop_thread = 0;
    std::cout << "IRQ thread running, waiting for interrupts on fd: " << uio_fd << " " << stop_thread << std::endl;

    while (!stop_thread) {
        if (uio_fd >= 0)
        {
            int ret = poll(&fds, 1, 1000); // 1 second timeout instead of -1
            
            if (ret > 0 && (fds.revents & POLLIN)) {
                // std::cout << "Interrupt detected!" << std::endl;
                handleIRQ();
            } else if (ret < 0) {
                if (!stop_thread) {  // Only log if not shutting down
                    perror("poll");
                }
                break;
            }
            // ret == 0 means timeout, check stop_thread and continue
        }
    }
    
    std::cout << "IRQ thread exiting" << std::endl;
}

void Renderer::fakeIRQHandlerThread()
{
    stop_thread = 0;
    while (!stop_thread) {
        // Simulate an interrupt every 100 milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // Only proceed if game instance is available and running
        Game& game = Game::getInstance();
        if (game.isRunning()) {
            drawScreen();
        }
    }
    
    std::cout << "Fake IRQ thread exiting" << std::endl;
}

void Renderer::init_lookup_tables() 
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        throw std::runtime_error("Cannot open /dev/mem");
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        lookup_table_ptrs[i] = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, LOOKUP_TABLE_ADDRS[i]);
        if (lookup_table_ptrs[i] == MAP_FAILED) {
            perror("mmap lookup table");
            throw std::runtime_error("mmap failed");
        }

        lookup_tables[i] = (volatile uint64_t *)(lookup_table_ptrs[i]);
    }
        
    int index = 0;
    for(auto& map : spriteSheetMap)  // map.first is the sprite sheet name, map.second is a map of sprite index to address, i.e. minotaurus_idle -> 8 sprites, with addresses
    {
        lookup_table_map[map.first] = index; // Store the index for this sprite sheet
        for (const auto& sprite : map.second) {// index of sprite in spritesheet, mapped to start address, i.e. index:0 -> 0x32d23000
            // 0-1023, baseAddr, width, height
            SpriteData* spData = SpriteData::getSharedInstance(map.first);
            uint16_t sprite_width = spData->getSpriteRect(sprite.first).w;
            uint16_t sprite_height = spData->getSpriteRect(sprite.first).h;
            uint32_t sprite_data_base = sprite.second; // The physical address of the sprite data
            for(int i = 0; i < NUM_PIPELINES; i++) {
                write_lookup_table_entry(lookup_tables[i], index, sprite_data_base, sprite_width, sprite_height);
                //write_lookup_table_entry(lookup_tables[i], index, SPRITE_DATA_BASE, 400, 400);
            }
            index++;
        }
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        munmap(lookup_table_ptrs[i], LOOKUP_TABLE_SIZE);
    }
    close(fd);
}

void Renderer::init_frame_infos() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        throw std::runtime_error("Cannot open /dev/mem");
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        frame_info_ptrs[i] = mmap(NULL, FRAME_INFO_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd, FRAME_INFO_ADDRS[i]);
        if (frame_info_ptrs[i] == MAP_FAILED) {
            perror("mmap frame info");
            throw std::runtime_error("mmap failed");
        }

        frame_infos[i] = (volatile uint64_t *)(frame_info_ptrs[i]);
        
        // Initialize all indices 0-1023 with end marker for all pipelines
        for (int j = 0; j <= 1023; j++) {
            frame_infos[i][j] = 0xFFFFFFFFFFFFFFFF;
        }
    }

    close(fd);
}

void Renderer::distribute_sprites_over_pipelines() {

    // Arrays to track how many sprites are in each pipeline
    int sprites_in_pipeline[NUM_PIPELINES] = {0};
    
    int index = 0;
    for(auto frame : frame_info_data)
    {
        if (frame_info_data.size() > MAX_FRAME_INFO_SIZE) {
            std::cerr << "Frame info data size exceeded maximum limit!" << std::endl;
            return; // Or handle the error as needed
        }
        
        // Distribute sprites across pipelines using round-robin
        int pipeline = index % NUM_PIPELINES;
        int pipeline_index = sprites_in_pipeline[pipeline];
        
        // Write the sprite to the determined pipeline
        write_sprite_to_frame_info(frame_infos[pipeline], pipeline_index, frame.x, frame.y, frame.sprite_id);
        sprites_in_pipeline[pipeline]++;
        index++;
    }

    // Add end markers for all pipelines
    for (int i = 0; i < NUM_PIPELINES; i++) {
        frame_infos[i][sprites_in_pipeline[i]] = 0xFFFFFFFFFFFFFFFF;
    }

    // const uint16_t X_START = 130;
    // const uint16_t Y_START = 50;

    // // Place just one sprite in the first pipeline at X_START, Y_START
    // int pipeline = 0; // Use the first pipeline
    // static int sprite_id = 0; // Use sprite ID 1

    // static int counter = 0;
    // counter++;
    // if (counter == 5)
    // {
    //     sprite_id++;
    //     counter = 0;
    // }
    // if (sprite_id > 800) {
    //     sprite_id = 0; // Reset to a lower sprite ID
    // }
    
    // // Write the single sprite to the frame info
    // write_sprite_to_frame_info(frame_infos[pipeline], 0, X_START, Y_START, sprite_id);
    
    // // Add end marker after the sprite
    // frame_infos[pipeline][1] = 0xFFFFFFFFFFFFFFFF;
}

void Renderer::drawScreen()
{
    frame_info_data.clear(); // Clear previous frame info data

    Game& game = Game::getInstance();
   
    renderObjects(game);
    renderActors(game);
}

void Renderer::renderObjects(Game& game)
{
    std::lock_guard<std::mutex> lock(game.getObjectsMutex());
    std::lock_guard<std::mutex> lock2(game.getSpatialGridMutex());

    // Get camera viewport for culling
    float cameraX = camera_->getPosition().x;
    float cameraY = camera_->getPosition().y;
    float viewportWidth = camera_->getWidth();
    float viewportHeight = camera_->getHeight();
    
    // Calculate visible bounds with some padding for smooth scrolling
    float padding = 200.0f; // Extra pixels around viewport
    float minX = cameraX - padding;
    float maxX = cameraX + viewportWidth + padding;
    float minY = cameraY - padding;
    float maxY = cameraY + viewportHeight + padding;
    
    // Ask spatial grid which objects are relevant
    std::vector<Object*> visibleObjects;
    if (game.getSpatialGrid()) {
        visibleObjects = game.getSpatialGrid()->getObjectsInRegion(minX, minY, maxX, maxY);
    } else {
        // fallback: all objects
        const std::vector<std::shared_ptr<Object>>& allObjects = game.getObjects();
        for (const auto& obj : allObjects) {
            if (obj) visibleObjects.push_back(obj.get());
        }
    }
    
    // Count objects processed for performance monitoring
    static int frameCount = 0;
    static int totalObjectsChecked = 0;
    static int objectsRendered = 0;
    
    int currentFrameChecked = 0;
    int currentFrameRendered = 0;
    
    for (Object* entity : visibleObjects) {
        if (!entity) continue;
        currentFrameChecked++;
        
        const SpriteData* spriteData = entity->getCurrentSpriteData();

        if(entity->type == ObjectType::TILE ){
            if(entity->getLayer() > 1) {
                continue; // Skip rendering if no animation state is set
            }
        }

        if (!spriteData) continue; // Basic safety check
        
        // Use the current sprite index from animation system
        int spriteIndex = entity->getCurrentSpriteIndex();
        const SpriteRect& spriteRect = spriteData->getSpriteRect(spriteIndex);

        // Final viewport culling check with actual sprite dimensions
        BoxCollider collider = entity->getcollider();
        float entityLeft = collider.position.x - (spriteRect.w / 2);
        float entityTop = collider.position.y - (spriteRect.h / 2);
        float entityWidth = spriteRect.w;
        float entityHeight = spriteRect.h;
        
        if (!camera_->isVisible(entityLeft, entityTop, entityWidth, entityHeight)) {
            continue; // Skip rendering this object
        }
        
        currentFrameRendered++;
        
        // Get Sprite Address
        std::map<int, uint32_t>spriterects = spriteSheetMap[spriteRect.id_];
        int first_index_of_map = lookup_table_map[spriteRect.id_];
        int index = first_index_of_map + spriteRect.count;
        
        // Convert world coordinates to screen coordinates using the camera
        Vec2 screenPos = camera_->worldToScreen(entityLeft, entityTop);

        if(frame_info_data.size() > MAX_FRAME_INFO_SIZE) {
            std::cerr << "Frame info data size exceeded maximum limit!" << std::endl;
            return; // Or handle the error as needed
        }
        frame_info_data.push_back({
            .x = static_cast<int16_t>(screenPos.x),
            .y = static_cast<int16_t>(screenPos.y),
            .sprite_id = static_cast<uint32_t>(index),
            .is_tile = (entity->type == ObjectType::TILE)
        });
    }

    // Performance logging every 5 seconds
    totalObjectsChecked += currentFrameChecked;
    objectsRendered += currentFrameRendered;
    
    if (++frameCount % 300 == 0) {
        float avgChecked = totalObjectsChecked / 300.0f;
        float avgRendered = objectsRendered / 300.0f;
        float cullRatio = avgChecked > 0 ? (avgChecked - avgRendered) / avgChecked * 100.0f : 0.0f;
        
        std::cout << "[FPGA Renderer] Performance - Avg objects checked: " << avgChecked 
                  << ", rendered: " << avgRendered << ", culled: " << cullRatio << "%" << std::endl;
        
        totalObjectsChecked = 0;
        objectsRendered = 0;
    }
}

void Renderer::renderActors(Game& game)
{
    
    
    std::lock_guard<std::mutex> lock(game.getActorsMutex());
    const std::vector<Actor*>& actors = game.getActors();

    for(const auto& actor : game.getActors()) {
        if (!actor) continue; // Basic safety check

        const SpriteData* spriteData = actor->getCurrentSpriteData();
        
        ActorType actorType = actor->gettype();
        Vec2 actorPos = actor->getposition();

        
        if(actorType == ActorType::HEALTHBAR) {
            std::vector<SpriteRect> spriteRects = static_cast<Healthbar*>(actor)->getSpriteRects();
            int count = 0;
            int maxCount = spriteRects.size();
            std::vector<float> offsets = static_cast<Healthbar*>(actor)->getOffsets(maxCount); // Get personal offsets
            for(int count = 0; count < maxCount; count++)
            {
                auto rect = spriteRects[count];
                
                
                if(!camera_->isVisible(actorPos.x, actorPos.y, rect.w, rect.h)) // Check if actor is visible
                {
                    continue; // Skip rendering this actor if not visible
                }

                // Use personal offset for this specific count/part
                float centerOffset = 0.0f;
                if (count < offsets.size()) {
                    centerOffset = offsets[count];
                } else {
                    // Fallback to old calculation if offset not provided
                    if (count == 2 || count == maxCount - 1) {
                        centerOffset = 0.0f;
                    } else {
                        centerOffset = (count - 2) * rect.w;
                    }
                }
                float x = actorPos.x + centerOffset;

                // Get Sprite Address
                std::map<int, uint32_t>spriterects = spriteSheetMap[rect.id_];
                int first_index_of_map = lookup_table_map[rect.id_];
                // Needed index
                int index = first_index_of_map + rect.count; // Get the index of the sprite in the lookup table

                Vec2 screenPos = camera_->worldToScreen(
                    x - (rect.w / 2),
                    actorPos.y - (rect.h / 2)
                );

                if(frame_info_data.size() > MAX_FRAME_INFO_SIZE) {
                    std::cerr << "Frame info data size exceeded maximum limit!" << std::endl;
                    return; // Or handle the error as needed
                }
                frame_info_data.push_back({
                    .x = static_cast<int16_t>(screenPos.x),
                    .y = static_cast<int16_t>(screenPos.y),
                    .sprite_id =  static_cast<uint32_t>(index), // Use the index as sprite ID
                    .is_tile = 0 // Indicate if this is a tile
                });
                
            }
        }
        else    //Render text
        {
    
            // Get the specific source rect for this character index
            const SpriteRect& charSpriteRect = spriteData->getSpriteRect(actor->getdefaultIndex());
            
            if(!camera_->isVisible(actorPos.x, actorPos.y, charSpriteRect.w, charSpriteRect.h)) // Check if actor is visible
            {
                continue; // Skip rendering this actor if not visible
            }

            // Note: For UI elements like actors, we might want to keep them in screen space
            // rather than applying the camera transform. This depends on whether they're
            // part of the HUD or part of the game world.
            
            // Convert world coordinates to screen coordinates using the camera
            Vec2 screenPos = camera_->worldToScreen(
                actor->getposition().x,
                actor->getposition().y
            );

            // Get Sprite Address
            std::map<int, uint32_t>spriterects = spriteSheetMap[charSpriteRect.id_];
            int first_index_of_map = lookup_table_map[charSpriteRect.id_];
            // Needed index
            int index = first_index_of_map + charSpriteRect.count; // Get the index of the sprite in the lookup table
        
            if(frame_info_data.size() > MAX_FRAME_INFO_SIZE) {
                std::cerr << "Frame info data size exceeded maximum limit!" << std::endl;
                return; // Or handle the error as needed
            }
            frame_info_data.push_back({
                .x = static_cast<int16_t>(screenPos.x),
                .y = static_cast<int16_t>(screenPos.y),
                .sprite_id = static_cast<uint32_t>(index),
                .is_tile = 0 // Use the sprite rect count as sprite ID
            });
        }
    }
}

// void Renderer::distribute_sprites_over_pipelines() {
//     const int TOTAL_STATIC_SPRITES = 15;
//     const uint16_t SPRITE_WIDTH = 400;
//     const uint16_t SPRITE_HEIGHT = 400;
//     const uint16_t X_START = 133;
//     const uint16_t Y_START = 50;
//     const uint16_t X_MAX = 2050 - SPRITE_WIDTH;
//     const uint16_t Y_MAX = 1080 - SPRITE_HEIGHT;
 
//     int sprites_in_pipeline[NUM_PIPELINES] = {0};
//     uint16_t x = X_START;
//     uint16_t y = Y_START;
 
//     for (int sprite_idx = 0; sprite_idx < TOTAL_STATIC_SPRITES; sprite_idx++) {
//         int pipeline = sprite_idx % NUM_PIPELINES;
//         int index_in_pipeline = sprites_in_pipeline[pipeline];
 
//         write_sprite_to_frame_info(frame_infos[pipeline], index_in_pipeline, x, y, 1);
//         sprites_in_pipeline[pipeline]++;
 
//         x += SPRITE_WIDTH;
//         if (x > X_MAX) {
//             x = X_START;
//             y += SPRITE_HEIGHT;
//             if (y > Y_MAX) {
//                 y = Y_START;
//             }
//         }
//     }
 
//     for (int i = 0; i < NUM_PIPELINES; i++) {
//         frame_infos[i][sprites_in_pipeline[i]] = 0xFFFFFFFFFFFFFFFF;
//     }
// }

// Lookup table entry schrijven
void Renderer::write_lookup_table_entry(volatile uint64_t *lookup_table, int index, uint32_t sprite_data_base, uint16_t width, uint16_t height) {
    if (lookup_table == nullptr) {
        printf("Error: lookup_table pointer is NULL\n");
        return; // Of: throw std::runtime_error("lookup_table pointer is NULL");
    }

    // Bounds checking
    if (index < 0 || index > 1023) {
        printf("Error: index %d out of bounds (0-1023)\n", index);
        return;
    }
    
    if (width > 1921) {
        printf("Error: width %u out of bounds (0-1921)\n", width);
        return;
    }
    
    if (height > 1081) {
        printf("Error: height %u out of bounds (0-1080)\n", height);
        return;
    }

    uint64_t value = ((uint64_t)sprite_data_base << 23) |
                     ((uint64_t)height << 12) |
                     width;

    lookup_table[index] = value;

    // printf("Lookup Table [%d]:\n", index);
    // printf("  Base Addr = 0x%08X\n", sprite_data_base);
    // printf("  Width     = %u\n", width);
    // printf("  Height    = %u\n", height);
    // printf("  Value(hex)= 0x%016llX\n", value);
}

// ─────────────────────────────────────────────
void Renderer::write_sprite_to_frame_info(volatile uint64_t *frame_info_arr, int index, int16_t x, int16_t y, uint32_t sprite_id) {
    if (frame_info_arr == nullptr) {
        printf("Error: frame_info_arr pointer is NULL\n");
        return; // Of: throw std::runtime_error("frame_info_arr pointer is NULL");
    }

    // Bounds checking
    if (index < 0 || index > 1023) {
        printf("Error: index %d out of bounds (0-1023)\n", index);
        return;
    }
    
    
    if (x < -2047 || x > 2047) {
        printf("Error: x %d out of bounds (-2047 to 2047)\n", x);
        return;
    }
    
    if (y < -1080 || y > 1080) {
        printf("Error: y %d out of bounds (-1080 to 1080)\n", y);
        return;
    }
    
    if (sprite_id > 1023) {
        printf("Error: sprite_id %u out of bounds (0-1023)\n", sprite_id);
        return;
    }

    uint64_t masked_y = ((uint64_t)y) & 0xFFF;
    uint64_t base_value = ((uint64_t)x << 23) | (masked_y << 11) | sprite_id;
    frame_info_arr[index] = base_value;

    //printf("Frame info [%d]: X=%u, Y=%u, ID=%u\n", index, x, y, sprite_id);
    //printf("  Value (hex): 0x%016llX\n", base_value);
}

