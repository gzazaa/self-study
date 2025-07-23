#include <iostream>
#include <cstddef>
#include <stdexcept>
#include <vector>

// Memory block control structure
struct MemoryBlock {
    size_t size;            // Total block size (including control structure)
    bool is_free;           // Free status flag
    MemoryBlock* prev;      // Previous block (physical adjacency)
    MemoryBlock* next;      // Next block (physical adjacency)
    void* data_ptr;         // Pointer to data area
    
    // Initialize memory block
    void initialize(size_t block_size, bool free, MemoryBlock* prev_block, MemoryBlock* next_block) {
        size = block_size;
        is_free = free;
        prev = prev_block;
        next = next_block;
        data_ptr = reinterpret_cast<char*>(this) + sizeof(MemoryBlock);
    }
    
    // Merge with next free block
    void merge_next() {
        if (next && next->is_free) {
            size += next->size;
            next = next->next;
            if (next) {
                next->prev = this;
            }
        }
    }
};

// Memory pool class
class MemoryPool {
private:
    MemoryBlock* head;          // Memory block list head
    char* pool_start;           // Start of memory buffer
    char* pool_end;             // Current end of allocated blocks
    size_t pool_total_size;     // Total size of memory buffer
    
    // Create new block at current allocation point
    MemoryBlock* create_block(size_t size, MemoryBlock* prev, MemoryBlock* next) {
        // Check if we exceed total buffer size
        if (reinterpret_cast<size_t>(pool_end + size) > 
            reinterpret_cast<size_t>(pool_start + pool_total_size)) {
            throw std::bad_alloc();
        }
        
        MemoryBlock* block = reinterpret_cast<MemoryBlock*>(pool_end);
        block->initialize(size, true, prev, next);
        pool_end += size;
        return block;
    }
    
    // Split block if remaining space is sufficient
    void split_block(MemoryBlock* block, size_t requested_size) {
        size_t remaining = block->size - requested_size;
        // Ensure remaining space can hold a new block
        if (remaining >= sizeof(MemoryBlock) + 8) {  // Minimum usable space
            // Create new block in remaining space
            MemoryBlock* new_block = reinterpret_cast<MemoryBlock*>(
                reinterpret_cast<char*>(block) + requested_size);
            
            // Initialize new block
            new_block->initialize(remaining, true, block, block->next);
            
            // Update linked list
            if (block->next) {
                block->next->prev = new_block;
            }
            block->next = new_block;
            block->size = requested_size;
        }
    }

public:
    // Constructor - uses preallocated buffer
    MemoryPool(void* buffer, size_t size) 
        : head(nullptr), pool_total_size(size) {
        pool_start = static_cast<char*>(buffer);
        pool_end = pool_start;
        
        // Create initial block covering entire buffer
        head = create_block(size, nullptr, nullptr);
    }
    
    // Allocate memory
    void* allocate(size_t size) {
        if (size == 0) return nullptr;
        
        // Apply 8-byte alignment
        size_t aligned_size = (size + 7) & ~7;
        size_t total_size = aligned_size + sizeof(MemoryBlock);
        
        // First-fit allocation strategy
        MemoryBlock* block = head;
        MemoryBlock* best_fit = nullptr;
        
        // Find best fitting block
        while (block) {
            if (block->is_free && block->size >= total_size) {
                if (!best_fit || block->size < best_fit->size) {
                    best_fit = block;
                }
            }
            block = block->next;
        }
        
        if (!best_fit) {
            // Try merging free blocks
            coalesce();
            
            // Retry search
            block = head;
            while (block) {
                if (block->is_free && block->size >= total_size) {
                    if (!best_fit || block->size < best_fit->size) {
                        best_fit = block;
                    }
                }
                block = block->next;
            }
            
            if (!best_fit) {
                throw std::bad_alloc();
            }
        }
        
        // Split block if possible
        split_block(best_fit, total_size);
        
        // Mark as allocated
        best_fit->is_free = false;
        
        return best_fit->data_ptr;
    }
    
    // Release memory
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        // Get block header from data pointer
        MemoryBlock* block = reinterpret_cast<MemoryBlock*>(
            reinterpret_cast<char*>(ptr) - sizeof(MemoryBlock));
        
        // Mark as free
        block->is_free = true;
        
        // Merge adjacent free blocks
        coalesce();
    }
    
    // Merge adjacent free blocks
    void coalesce() {
        MemoryBlock* block = head;
        while (block) {
            if (block->is_free && block->next && block->next->is_free) {
                // Merge next block into current
                block->size += block->next->size;
                block->next = block->next->next;
                if (block->next) {
                    block->next->prev = block;
                }
                // Continue merging in case of multiple free blocks
            } else {
                block = block->next;
            }
        }
    }
    
    // Diagnostic information
    void print_diagnostics() const {
        std::cout << "==== Memory Pool Diagnostics ====\n";
        std::cout << "Total pool size: " << pool_total_size << " bytes\n";
        
        size_t total_used = 0;
        size_t user_used = 0;
        size_t user_free = 0;
        size_t overhead = 0;
        int blocks_used = 0;
        int blocks_free = 0;
        
        MemoryBlock* block = head;
        while (block) {
            size_t user_size = block->size - sizeof(MemoryBlock);
            overhead += sizeof(MemoryBlock);
            
            if (block->is_free) {
                user_free += user_size;
                blocks_free++;
            } else {
                user_used += user_size;
                blocks_used++;
            }
            total_used += block->size;
            
            block = block->next;
        }
        
        std::cout << "Used by allocations: " << user_used << " bytes (" 
                  << blocks_used << " blocks)\n";
        std::cout << "Available free memory: " << user_free << " bytes (" 
                  << blocks_free << " blocks)\n";
        std::cout << "Management overhead: " << overhead << " bytes\n";
        std::cout << "Unallocated/fragmented: " 
                  << pool_total_size - total_used << " bytes\n";
        
        // Calculate fragmentation
        size_t max_free_block = 0;
        block = head;
        while (block) {
            if (block->is_free) {
                size_t user_size = block->size - sizeof(MemoryBlock);
                if (user_size > max_free_block) {
                    max_free_block = user_size;
                }
            }
            block = block->next;
        }
        
        double fragmentation = 0.0;
        if (user_free > 0) {
            fragmentation = (1.0 - static_cast<double>(max_free_block) / user_free) * 100.0;
        }
        std::cout << "Fragmentation: " << fragmentation << "%\n";
        std::cout << "Largest free block: " << max_free_block << " bytes\n";
        std::cout << "================================\n";
    }
};

// Test cases
int main() {
    try {
        // Create 1MB static buffer
        const size_t POOL_SIZE = 1024 * 1024; // 1MB
        static char buffer[POOL_SIZE]; 
        
        // Initialize memory pool
        MemoryPool pool(buffer, POOL_SIZE);
        
        std::cout << "Memory pool created (1 MB)\n";
        pool.print_diagnostics();
        
        // Allocation tests
        std::vector<void*> allocations;
        
        std::cout << "\nAllocating memory blocks...\n";
        allocations.push_back(pool.allocate(128));    // 128 bytes
        allocations.push_back(pool.allocate(256));    // 256 bytes
        allocations.push_back(pool.allocate(512));    // 512 bytes
        allocations.push_back(pool.allocate(1024));   // 1 KB
        allocations.push_back(pool.allocate(2048));   // 2 KB
        
        pool.print_diagnostics();
        
        // Partial deallocation
        std::cout << "\nReleasing some allocations...\n";
        pool.deallocate(allocations[1]);  // Release 256-byte block
        pool.deallocate(allocations[3]);  // Release 1KB block
        allocations[1] = nullptr;
        allocations[3] = nullptr;
        
        pool.print_diagnostics();
        
        // Additional allocations
        std::cout << "\nAllocating more blocks...\n";
        allocations.push_back(pool.allocate(1500));   // 1.5 KB
        allocations.push_back(pool.allocate(800));    // 800 bytes
        allocations.push_back(pool.allocate(3000));   // 3 KB
        
        pool.print_diagnostics();
        
        // Cleanup all allocations
        std::cout << "\nReleasing all allocations...\n";
        for (void* ptr : allocations) {
            if (ptr) pool.deallocate(ptr);
        }
        allocations.clear();
        
        pool.print_diagnostics();
        
        // Large allocation test (should fail)
        std::cout << "\nTesting large allocation (should fail)...\n";
        try {
            // Try to allocate more than available
            void* huge = pool.allocate(POOL_SIZE - sizeof(MemoryBlock) + 1);
            std::cout << "ERROR: Large allocation succeeded unexpectedly!\n";
            pool.deallocate(huge);
        } catch (const std::bad_alloc& e) {
            std::cout << "Expected exception caught: " << e.what() << "\n";
        }
        
        // Valid large allocation test
        std::cout << "\nTesting maximum possible allocation...\n";
        try {
            // Should succeed
            void* huge = pool.allocate(POOL_SIZE - sizeof(MemoryBlock));
            std::cout << "Large allocation succeeded as expected\n";
            pool.deallocate(huge);
        } catch (const std::bad_alloc& e) {
            std::cout << "ERROR: " << e.what() << "\n";
        }
        
        pool.print_diagnostics();
        
        std::cout << "\nAll tests completed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
